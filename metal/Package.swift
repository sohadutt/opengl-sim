// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "OpenGLSimMetal",
    platforms: [.macOS("26.0")],
    products: [
        .executable(name: "opengl-sim", targets: ["OpenGLSim"])
    ],
    targets: [
        .executableTarget(
            name: "OpenGLSim",
            path: "Sources/OpenGLSim",
            resources: [.process("Resources")]
        )
    ]
)
