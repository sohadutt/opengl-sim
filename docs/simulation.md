# Simulation and visualization

The current scene uses scaled, dimensionless coordinates. It is designed as an interactive 3D visualization, not a calibrated astrophysical simulation.

## Three-dimensional black hole

`physics::black_hole` stores a 3D center, mass, event-horizon radius, and spin. The renderer draws a near-black sphere sized to approximate the larger shadow seen by a distant observer.

## Planets and gravitational lensing

`physics::planet` stores a 3D position, radius, color, and rotation. The planet uses a smooth solid-color surface without a procedural stripe pattern. The scene is drawn to a color and depth target, then the point-mass lens equation is applied per camera pixel: each ray is back-projected to a source-image coordinate and samples the scene there. This naturally produces the primary and secondary images while warping the planet and light disk as the camera sees them. The depth check preserves objects in front of the black hole. The Einstein angle is estimated from black-hole mass, event-horizon scale, and camera/source distances; the shadow mask covers captured rays.

This is a depth-tested screen-space approximation, not full general-relativistic ray tracing. It does not integrate photons through curved spacetime or account for multiple lens planes.

## Curved grid

`physics::grid_plane::grid_lines()` generates endpoint pairs across the XZ plane. The renderer subdivides those lines and bends them vertically using a softened 3D distance from each grid point to the black-hole center. Mass controls the depth of the visible gravity well. Grid extent, spacing, thickness, opacity, and visibility are adjustable in the UI.

## Orbiting light disk

`physics::light_particle_field` stores the particle count, animation speed, and elapsed time. The renderer distributes particles around a thin equatorial disk with slight vertical thickness. Particle radii begin just outside the shadow and extend toward the configured grid size; mass changes their approximate angular speed, while spin adds a small precession.

Particle positions are calculated in the vertex shader from `gl_VertexID`; the CPU does not allocate one object per particle. The **PARTICLE COUNT** control ranges from one to one million, **PARTICLE SPEED** controls orbital speed, and **PARTICLE SIZE** changes the rendered point size. The **TRAILS** toggle draws six fading time samples per particle to show its recent path; enabling trails increases the point workload sixfold. Actual frame rate at high counts depends on the graphics hardware and viewport size.

## Heatmap

The shader maps a proximity-based heat score from deep red through orange and gold to near-white at the inner edge. The UI legend shows the same palette. These colors do not represent physical temperature or radiation.

## Current limits and extension points

- Particle orbits and spin precession are visual approximations; the renderer does not integrate individual general-relativistic trajectories.
- `physics::G` and `physics::c` are defined as physical constants, but the current scene uses scaled parameters rather than SI unit conversions.
- `atom` and `simulation` source files are placeholders for future implementations.

Future work can replace the approximate GPU particle motion with a physical 3D integrator while keeping the renderer's batched point drawing.
