# Architecture

The project is split into four libraries and one executable. The libraries own a focused part of the application; `opengl-sim` wires those parts together.

## Modules

| Target | Responsibilities | Dependencies |
| --- | --- | --- |
| `sim_core` | Parse `config/simulator.conf`; provide `Settings` and the thread-safe, level-filtered logger. | C++ standard library |
| `sim_physics` | Store black-hole, grid, planet, light-particle, and atom models; calculate screened atomic orbitals and charge. | GLM, C++ standard library |
| `sim_render` | Own the camera and coordinate scene geometry, GPU light particles, atomic probability samples, and screen-space lensing. | OpenGL, `sim_core`, `sim_physics` |
| `sim_ui` | Draw and handle sliders, toggles, buttons, and the particle-heat legend. | OpenGL, `sim_core` |
| `opengl-sim` | Initialize GLFW, create models and controls, forward input, and run the frame loop. | All modules and GLFW |

## Runtime flow

`src/main.cpp` loads the settings, configures the logger, and starts `app::Application`. The application creates a GLFW window and OpenGL context, then asks the user to select celestial or atomic mode. It binds the selected mode's controls to model properties and draws that mode each frame.

`render::Renderer` owns scene geometry and the off-screen scene/lensing targets. `render::ParticleRenderer` and `render::AtomRenderer` own separate GPU pipelines. The atom module creates orbital configurations and radial probability tables; its renderer uses those tables to sample the electron cloud and color the probability heatmap. The UI knows about generic values, toggles, informational labels, and actions; it does not depend on physics classes.

## Main interfaces

- `core::Settings` and `loadSettings()` in `include/opengl_sim/core/config.hpp` define startup configuration.
- `core::Logger` in `include/opengl_sim/core/logger.hpp` provides trace through critical log levels.
- `physics::black_hole`, `physics::grid_plane`, `physics::planet`, `physics::light_particle_field`, and `physics::atom` in `include/opengl_sim/physics/` hold the simulation models.
- `render::Camera`, `render::Renderer`, `render::ParticleRenderer`, and `render::AtomRenderer` in `include/opengl_sim/render/` control viewing and scene drawing.
- `ui::UserInterface` in `include/opengl_sim/ui/user_interface.hpp` exposes slider, toggle, button, and heatmap-legend registration.

## Adding a module

For a new reusable subsystem, add its public interface under `include/opengl_sim/<module>/`, implementation under `src/<module>/`, and a CMake library target or source list entry with only the dependencies it needs. Keep app-specific wiring in `src/app/application.cpp`. See [development.md](development.md) for the existing CMake and UI conventions.
