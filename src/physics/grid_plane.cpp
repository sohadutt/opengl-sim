#include "opengl_sim/physics/grid_plane.hpp"

#include <cmath>
#include <stdexcept>

namespace opengl_sim::physics {

std::vector<glm::dvec3> grid_plane::grid_lines() const {
    if (spacing <= 0.0 || size <= 0.0) {
        throw std::invalid_argument{"Grid spacing and size must be positive."};
    }

    const auto half_size = size * 0.5;
    const auto line_count = static_cast<int>(std::floor(half_size / spacing));
    if (line_count > 1000) {
        throw std::invalid_argument{"Grid settings would create too many lines."};
    }

    std::vector<glm::dvec3> endpoints;
    endpoints.reserve(static_cast<std::size_t>(line_count * 2 + 1) * 4);
    for (int i = -line_count; i <= line_count; ++i) {
        const auto offset = static_cast<double>(i) * spacing;
        endpoints.emplace_back(position.x + offset, position.y, position.z - half_size);
        endpoints.emplace_back(position.x + offset, position.y, position.z + half_size);
        endpoints.emplace_back(position.x - half_size, position.y, position.z + offset);
        endpoints.emplace_back(position.x + half_size, position.y, position.z + offset);
    }
    return endpoints;
}

} // namespace opengl_sim::physics
