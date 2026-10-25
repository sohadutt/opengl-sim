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

`src/main.cpp` loads the settings, configures the logger, and starts `app::Application`. The application creates a GLFW window and OpenGL context, then registers the available scenes with `app::SceneManager`. The manager builds the scene selection panel, configures the active scene's controls, and forwards update and draw calls.

`render::Renderer` owns scene geometry and the off-screen scene/lensing targets. `render::ParticleRenderer` and `render::AtomRenderer` own separate GPU pipelines. The atom module creates orbital configurations and radial probability tables; its renderer uses those tables to sample the electron cloud and color the probability heatmap. The UI knows about generic values, toggles, informational labels, and actions; it does not depend on physics classes.

## Main interfaces

- `core::Settings` and `loadSettings()` in `include/opengl_sim/core/config.hpp` define startup configuration.
- `core::Logger` in `include/opengl_sim/core/logger.hpp` provides trace through critical log levels.
- `physics::black_hole`, `physics::grid_plane`, `physics::planet`, `physics::light_particle_field`, and `physics::atom` in `include/opengl_sim/physics/` hold the simulation models.
- `render::Camera`, `render::Renderer`, `render::ParticleRenderer`, and `render::AtomRenderer` in `include/opengl_sim/render/` control viewing and scene drawing.
- `ui::UserInterface` in `include/opengl_sim/ui/user_interface.hpp` exposes slider, toggle, button, and heatmap-legend registration.
- `app::Scene` in `include/opengl_sim/app/scene.hpp` is the shared contract for a simulation scene. `CelestialScene` and `AtomicScene` own their simulation state, controls, update logic, and render calls.
- `app::SceneManager` owns registered scenes, scene selection, and the common return-to-scenes action.

## Adding a scene

1. Add a class under `include/opengl_sim/app/scenes/` that derives from `app::Scene`.
2. Give it a stable `id()` and a UI title from `title()`.
3. Keep its simulation data in the class. Implement `configureUI()`, `update()`, and `draw()` for its controls, simulation step, and rendering.
4. Add the implementation file to the `opengl-sim` source list in `CMakeLists.txt`.
5. Register it in `src/app/application.cpp` with `scenes.registerScene(std::make_unique<YourScene>(...))`.

The scene manager provides a selection button and a **Back to Scenes** button automatically. Window creation, camera input, and the main frame loop stay shared, so a new scene only needs to implement its own simulation behavior.

## Adding a module

For a new reusable subsystem, add its public interface under `include/opengl_sim/<module>/`, implementation under `src/<module>/`, and a CMake library target or source list entry with only the dependencies it needs. Keep app-specific wiring in `src/app/application.cpp`. See [development.md](development.md) for the existing CMake and UI conventions.
