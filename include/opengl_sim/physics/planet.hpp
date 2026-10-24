#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace opengl_sim::physics {

struct planet {
    glm::dvec3 position{};
    double radius = 0.5;
    glm::vec3 color{0.14F, 0.55F, 0.92F};
    double rotation = 1.0;
    // Mass uses scaled scene units so the controls stay practical.
    double mass = 0.04;
    glm::dvec3 velocity{};

    void update(double delta_seconds) noexcept;
};

struct black_hole;
void advanceCelestialSystem(black_hole& hole, std::vector<planet>& planets,
                            double delta_seconds);

} // namespace opengl_sim::physics
