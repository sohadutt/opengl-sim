#pragma once

#include <glm/glm.hpp>

namespace opengl_sim::physics {

struct black_hole {
    glm::dvec3 position{};
    double mass{};
    double event_horizon{};
    double spin{};
};

} // namespace opengl_sim::physics
