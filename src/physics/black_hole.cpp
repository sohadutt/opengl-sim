#include "opengl_sim/physics/black_hole.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace opengl_sim::physics {

std::vector<glm::dvec2> black_hole::event_horizon_outline(std::size_t segments) const {
    if (segments < 3) {
        throw std::invalid_argument{"An event horizon outline requires at least 3 segments."};
    }
    if (event_horizon < 0.0) {
        throw std::invalid_argument{"The event horizon radius cannot be negative."};
    }

    std::vector<glm::dvec2> points;
    points.reserve(segments + 1);
    for (std::size_t i = 0; i <= segments; ++i) {
        const auto angle = (2.0 * std::numbers::pi * static_cast<double>(i)) /
                           static_cast<double>(segments);
        points.emplace_back(position.x + event_horizon * std::cos(angle),
                            position.y + event_horizon * std::sin(angle));
    }
    return points;
}

} // namespace opengl_sim::physics
