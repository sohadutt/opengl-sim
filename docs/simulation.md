# Simulation and visualization

At startup, choose **Celestial Mode** or **Atomic Mode**.

## Atomic mode

Atomic mode begins with neutral hydrogen: one proton, no neutrons, and one electron. It computes the nonrelativistic hydrogenic 1s radial probability using the Coulomb potential and the electron-nucleus reduced mass. The proton, neutron, and electron charges use SI elementary-charge values; the displayed ionic charge is `(protons - electrons) × e`. Neutrons are electrically neutral, but their mass changes the reduced-mass correction.

The electron dots are Monte Carlo samples of orbital probability. They show where position measurements are more likely; they are not classical electrons moving on fixed paths. The heatmap displays spherically averaged electron probability density, normalized within the current atom. A spherical average is used for subshells so partially occupied p and d orbitals do not show a particular magnetic-orbital orientation. The nucleus is enlarged for visibility and is not to scale.

For more than one electron, the simulator fills subshells in Madelung/Aufbau order and estimates screening with Slater-style effective nuclear charges. It adds hydrogen-like radial densities for occupied subshells. This independent-electron approximation does not solve a self-consistent Hartree-Fock equation or a correlated many-electron wavefunction. Transition-metal shell exceptions, detailed spectra, collisions, ionization dynamics, and nuclear reactions are not modeled. Results for multi-electron atoms are visual estimates; one-electron hydrogenic states use the analytic nonrelativistic solution.

The electron charge, proton charge, particle masses, and Bohr radius use rounded [NIST 2022 CODATA values](https://physics.nist.gov/cuu/Constants/index.html). The elementary charge is exact in SI.

## Celestial mode

Celestial coordinates are scaled and dimensionless; this is an interactive visualization rather than a calibrated astrophysical simulation.

### Three-dimensional black hole

`physics::black_hole` stores a 3D center, mass, event-horizon radius, and spin. The renderer draws a near-black sphere sized to approximate the larger shadow seen by a distant observer.

### Planets and gravitational lensing

`physics::planet` stores a 3D position, velocity, scaled mass, radius, color, and rotation. The UI can add up to 32 planets and select each one to edit its mass and size. A softened Newtonian N-body model advances planet-planet gravity and black-hole recoil in bounded substeps. When a planet crosses the event-horizon radius, it is removed and its mass is added to the black hole; the horizon grows in proportion to the mass gain. The planet uses a smooth solid-color surface without a procedural stripe pattern. The scene is drawn to a color and depth target, then the point-mass lens equation is applied per camera pixel: each ray is back-projected to a source-image coordinate and samples the scene there. This naturally produces the primary and secondary images while warping the planets and light disk as the camera sees them. The depth check preserves objects in front of the black hole. The Einstein angle is estimated from black-hole mass, event-horizon scale, and camera/source distances; the shadow mask covers captured rays.

The celestial model uses scene-scaled masses and a tuned gravitational constant (`physics::scaled_celestial_G = 0.32`) rather than SI units. It is intended for interactive orbital behavior, not precision astrophysical predictions.

This is a depth-tested screen-space approximation, not full general-relativistic ray tracing. It does not integrate photons through curved spacetime or account for multiple lens planes.

### Curved grid and orbiting light

`physics::grid_plane::grid_lines()` generates endpoint pairs across the XZ plane. The renderer subdivides those lines and bends them vertically using a softened 3D distance from each grid point to the black-hole center. Mass controls the depth of the visible gravity well. Grid extent, spacing, thickness, opacity, and visibility are adjustable in the UI.

`physics::light_particle_field` stores particle count, animation speed, and elapsed time. The renderer creates particles around a thin equatorial disk in the vertex shader using `gl_VertexID`; it does not allocate one object per particle on the CPU. Particle radii begin just outside the shadow and extend toward the configured grid size. Mass changes approximate angular speed, spin adds small precession, and optional trails use four fading samples per particle.

### Celestial heatmap

The shader maps a proximity-based heat score from deep red through orange and gold to near-white at the inner edge. These colors do not represent physical temperature or radiation.

## Physical constants

`physics::G`, `physics::c`, the scaled celestial gravity value, and the atomic constants are defined in `include/opengl_sim/physics/constants.hpp`.
