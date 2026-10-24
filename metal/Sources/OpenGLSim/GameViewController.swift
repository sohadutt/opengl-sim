import AppKit
import MetalKit

@MainActor
final class SimulationViewController: NSViewController {
    private let model = SimulationModel()
    private var metalView: SimulationMTKView!
    private var renderer: Renderer?
    private let sidebar = NSStackView()
    private var retainedTargets: [NSObject] = []
    private var panelTitle: NSTextField!

    override func loadView() {
        let split = NSSplitView()
        split.isVertical = true
        split.dividerStyle = .thin
        split.translatesAutoresizingMaskIntoConstraints = false
        let controlsScroll = NSScrollView()
        controlsScroll.hasVerticalScroller = true
        controlsScroll.drawsBackground = false
        sidebar.orientation = .vertical
        sidebar.alignment = .leading
        sidebar.spacing = 12
        sidebar.edgeInsets = NSEdgeInsets(top: 18, left: 16, bottom: 18, right: 16)
        sidebar.translatesAutoresizingMaskIntoConstraints = false
        controlsScroll.documentView = sidebar
        controlsScroll.widthAnchor.constraint(equalToConstant: 320).isActive = true
        let scene = SimulationMTKView()
        scene.translatesAutoresizingMaskIntoConstraints = false
        scene.colorPixelFormat = .bgra8Unorm_srgb
        scene.depthStencilPixelFormat = .depth32Float_stencil8
        scene.sampleCount = 1
        scene.clearColor = MTLClearColor(red: 0.009, green: 0.014, blue: 0.028, alpha: 1)
        scene.preferredFramesPerSecond = 60
        metalView = scene
        split.addArrangedSubview(controlsScroll)
        split.addArrangedSubview(scene)
        split.setPosition(320, ofDividerAt: 0)
        view = split
        rebuildControls()
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        guard let device = MTLCreateSystemDefaultDevice() else {
            showError("This Mac does not support Metal.")
            return
        }
        guard device.supportsFamily(.metal4) else {
            showError("This Mac requires a GPU that supports Metal 4.")
            return
        }
        metalView.device = device
        guard let renderer = Renderer(view: metalView, model: model) else {
            showError("The Metal 4 renderer could not be initialized. See logs/simulator.log.")
            return
        }
        self.renderer = renderer
        metalView.delegate = renderer
        metalView.onDrag = { [weak self] dx, dy in
            guard let self else { return }
            model.cameraYaw += Float(dx) * 0.008
            model.cameraPitch = min(max(model.cameraPitch + Float(dy) * 0.008, -1.35), 1.35)
        }
        metalView.onZoom = { [weak self] delta in
            guard let self else { return }
            model.cameraDistance = min(max(model.cameraDistance * Float(exp(-delta * 0.08)), 2.0), 40.0)
        }
        SimulatorLogger.shared.write(.info, "Metal scene ready. Drag to look, scroll to zoom.")
    }

    private func showError(_ message: String) {
        SimulatorLogger.shared.write(.critical, message)
        let label = NSTextField(wrappingLabelWithString: message)
        label.textColor = .secondaryLabelColor
        label.alignment = .center
        view = label
    }

    private func rebuildControls() {
        retainedTargets.removeAll()
        sidebar.arrangedSubviews.forEach { sidebar.removeArrangedSubview($0); $0.removeFromSuperview() }
        func heading(_ text: String) {
            let label = NSTextField(labelWithString: text)
            label.font = .boldSystemFont(ofSize: 13)
            label.textColor = .secondaryLabelColor
            sidebar.addArrangedSubview(label)
        }
        func button(_ title: String, _ action: @escaping () -> Void) {
            let target = ControlAction(action)
            retainedTargets.append(target)
            let control = NSButton(title: title, target: target, action: #selector(ControlAction.invoke))
            control.bezelStyle = .rounded
            control.widthAnchor.constraint(equalToConstant: 275).isActive = true
            sidebar.addArrangedSubview(control)
        }
        func slider(_ title: String, min minimum: Double, max maximum: Double,
                    value: Double, changed: @escaping (Double) -> Void) {
            let row = NSStackView()
            row.orientation = .vertical
            row.alignment = .leading
            row.spacing = 3
            let caption = NSTextField(labelWithString: "\(title)  \(value.formatted(.number.precision(.fractionLength(2))))")
            caption.font = .systemFont(ofSize: 11)
            let control = CallbackSlider(value: value, min: minimum, max: maximum)
            control.onChange = { value in
                caption.stringValue = "\(title)  \(value.formatted(.number.precision(.fractionLength(2))))"
                changed(value)
            }
            row.addArrangedSubview(caption)
            row.addArrangedSubview(control)
            control.widthAnchor.constraint(equalToConstant: 275).isActive = true
            sidebar.addArrangedSubview(row)
        }

        panelTitle = NSTextField(labelWithString: model.mode == .celestial ? "CELESTIAL SIMULATOR" : model.mode == .atomic ? "ATOMIC SIMULATOR" : "PHYSICS SIMULATOR")
        panelTitle.font = .boldSystemFont(ofSize: 18)
        sidebar.addArrangedSubview(panelTitle)
        if model.mode == .selection {
            heading("CHOOSE A SCENE")
            button("Celestial mode — black hole and planets") { [weak self] in self?.model.mode = .celestial; self?.rebuildControls() }
            button("Atomic mode — electron probability cloud") { [weak self] in self?.model.mode = .atomic; self?.rebuildControls() }
            return
        }
        button("Back to mode selection") { [weak self] in self?.model.mode = .selection; self?.rebuildControls() }
        if model.mode == .celestial {
            heading("BLACK HOLE")
            slider("Mass", min: 0.1, max: 10, value: model.blackHoleMass) { self.model.blackHoleMass = $0 }
            slider("Spin", min: -1, max: 1, value: model.spin) { self.model.spin = $0 }
            slider("Event horizon", min: 0.05, max: 0.8, value: model.eventHorizon) { self.model.eventHorizon = $0 }
            heading("PLANETS")
            let selected = model.planets.indices.contains(model.selectedPlanet) ? model.planets[model.selectedPlanet] : nil
            let summary = NSTextField(labelWithString: selected.map { _ in "Planet \(model.selectedPlanet + 1) of \(model.planets.count) · mass in scene units" } ?? "All planets devoured")
            summary.font = .systemFont(ofSize: 11)
            sidebar.addArrangedSubview(summary)
            if let selected {
                slider("Planet mass", min: 0.005, max: 0.25, value: selected.mass) { value in
                    guard self.model.planets.indices.contains(self.model.selectedPlanet) else { return }
                    self.model.planets[self.model.selectedPlanet].mass = value
                }
                slider("Planet size", min: 0.1, max: 1.5, value: selected.radius) { value in
                    guard self.model.planets.indices.contains(self.model.selectedPlanet) else { return }
                    self.model.planets[self.model.selectedPlanet].radius = value
                }
            }
            button("Add planet") { self.model.addPlanet() ; self.rebuildControls() }
            button("Select next planet") {
                guard !self.model.planets.isEmpty else { return }
                self.model.selectedPlanet = (self.model.selectedPlanet + 1) % self.model.planets.count
                self.rebuildControls()
            }
            button("Change planet color") { self.model.recolorSelectedPlanet() }
            button("Reset black hole") { self.model.resetBlackHole(); self.rebuildControls() }
            heading("SCENE")
            slider("Grid spacing", min: 0.2, max: 3, value: model.gridSpacing) { self.model.gridSpacing = $0 }
            slider("Grid extent", min: 2, max: 20, value: model.gridExtent) { self.model.gridExtent = $0 }
            slider("Light particles", min: 1_000, max: 1_000_000, value: Double(model.particleCount)) { self.model.particleCount = Int($0) }
            slider("Particle speed", min: 0.1, max: 6, value: model.particleSpeed) { self.model.particleSpeed = $0 }
            slider("Particle size", min: 1, max: 12, value: model.particleSize) { self.model.particleSize = $0 }
            slider("Grid opacity", min: 0.05, max: 1, value: model.gridOpacity) { self.model.gridOpacity = $0 }
            button(model.showGrid ? "Hide gravity grid" : "Show gravity grid") { self.model.showGrid.toggle(); self.rebuildControls() }
            button(model.trailsEnabled ? "Disable trails" : "Enable trails") { self.model.trailsEnabled.toggle(); self.rebuildControls() }
        } else {
            heading("NUCLEUS AND ELECTRONS")
            slider("Electrons", min: 0, max: 118, value: Double(model.electronCount)) { self.model.electronCount = Int($0) }
            slider("Protons", min: 1, max: 118, value: Double(model.protonCount)) { self.model.protonCount = Int($0) }
            slider("Neutrons", min: 0, max: 300, value: Double(model.neutronCount)) { self.model.neutronCount = Int($0) }
            let charge = model.protonCount - model.electronCount
            let info = NSTextField(labelWithString: "Net charge: \(charge >= 0 ? "+" : "")\(charge)e · electron charge −1.60218×10⁻¹⁹ C")
            info.font = .systemFont(ofSize: 11)
            info.lineBreakMode = .byWordWrapping
            info.preferredMaxLayoutWidth = 275
            sidebar.addArrangedSubview(info)
            let note = NSTextField(wrappingLabelWithString: "Electron dots are probability samples, not classical tracks. Heat color shows relative probability density.")
            note.font = .systemFont(ofSize: 11)
            note.preferredMaxLayoutWidth = 275
            sidebar.addArrangedSubview(note)
        }
    }
}

private final class ControlAction: NSObject {
    let action: () -> Void
    init(_ action: @escaping () -> Void) { self.action = action }
    @objc func invoke() { action() }
}

private final class CallbackSlider: NSSlider {
    var onChange: ((Double) -> Void)?
    init(value: Double, min minimum: Double, max maximum: Double) {
        super.init(frame: .zero)
        minValue = minimum
        maxValue = maximum
        doubleValue = value
        target = self
        action = #selector(changed)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) is unsupported") }
    @objc private func changed() { onChange?(doubleValue) }
}

private final class SimulationMTKView: MTKView {
    var onDrag: ((CGFloat, CGFloat) -> Void)?
    var onZoom: ((CGFloat) -> Void)?
    private var previous = NSPoint.zero
    override var acceptsFirstResponder: Bool { true }
    override func mouseDown(with event: NSEvent) { previous = event.locationInWindow }
    override func mouseDragged(with event: NSEvent) {
        let current = event.locationInWindow
        onDrag?(current.x - previous.x, previous.y - current.y)
        previous = current
    }
    override func scrollWheel(with event: NSEvent) { onZoom?(event.deltaY) }
}
