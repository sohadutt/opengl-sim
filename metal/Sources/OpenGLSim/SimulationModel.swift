import Foundation
import simd

enum BufferIndex: Int {
    case meshVertices = 0
    case uniforms = 2
}

struct Uniforms {
    var viewProjection = matrix_identity_float4x4
    var model = matrix_identity_float4x4
    var positionRadius = SIMD4<Float>(repeating: 0)
    var colorMass = SIMD4<Float>(repeating: 0)
    var sceneParameters = SIMD4<Float>(repeating: 0)
    var counts = SIMD4<UInt32>(repeating: 0)
    var lensParameters = SIMD4<Float>(repeating: 0)
}

enum SimulationMode: Int32 {
    case selection = 0
    case celestial = 1
    case atomic = 2
}

struct Planet: Identifiable {
    let id = UUID()
    var position: SIMD3<Double>
    var velocity: SIMD3<Double> = .zero
    var radius: Double = 0.42
    var mass: Double = 0.04
    var color: SIMD3<Float> = SIMD3(0.18, 0.56, 0.92)
}

@MainActor
final class SimulationModel {
    var mode: SimulationMode = .selection
    var blackHolePosition = SIMD3<Double>(repeating: 0)
    var blackHoleVelocity = SIMD3<Double>(repeating: 0)
    var blackHoleMass = 1.0
    var eventHorizon = 0.24
    var spin = 0.5
    var planets: [Planet] = []
    var selectedPlanet = 0
    var showGrid = true
    var gridSpacing = 1.0
    var gridExtent = 10.0
    var gridOpacity = 0.62
    var particleCount = 100_000
    var particleSpeed = 1.0
    var particleSize = 3.0
    var trailsEnabled = false
    var electronCount = 1
    var protonCount = 1
    var neutronCount = 0
    var cameraYaw = Float(0)
    var cameraPitch = Float(0.45)
    var cameraDistance = Float(8.0)
    var time = 0.0

    init() {
        addPlanet(initial: true)
    }

    func addPlanet(initial: Bool = false) {
        guard planets.count < 32 else { return }
        let index = Double(planets.count)
        let angle = index * 2.399963229728653
        let radius = initial ? 2.2 : 2.2 + 0.35 * Double(planets.count % 6)
        var body = Planet(position: SIMD3(cos(angle) * radius, 0.12 * sin(angle), sin(angle) * radius))
        let speed = sqrt(SimulationConstants.scaledG * max(blackHoleMass, 0.05) / radius)
        body.velocity = SIMD3(-sin(angle) * speed, 0, cos(angle) * speed)
        if initial {
            body.position = SIMD3(0.35, 0.12, -2.2)
            body.velocity = SIMD3(0.37, 0, 0.06)
            body.radius = 0.52
            body.color = SIMD3(0.16, 0.58, 0.95)
        } else {
            body.color = SIMD3(Float.random(in: 0.25...0.95), Float.random(in: 0.25...0.95), Float.random(in: 0.25...0.95))
        }
        planets.append(body)
        selectedPlanet = planets.count - 1
    }

    func recolorSelectedPlanet() {
        guard planets.indices.contains(selectedPlanet) else { return }
        planets[selectedPlanet].color = SIMD3(Float.random(in: 0.25...0.95),
                                             Float.random(in: 0.25...0.95),
                                             Float.random(in: 0.25...0.95))
    }

    func resetBlackHole() {
        blackHolePosition = .zero
        blackHoleVelocity = .zero
        blackHoleMass = 1.0
        eventHorizon = 0.24
        spin = 0.5
    }

    func advance(_ delta: Double) {
        guard mode == .celestial, delta > 0, !planets.isEmpty else { return }
        let frame = min(delta, 0.1)
        let substeps = max(1, min(32, Int(ceil(frame / 0.004))))
        let step = frame / Double(substeps)
        let softening = 0.08
        for _ in 0..<substeps {
            var accelerations = Array(repeating: SIMD3<Double>(repeating: 0), count: planets.count)
            var holeAcceleration = SIMD3<Double>(repeating: 0)
            for i in planets.indices {
                let offset = blackHolePosition - planets[i].position
                let distance2 = simd_length_squared(offset) + softening * softening
                let invCube = 1 / (distance2 * sqrt(distance2))
                accelerations[i] += SimulationConstants.scaledG * blackHoleMass * offset * invCube
                holeAcceleration -= SimulationConstants.scaledG * planets[i].mass * offset * invCube
                if i + 1 < planets.count {
                    for j in (i + 1)..<planets.count {
                        let pair = planets[j].position - planets[i].position
                        let pair2 = simd_length_squared(pair) + softening * softening
                        let pairInv = 1 / (pair2 * sqrt(pair2))
                        accelerations[i] += SimulationConstants.scaledG * planets[j].mass * pair * pairInv
                        accelerations[j] -= SimulationConstants.scaledG * planets[i].mass * pair * pairInv
                    }
                }
            }
            blackHoleVelocity += holeAcceleration * step
            blackHolePosition += blackHoleVelocity * step
            for index in planets.indices {
                planets[index].velocity += accelerations[index] * step
                planets[index].position += planets[index].velocity * step
            }
            var remaining: [Planet] = []
            for body in planets {
                if simd_distance(body.position, blackHolePosition) <= max(eventHorizon, 0.01) {
                    let oldMass = blackHoleMass
                    blackHoleMass += body.mass
                    eventHorizon *= blackHoleMass / oldMass
                } else {
                    remaining.append(body)
                }
            }
            planets = remaining
            if planets.isEmpty { selectedPlanet = 0 }
            else { selectedPlanet = min(selectedPlanet, planets.count - 1) }
            time += step
        }
    }
}

enum SimulationConstants {
    static let G = 6.67430e-11
    static let c = 299_792_458.0
    static let scaledG = 0.32
    static let elementaryCharge = 1.602176634e-19
    static let electronMass = 9.1093837139e-31
    static let protonMass = 1.67262192595e-27
    static let neutronMass = 1.67492750056e-27
    static let bohrRadius = 5.29177210544e-11
}
