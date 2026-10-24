#pragma once

#include <glm/glm.hpp>

namespace opengl_sim::physics {

struct planet {
    glm::dvec3 position{};
    double radius = 0.5;
    glm::vec3 color{0.14F, 0.55F, 0.92F};
    double rotation = 1.0;

    void update(double delta_seconds) noexcept;
};

} // namespace opengl_sim::physics
