# Metal 4 Swift app

This is a standalone macOS Swift and Metal 4 version of the simulator. It lives alongside the root C++/OpenGL project, which remains unchanged as the main application. The Swift package and its management script are self-contained in this directory.

## Requirements

- macOS 26 or later and a GPU that supports the Metal 4 family. The app checks this when it starts.
- Xcode 27 or later, including the macOS SDK and Metal Toolchain.
- No external package dependencies.

The Command Line Tools package by itself does not include the Metal shader compiler needed by this project.

## Build and run

From this directory:

```sh
./scripts/manage.sh build
./scripts/manage.sh run
```

Other commands:

```sh
./scripts/manage.sh release
./scripts/manage.sh clean
```

You can also open `Package.swift` in Xcode and run the `opengl-sim` executable product. Xcode's command launcher can exist even when its optional Metal compiler component is missing. Install that component with:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
xcodebuild -downloadComponent MetalToolchain
```

The build script checks that the compiler can actually run before starting SwiftPM and prints these commands when the component is unavailable. It selects `/Applications/Xcode.app` automatically when `DEVELOPER_DIR` points to the standalone Command Line Tools; selecting Xcode for component installation also requires the macOS administrator password.

## Structure

- `Sources/OpenGLSim/SimulationModel.swift` — scene state, simulation constants, camera values, and scaled N-body evolution.
- `Sources/OpenGLSim/Renderer.swift` — Metal 4 command submission, buffers, render pipelines, and camera projection.
- `Sources/OpenGLSim/Resources/Shaders.metal` — scene meshes, lensing approximation, bent grid, particle field, and atom cloud shaders.
- `Sources/OpenGLSim/GameViewController.swift` — macOS controls, mode selection, and mouse/touchpad input.
- `Sources/OpenGLSim/SimulatorSettings.swift` — settings and level-filtered logging to `logs/simulator.log`, with an Application Support fallback if the launch directory is not writable.
- `Sources/OpenGLSim/Resources/simulator.conf` — log level and initial window dimensions.
- `docs/` — architecture and simulation notes.

The celestial model uses scene-scaled units and softened Newtonian gravity for interactive visualization. The lens shader approximates the point-mass image equation on mesh vertices; it does not integrate full relativistic light paths. Atomic mode samples a hydrogenic radial distribution for the first shell and uses screened shells at higher electron counts. It is not a self-consistent many-electron quantum calculation, and cloud dots represent probability samples rather than classical electron paths.
