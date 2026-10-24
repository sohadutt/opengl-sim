# opengl-sim

A C++23 GLFW/OpenGL project for exploring light around compact objects and atomic systems. It loads settings, logs startup errors, and renders a black hole with an event horizon and a stylized accretion ring.

## Requirements

- CMake 4.4 or newer
- A C++23-capable compiler (MSVC, GCC, or Clang)
- Bash to use the management script (macOS/Linux, or Git Bash on Windows)
- GLEW on Linux and Windows; macOS uses the system OpenGL framework

GLFW headers and the library are under `third_party/glfw/`. The included `libglfw.3.5.dylib` is a macOS arm64 build; other platforms need a compatible GLFW library there or an installed GLFW 3.3+ CMake package.

## Project layout

```text
config/                      Runtime settings
cmake/                       Dependency discovery and platform setup
include/opengl_sim/
  app/                       Application interface
  core/                      Settings and logging interfaces
  physics/                   Simulation models and constants
  render/                    Renderer and camera interfaces
  ui/                        Reusable buttons and sliders
src/                         Implementations, grouped by module
assets/shaders/              Shader files for future renderer assets
scripts/manage.sh            Configure, build, run, and clean
third_party/glfw/            Optional project-local GLFW copy
```

CMake builds reusable `sim_core`, `sim_physics`, `sim_render`, and `sim_ui` modules. The `opengl-sim` executable connects them in `src/app/`. New implementation files should be added to their module's source list in `CMakeLists.txt`.

`config/simulator.conf` contains startup settings. Logs are written to `logs/simulator.log`. The empty atom, light-ray, and simulation files under `physics/` are placeholders for future modules.

The black hole is currently a visual model; light bending, atomic simulation, and UI controls are not implemented yet. The speed of light is defined as a constant in `include/opengl_sim/physics/constants.hpp`.

The window includes a black-hole panel with sliders for mass, spin, and event-horizon size plus a reset button. Drag sliders with the left mouse button. Drag on the scene to look around, scroll to zoom, and use **Shift+scroll** to pan. **WASD** flies, **E/Q** moves vertically, and **+/-** changes the field of view. Press **Escape** to quit. These pointer gestures work with a mouse or trackpad.

`ui::UserInterface` is simulation-independent. Register numeric values with `addSlider`; it supports both floating-point and integer values, so later controls such as particle count can use the same widget system. The app binds the current black-hole controls to its model.

Set `app.log_level` in the config file to `trace`, `debug`, `info`, `warning`, `error`, `critical`, or `off`.

## Build with CMake

From the project root, configure and compile:

```sh
cmake -S . -B build
cmake --build build --config Release
```

Run the resulting program:

```sh
# macOS / Linux
./build/opengl-sim

# Windows PowerShell
.\build\opengl-sim.exe
```

## Use the management script

The shell script configures and builds the project, runs it, or removes build output:

```sh
./scripts/manage.sh build
./scripts/manage.sh run
./scripts/manage.sh clean
```

To also copy the built executable into the project root, use `--single`:

```sh
./scripts/manage.sh build --single
```

This creates `opengl-sim` on macOS/Linux or `opengl-sim.exe` on Windows. The root copy is ignored by Git.
On Windows, run these commands from Git Bash.
