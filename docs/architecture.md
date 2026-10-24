# Architecture

The project is split into four libraries and one executable. The libraries own a focused part of the application; `opengl-sim` wires those parts together.

## Modules

| Target | Responsibilities | Dependencies |
| --- | --- | --- |
| `sim_core` | Parse `config/simulator.conf`; provide `Settings` and the thread-safe, level-filtered logger. | C++ standard library |
| `sim_physics` | Store the 3D black-hole, grid, planet, and particle-emitter models; generate grid lines and advance animation. | GLM |
| `sim_render` | Own the 3D camera and coordinate scene geometry, the dedicated GPU particle renderer, and the screen-space lensing pass. | OpenGL, `sim_core`, `sim_physics` |
| `sim_ui` | Draw and handle sliders, toggles, buttons, and the particle-heat legend. | OpenGL, `sim_core` |
| `opengl-sim` | Initialize GLFW, create models and controls, forward input, and run the frame loop. | All modules and GLFW |

## Runtime flow

`src/main.cpp` loads the settings, configures the logger, and starts `app::Application`. The application creates a GLFW window and OpenGL context, initializes the renderer and UI, and binds UI controls to model properties. Every frame it processes input, advances the particle emitter clock, draws the 3D scene and GPU-generated particles, draws the UI, and presents the frame.

`render::Renderer` owns scene geometry and the off-screen scene/lensing targets. `render::ParticleRenderer` owns the independent particle shader and vertex array, so particle controls and orbit rendering do not expand the general scene shader. The UI knows about generic numeric values, toggles, and actions; it does not depend on physics classes. The application is the composition point where UI controls are connected to specific simulation properties.

## Main interfaces

- `core::Settings` and `loadSettings()` in `include/opengl_sim/core/config.hpp` define startup configuration.
- `core::Logger` in `include/opengl_sim/core/logger.hpp` provides trace through critical log levels.
- `physics::black_hole`, `physics::grid_plane`, `physics::planet`, and `physics::light_particle_field` in `include/opengl_sim/physics/` hold the scene models.
- `render::Camera`, `render::Renderer`, and `render::ParticleRenderer` in `include/opengl_sim/render/` control viewing, scene drawing, and particle drawing.
- `ui::UserInterface` in `include/opengl_sim/ui/user_interface.hpp` exposes slider, toggle, button, and heatmap-legend registration.

## Adding a module

For a new reusable subsystem, add its public interface under `include/opengl_sim/<module>/`, implementation under `src/<module>/`, and a CMake library target or source list entry with only the dependencies it needs. Keep app-specific wiring in `src/app/application.cpp`. See [development.md](development.md) for the existing CMake and UI conventions.
