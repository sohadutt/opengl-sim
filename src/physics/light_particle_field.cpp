#include "opengl_sim/physics/light_particle_field.hpp"

#include <cmath>

namespace opengl_sim::physics {

void light_particle_field::update(double delta_seconds) noexcept {
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) return;
    elapsed_time = std::fmod(elapsed_time + delta_seconds, 10000.0);
}

} // namespace opengl_sim::physics
