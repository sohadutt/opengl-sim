import Metal
import MetalKit
import simd

private let framesInFlight = 3
private let uniformStride = (MemoryLayout<Uniforms>.stride + 255) & ~255

private struct MeshVertex {
    var position: SIMD3<Float>
    var normal: SIMD3<Float>
}

@MainActor
final class Renderer: NSObject, MTKViewDelegate {
    let device: MTLDevice
    let model: SimulationModel
    private let queue: MTL4CommandQueue
    private let commandBuffer: MTL4CommandBuffer
    private let allocators: [MTL4CommandAllocator]
    private let event: MTLSharedEvent
    private let uniformsBuffer: MTLBuffer
    private let arguments: MTL4ArgumentTable
    private let residency: MTLResidencySet
    private let vertexBuffer: MTLBuffer
    private let indexBuffer: MTLBuffer
    private let indexCount: Int
    private let meshPipeline: MTLRenderPipelineState
    private let particlePipeline: MTLRenderPipelineState
    private let gridPipeline: MTLRenderPipelineState
    private let depthState: MTLDepthStencilState
    private var frame = 0
    private var frameSlot = 0
    private var drawableSize = CGSize(width: 1, height: 1)
    private var lastTime = CACurrentMediaTime()

    init?(view: MTKView, model: SimulationModel) {
        guard let device = view.device, device.supportsFamily(.metal4),
              let queue = device.makeMTL4CommandQueue(),
              let commandBuffer = device.makeCommandBuffer(),
              let event = device.makeSharedEvent() else { return nil }
        self.device = device
        self.model = model
        self.queue = queue
        self.commandBuffer = commandBuffer
        self.event = event
        allocators = (0..<framesInFlight).compactMap { _ in device.makeCommandAllocator() }
        guard allocators.count == framesInFlight,
              let uniforms = device.makeBuffer(length: uniformStride * framesInFlight * 48,
                                               options: .storageModeShared) else { return nil }
        uniformsBuffer = uniforms
        let tableDescription = MTL4ArgumentTableDescriptor()
        tableDescription.maxBufferBindCount = 3
        guard let arguments = try? device.makeArgumentTable(descriptor: tableDescription) else { return nil }
        self.arguments = arguments
        do {
            let geometry = Self.makeSphere()
            guard let vertices = device.makeBuffer(length: geometry.vertices.count * MemoryLayout<MeshVertex>.stride,
                                                   options: .storageModeShared),
                  let indices = device.makeBuffer(length: geometry.indices.count * MemoryLayout<UInt32>.stride,
                                                  options: .storageModeShared) else { return nil }
            geometry.vertices.withUnsafeBytes { source in
                if let base = source.baseAddress { vertices.contents().copyMemory(from: base, byteCount: source.count) }
            }
            geometry.indices.withUnsafeBytes { source in
                if let base = source.baseAddress { indices.contents().copyMemory(from: base, byteCount: source.count) }
            }
            vertexBuffer = vertices
            indexBuffer = indices
            indexCount = geometry.indices.count
            let descriptor = Self.vertexDescriptor()
            let library = try device.makeDefaultLibrary(bundle: Bundle.module)
            let compiler = try device.makeCompiler(descriptor: MTL4CompilerDescriptor())
            meshPipeline = try Self.pipeline(compiler: compiler, library: library, view: view,
                vertex: "meshVertex", fragment: "sceneFragment", vertexDescriptor: descriptor)
            particlePipeline = try Self.pipeline(compiler: compiler, library: library, view: view,
                vertex: "particleVertex", fragment: "particleFragment", vertexDescriptor: nil)
            gridPipeline = try Self.pipeline(compiler: compiler, library: library, view: view,
                vertex: "gridVertex", fragment: "gridFragment", vertexDescriptor: nil)
            let depthDescriptor = MTLDepthStencilDescriptor()
            depthDescriptor.depthCompareFunction = .less
            depthDescriptor.isDepthWriteEnabled = true
            guard let depth = device.makeDepthStencilState(descriptor: depthDescriptor) else { return nil }
            depthState = depth
            let setDescription = MTLResidencySetDescriptor()
            setDescription.initialCapacity = 3
            let set = try device.makeResidencySet(descriptor: setDescription)
            set.addAllocations([uniforms, vertices, indices])
            set.commit()
            residency = set
            queue.addResidencySet(set)
            if #available(macOS 26.4, *) { queue.addResidencySet(view.residencySet) }
        } catch {
            SimulatorLogger.shared.write(.error, "Metal 4 renderer setup failed: \(error)")
            return nil
        }
        event.signaledValue = 0
        super.init()
        SimulatorLogger.shared.write(.info, "Metal 4 renderer initialized.")
    }

    private static func pipeline(compiler: MTL4Compiler, library: MTLLibrary, view: MTKView,
                                 vertex: String, fragment: String,
                                 vertexDescriptor: MTLVertexDescriptor?) throws -> MTLRenderPipelineState {
        let vertexFunction = MTL4LibraryFunctionDescriptor()
        vertexFunction.library = library
        vertexFunction.name = vertex
        let fragmentFunction = MTL4LibraryFunctionDescriptor()
        fragmentFunction.library = library
        fragmentFunction.name = fragment
        let descriptor = MTL4RenderPipelineDescriptor()
        descriptor.label = "\(vertex) Pipeline"
        descriptor.rasterSampleCount = view.sampleCount
        descriptor.vertexFunctionDescriptor = vertexFunction
        descriptor.fragmentFunctionDescriptor = fragmentFunction
        descriptor.vertexDescriptor = vertexDescriptor
        descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
        if fragment == "gridFragment" || fragment == "particleFragment" {
            descriptor.colorAttachments[0].blendingState = .enabled
            descriptor.colorAttachments[0].rgbBlendOperation = .add
            descriptor.colorAttachments[0].sourceRGBBlendFactor = .sourceAlpha
            descriptor.colorAttachments[0].destinationRGBBlendFactor = .oneMinusSourceAlpha
            descriptor.colorAttachments[0].alphaBlendOperation = .add
            descriptor.colorAttachments[0].sourceAlphaBlendFactor = .one
            descriptor.colorAttachments[0].destinationAlphaBlendFactor = .oneMinusSourceAlpha
        }
        return try compiler.makeRenderPipelineState(descriptor: descriptor)
    }

    private static func vertexDescriptor() -> MTLVertexDescriptor {
        let descriptor = MTLVertexDescriptor()
        descriptor.attributes[0].format = .float3
        descriptor.attributes[0].offset = 0
        descriptor.attributes[0].bufferIndex = BufferIndex.meshVertices.rawValue
        descriptor.attributes[1].format = .float3
        descriptor.attributes[1].offset = MemoryLayout<SIMD3<Float>>.stride
        descriptor.attributes[1].bufferIndex = BufferIndex.meshVertices.rawValue
        descriptor.layouts[BufferIndex.meshVertices.rawValue].stride = MemoryLayout<MeshVertex>.stride
        descriptor.layouts[BufferIndex.meshVertices.rawValue].stepFunction = .perVertex
        return descriptor
    }

    private static func makeSphere() -> (vertices: [MeshVertex], indices: [UInt32]) {
        let rows = 32, columns = 48
        var vertices: [MeshVertex] = []
        var indices: [UInt32] = []
        for row in 0...rows {
            let v = Float(row) / Float(rows)
            let phi = v * .pi
            for column in 0...columns {
                let u = Float(column) / Float(columns)
                let theta = u * 2 * .pi
                let n = SIMD3<Float>(sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta))
                vertices.append(MeshVertex(position: n, normal: n))
            }
        }
        for row in 0..<rows {
            for column in 0..<columns {
                let a = UInt32(row * (columns + 1) + column)
                let b = a + UInt32(columns + 1)
                indices += [a, b, a + 1, a + 1, b, b + 1]
            }
        }
        return (vertices, indices)
    }

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) { drawableSize = size }

    func draw(in view: MTKView) {
        guard let drawable = view.currentDrawable,
              let pass = view.currentMTL4RenderPassDescriptor else { return }
        let now = CACurrentMediaTime()
        model.advance(now - lastTime)
        lastTime = now
        let slot = frame % framesInFlight
        let previousFrame = frame >= framesInFlight ? frame - framesInFlight + 1 : 0
        _ = event.wait(untilSignaledValue: UInt64(previousFrame), timeoutMS: 20)
        let allocator = allocators[slot]
        allocator.reset()
        commandBuffer.beginCommandBuffer(allocator: allocator)
        frameSlot = slot
        guard let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) else { return }
        drawOffset = 0
        encoder.setDepthStencilState(depthState)
        encoder.setCullMode(.back)
        encoder.setFrontFacing(.counterClockwise)

        let aspect = Float(max(drawableSize.width, 1) / max(drawableSize.height, 1))
        let viewProjection = cameraMatrix(aspect: aspect)
        if model.mode == .celestial {
            if model.showGrid { drawGrid(encoder, viewProjection: viewProjection) }
            drawParticles(encoder, viewProjection: viewProjection, mode: .celestial)
            drawSphere(encoder, viewProjection: viewProjection, position: model.blackHolePosition,
                       radius: model.eventHorizon * 1.3, color: SIMD4(0.006, 0.008, 0.014, 1))
            for planet in model.planets {
                drawSphere(encoder, viewProjection: viewProjection, position: planet.position,
                           radius: planet.radius, color: SIMD4(planet.color.x, planet.color.y, planet.color.z, 1))
            }
        } else if model.mode == .atomic {
            drawSphere(encoder, viewProjection: viewProjection, position: .zero, radius: 0.24,
                       color: SIMD4(0.96, 0.28, 0.12, 1))
            drawParticles(encoder, viewProjection: viewProjection, mode: .atomic)
        }
        encoder.endEncoding()
        commandBuffer.useResidencySet(residency)
        if #available(macOS 26.4, *) { commandBuffer.useResidencySet(view.residencySet) }
        commandBuffer.endCommandBuffer()
        queue.waitForDrawable(drawable)
        queue.commit([commandBuffer])
        queue.signalDrawable(drawable)
        queue.signalEvent(event, value: UInt64(frame + 1))
        frame += 1
        drawable.present()
    }

    private func cameraMatrix(aspect: Float) -> matrix_float4x4 {
        let yaw = model.cameraYaw, pitch = model.cameraPitch
        let distance = model.cameraDistance
        let eye = SIMD3<Float>(distance * cos(pitch) * sin(yaw), distance * sin(pitch), distance * cos(pitch) * cos(yaw))
        let forward = simd_normalize(-eye)
        let right = simd_normalize(simd_cross(forward, SIMD3(0, 1, 0)))
        let up = simd_cross(right, forward)
        let view = matrix_float4x4(columns: (
            SIMD4(right.x, up.x, -forward.x, 0), SIMD4(right.y, up.y, -forward.y, 0),
            SIMD4(right.z, up.z, -forward.z, 0),
            SIMD4(-simd_dot(right, eye), -simd_dot(up, eye), simd_dot(forward, eye), 1)))
        let fov: Float = 0.82
        let y = 1 / tan(fov / 2), x = y / max(aspect, 0.01)
        let near: Float = 0.08, far: Float = 100
        let projection = matrix_float4x4(columns: (
            SIMD4(x, 0, 0, 0), SIMD4(0, y, 0, 0),
            SIMD4(0, 0, far / (near - far), -1),
            SIMD4(0, 0, far * near / (near - far), 0)))
        return simd_mul(projection, view)
    }

    private func baseUniforms(viewProjection: matrix_float4x4, mode: SimulationMode) -> Uniforms {
        var value = Uniforms()
        value.viewProjection = viewProjection
        value.model = matrix_identity_float4x4
        value.positionRadius = SIMD4(Float(model.blackHolePosition.x), Float(model.blackHolePosition.y),
                                     Float(model.blackHolePosition.z), Float(model.gridExtent))
        value.colorMass = SIMD4(1, 1, 1, Float(model.blackHoleMass))
        value.sceneParameters = SIMD4(Float(model.time * model.particleSpeed), Float(model.spin),
                                      Float(model.eventHorizon), Float(model.gridSpacing))
        value.counts = SIMD4(UInt32(model.particleCount), UInt32(mode.rawValue),
                             UInt32(model.particleSize * 10), UInt32(model.electronCount))
        value.lensParameters = SIMD4(Float(model.blackHoleMass), Float(model.protonCount),
                                     Float(model.neutronCount), 0)
        return value
    }

    private func bind(_ uniforms: Uniforms) {
        let offset = uniformStride * (frameSlot * 48 + drawOffset)
        let pointer = uniformsBuffer.contents().advanced(by: offset).bindMemory(to: Uniforms.self, capacity: 1)
        pointer.pointee = uniforms
        arguments.setAddress(uniformsBuffer.gpuAddress + UInt64(offset), index: BufferIndex.uniforms.rawValue)
        drawOffset += 1
    }
    private var drawOffset = 0

    private func drawSphere(_ encoder: MTL4RenderCommandEncoder, viewProjection: matrix_float4x4,
                            position: SIMD3<Double>, radius: Double, color: SIMD4<Float>) {
        var uniforms = baseUniforms(viewProjection: viewProjection, mode: model.mode)
        uniforms.model = matrix_float4x4(columns: (
            SIMD4(Float(radius), 0, 0, 0), SIMD4(0, Float(radius), 0, 0), SIMD4(0, 0, Float(radius), 0),
            SIMD4(Float(position.x), Float(position.y), Float(position.z), 1)))
        uniforms.colorMass = color
        bind(uniforms)
        encoder.setRenderPipelineState(meshPipeline)
        encoder.setArgumentTable(arguments, stages: .vertex)
        arguments.setAddress(vertexBuffer.gpuAddress, index: BufferIndex.meshVertices.rawValue)
        encoder.drawIndexedPrimitives(primitiveType: .triangle, indexCount: indexCount, indexType: .uint32,
                                      indexBuffer: indexBuffer.gpuAddress, indexBufferLength: indexBuffer.length)
    }

    private func drawParticles(_ encoder: MTL4RenderCommandEncoder, viewProjection: matrix_float4x4,
                               mode: SimulationMode) {
        encoder.setRenderPipelineState(particlePipeline)
        encoder.setArgumentTable(arguments, stages: .vertex)
        let trailCount = mode == .celestial && model.trailsEnabled ? 4 : 1
        for trail in 0..<trailCount {
            var uniforms = baseUniforms(viewProjection: viewProjection, mode: mode)
            if mode == .atomic { uniforms.colorMass.w = Float(max(model.protonCount, 1)) }
            uniforms.lensParameters.w = Float(trail)
            bind(uniforms)
            encoder.drawPrimitives(primitiveType: .point, vertexStart: 0,
                                   vertexCount: mode == .atomic ? 30_000 : model.particleCount)
        }
    }

    private func drawGrid(_ encoder: MTL4RenderCommandEncoder, viewProjection: matrix_float4x4) {
        let uniforms = baseUniforms(viewProjection: viewProjection, mode: .celestial)
        var gridUniforms = uniforms
        gridUniforms.lensParameters.w = Float(model.gridOpacity)
        let lines = Int(2 * model.gridExtent / max(model.gridSpacing, 0.1)) + 1
        let count = min(lines * 96 * 2, 600_000)
        bind(gridUniforms)
        encoder.setRenderPipelineState(gridPipeline)
        encoder.setArgumentTable(arguments, stages: .vertex)
        encoder.drawPrimitives(primitiveType: .line, vertexStart: 0, vertexCount: count)
    }
}
