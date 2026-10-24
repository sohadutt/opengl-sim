#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace opengl_sim::physics {

struct black_hole {
    glm::dvec2 position{};
    double mass{};
    double event_horizon{};
    double spin{};

    [[nodiscard]] std::vector<glm::dvec2>
    event_horizon_outline(std::size_t segments = 128) const;
};

} // namespace opengl_sim::physics
