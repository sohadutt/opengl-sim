#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace opengl_sim::physics {

struct grid_plane {
    glm::dvec3 position{};
    double spacing = 0.5;
    double size = 10.0;
    double line_thickness = 0.01;
    double line_opacity = 0.5;

    // Returns endpoint pairs on the horizontal XZ plane.
    [[nodiscard]] std::vector<glm::dvec3> grid_lines() const;
};

} // namespace opengl_sim::physics
