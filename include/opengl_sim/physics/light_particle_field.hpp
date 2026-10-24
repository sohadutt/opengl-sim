#pragma once

namespace opengl_sim::physics {

struct light_particle_field {
    int count = 50000;
    double speed = 0.9;
    double size = 4.0;
    bool trails_enabled = false;
    double elapsed_time = 0.0;

    // Advances the shader's deterministic orbit clock; particle positions are
    // evaluated on the GPU, so the CPU does not upload a per-frame vertex list.
    void update(double delta_seconds) noexcept;
};

} // namespace opengl_sim::physics
