#include "opengl_sim/physics/planet.hpp"

#include <cmath>

namespace opengl_sim::physics {

void planet::update(double delta_seconds) noexcept {
    if (delta_seconds <= 0.0) return;
    rotation = std::fmod(rotation + delta_seconds * 0.35, 6.283185307179586);
}

} // namespace opengl_sim::physics
