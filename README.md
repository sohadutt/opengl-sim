# opengl-sim

`opengl-sim` is a modular C++23 GLFW/OpenGL application for visual experiments with gravity and light. The current scene is fully 3D: a dark black-hole shadow, a gravity-warped grid, a colored planet shown through an approximate point-mass lens equation, and a thin glowing disk of orbiting light particles. Atomic and general simulation modules are planned placeholders.

## Current features

- Adjustable black-hole mass, spin, and event-horizon radius.
- A near-black 3D shadow with a silhouette scaled to approximate the larger photon capture shadow.
- A smoothly shaded, solid-color planet that forms primary and fainter mirrored images around the black hole when behind it.
- A gravity-warped grid with controls for visibility, spacing, extent, thickness, and opacity.
- Up to one million GPU-generated light particles orbit in a thin, glowing disk. Particle size is adjustable, with an optional fading trail.
- Particle color shifts from deep red through orange and gold to white near the inner edge; the legend is shown in the scene window.
- A modular UI, camera controls, settings file, and level-filtered logging.

The visual model is qualitative. A depth-tested screen-space pass bends camera rays through a point-mass lens equation, so the planet and light disk appear through the black hole together. It does not integrate full general-relativistic paths. Heat colors do not represent calibrated temperatures, and particle orbits are approximate. `physics::G` and `physics::c` are defined in `include/opengl_sim/physics/constants.hpp`, but the current visualization uses scaled simulation units.

## Requirements

- CMake 4.4 or newer.
- A C++23 compiler: Clang/AppleClang, GCC, or MSVC.
- GLFW 3.3 or newer and GLM.
- GLEW on Linux and Windows. macOS uses the system OpenGL framework.
- Bash for `scripts/manage.sh` (available by default on macOS/Linux and through Git Bash on Windows).

On macOS with Homebrew:

```sh
brew install cmake glfw glm
```

On Debian or Ubuntu:

```sh
sudo apt install cmake build-essential libglfw3-dev libglm-dev libglew-dev libgl1-mesa-dev
```

### Windows with open source GCC

Use [MSYS2](https://www.msys2.org/) and its **UCRT64** terminal to get a GCC toolchain and the matching GLFW, GLM, and GLEW packages. In the MSYS2 UCRT64 terminal, update the package database and system packages:

```sh
pacman -Syu
```

If the update asks you to close the terminal, close it, reopen **MSYS2 UCRT64**, and run `pacman -Syu` again. Then install the compiler, CMake, Ninja, and project dependencies:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-glfw \
  mingw-w64-ucrt-x86_64-glm \
  mingw-w64-ucrt-x86_64-glew
```

From the project root in that same UCRT64 terminal, configure CMake to use GCC and Ninja, then build and run:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
./build/opengl-sim.exe
```

Run `ctest --test-dir build --output-on-failure` to run the test suite. Launch the program from the project root so `logs/simulator.log` is created in the expected location. The UCRT64 terminal supplies the runtime DLL search path for the MSYS2 libraries. The Windows management script also uses GCC and Ninja; its `--auto` option can install MSYS2 and the required UCRT64 packages.

The CMake dependency setup first checks for a compatible project-local GLFW copy under `third_party/glfw/`, then checks for a system GLFW package. On macOS it also checks Homebrew's package prefix. The checked-in `third_party/glfw/lib/libglfw.3.5.dylib` is a macOS arm64 runtime library; other systems need their own compatible GLFW installation.

## Build and run

From the project root, the management script configures a Release build and compiles it:

```sh
./scripts/manage.sh build
./scripts/manage.sh run
./scripts/manage.sh test
```

To configure and build directly with CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure
```

Run the executable from the project root so relative runtime paths such as the log file resolve as expected:

```sh
# macOS / Linux
./build/opengl-sim

# Windows PowerShell
.\build\opengl-sim.exe
```

The management scripts support cleanup and placing a copy of the executable in the project root. On Windows, use the batch version from Command Prompt:

```sh
./scripts/manage.sh clean
./scripts/manage.sh build --single
./scripts/manage.sh run --single
```

```bat
scripts\manage.bat build
scripts\manage.bat run
scripts\manage.bat test
scripts\manage.bat clean
scripts\manage.bat build --single
```

`--single` copies the built program to the project root for convenience. It does not bundle GLFW or other shared libraries. The Windows script uses GCC with Ninja and accepts `--auto` on `build`, `run`, and `test` to install MSYS2 and its UCRT64 GCC, CMake, Ninja, GLFW, GLM, and GLEW packages with winget and pacman:

```bat
scripts\manage.bat build --auto
scripts\manage.bat run --auto --single
```

`--auto` requires Windows Package Manager (`winget`) and an internet connection. By default, the script looks for MSYS2 under `C:\msys64`, `%ProgramFiles%\MSYS2`, or `%LOCALAPPDATA%\Programs\MSYS2`. Set `MSYS2_ROOT` if MSYS2 is installed in a different folder.

## Controls

The left panel contains sliders for grid settings, particle count and speed, and black-hole mass, spin, and event-horizon radius. Use the **GRID** toggle to show or hide the grid, and **RESET BLACK HOLE** to restore the black-hole defaults. Drag sliders with the left mouse button.

- Drag the scene with the left mouse button to look around.
- Drag with the right mouse button to look around while over the controls.
- Scroll to zoom; hold **Shift** while scrolling to pan.
- **W/A/S/D** move the camera horizontally; **E/Q** move vertically.
- **+/-** changes the field of view.
- **Escape** exits.

Mouse and trackpad gestures use GLFW pointer and scroll events.

## Settings and logs

Edit [`config/simulator.conf`](config/simulator.conf) to change the startup log level, log file, window dimensions, title, or vertical sync. Supported log levels are `trace`, `debug`, `info`, `warning`, `error`, `critical`, and `off`. Logs are appended to `logs/simulator.log` by default; the application creates the log directory if needed.

## Project structure

```text
config/                       Runtime settings
docs/                         Architecture, simulation, and development guides
include/opengl_sim/
  app/                         Application interface
  core/                        Settings, logging, and shared utilities
  physics/                     Black hole, grid, planet, particle field, and future models
  render/                      OpenGL renderer and camera interfaces
  ui/                          Reusable on-screen controls
src/                           Implementations, grouped by module
assets/shaders/                Shader assets
scripts/manage.sh              Configure, build, run, and clean
third_party/glfw/              Optional/local GLFW runtime library
CMakeLists.txt                 Build targets and module dependencies
```

CMake builds `sim_core`, `sim_physics`, `sim_render`, and `sim_ui` as static libraries. The `opengl-sim` executable composes these modules in `src/app/`. See the [architecture guide](docs/architecture.md), [simulation notes](docs/simulation.md), and [development guide](docs/development.md) for details.
