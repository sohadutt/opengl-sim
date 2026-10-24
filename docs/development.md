# Development guide

## Build

Install CMake 4.4+, a C++23 compiler, GLFW 3.3+, GLM, and platform OpenGL development files. Linux and Windows also require GLEW. Platform package examples are in the root [README](../README.md).

Use the project script for the regular Release build:

```sh
./scripts/manage.sh build
```

Or configure a build directory directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## Unit tests

The CTest suite covers grid generation and invalid dimensions, simulation time updates, planet rotation, logger level parsing, and settings loading with defaults and invalid values. These tests do not create an OpenGL window.

Build and run the suite with the project script:

```sh
./scripts/manage.sh test
```

Or run CTest after configuring and building:

```sh
ctest --test-dir build --output-on-failure
```

On Windows, use `scripts\manage.bat test`.

The executable is written to `build/opengl-sim` (or `build/opengl-sim.exe` on Windows). Run it from the project root so project-relative output such as logs has a predictable location. The CMake configuration writes `compile_commands.json` into the build directory.

On Windows, `scripts\manage.bat` provides `build`, `run`, `test`, `clean`, and `--single`, and configures GCC with Ninja. Its `--auto` option uses winget to install MSYS2 when needed, then pacman to install the UCRT64 GCC toolchain, CMake, Ninja, GLFW, GLM, and GLEW:

```bat
scripts\manage.bat build --auto
scripts\manage.bat run --auto --single
```

The automatic setup requires winget and network access. It looks for MSYS2 at `C:\msys64`, `%ProgramFiles%\MSYS2`, or `%LOCALAPPDATA%\Programs\MSYS2`; set `MSYS2_ROOT` to use another installation directory.

### Windows GCC build with MSYS2

To build with open source GCC manually, install the packages listed in the [Windows GCC instructions](../README.md#windows-with-open-source-gcc), open the **MSYS2 UCRT64** terminal, and use the Ninja generator explicitly:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
./build/opengl-sim.exe
```

Run these commands from the project root inside UCRT64. Its `PATH` includes the matching runtime DLLs. If `build/` was configured earlier with another compiler or generator, remove that directory before configuring with GCC.

## CMake conventions

`configure_simulator_target()` applies the public include directory, C++23, disabled compiler extensions, and strict warnings (`/W4 /WX` for MSVC, `-Wall -Wextra -Werror` for GCC/Clang). Add source files to their owning library in `CMakeLists.txt` and link dependencies at the narrowest appropriate target. `Dependencies.cmake` discovers GLFW, GLM, and OpenGL/GLEW. On macOS, `LocalGLFWRuntime.cmake` adjusts the executable's runtime search path when the local GLFW library is selected.

Release builds enable interprocedural optimization when CMake reports that the active toolchain supports it. The renderer caches the unit sphere mesh and generates particle positions in the vertex shader from `gl_VertexID`, so high particle counts do not require CPU-side particle allocations or per-particle draw calls.

Keep the executable as the composition root: reusable model and UI behavior belongs in the libraries, while GLFW callbacks, initial model values, and binding controls to models belong in `src/app/application.cpp`.

## UI conventions

`ui::UserInterface` supports sliders for arithmetic values, toggles backed by a `bool`, action buttons, and a heatmap legend label. Sliders bind getter and setter callbacks, so they can also connect to a property that is not stored as a plain numeric reference. The UI owns pointer interaction and drawing; it should not know what a value means.

Example from the application:

```cpp
interface.addSlider("MASS", black_hole.mass, 0.1, 10.0);
interface.addToggle("GRID", show_grid);
interface.addButton("RESET BLACK HOLE", [&black_hole] {
    black_hole.mass = 1.0;
});
```

For a new model control, register it in the application and consume the model value in physics or rendering code. Keep reasonable slider bounds next to the registration.

## Settings and logging

The application reads `config/simulator.conf`, an INI-style file with `[app]` and `[window]` sections. Supported keys are `app.log_level`, `app.log_file`, `window.width`, `window.height`, `window.title`, and `window.vertical_sync`. Unknown keys are logged as warnings. Relative log-file paths are resolved from the project root; logs append to the file.

Log levels from least to most restrictive are `trace`, `debug`, `info`, `warning`, `error`, `critical`, and `off`. Change `[app] log_level` to control emitted messages. GLFW, shader compilation, settings, and startup errors are sent to the logger.

## Repository notes

- `scripts/manage.sh clean` removes the generated `build/` directory.
- `scripts\manage.bat clean` is the Windows Command Prompt equivalent.
- `scripts/manage.sh build --single` copies the executable to the project root. It does not package dynamic-library dependencies.
- `logs/`, `build/`, and generated executables are ignored by Git.
- The checked-in `third_party/glfw/lib/libglfw.3.5.dylib` is specifically a macOS arm64 runtime; it is not a portable dependency bundle.
