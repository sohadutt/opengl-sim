#include "opengl_sim/physics/planet.hpp"
#include "opengl_sim/physics/black_hole.hpp"
#include "opengl_sim/physics/constants.hpp"

#include <cmath>
#include <algorithm>

namespace opengl_sim::physics {

void planet::update(double delta_seconds) noexcept {
    if (delta_seconds <= 0.0) return;
    rotation = std::fmod(rotation + delta_seconds * 0.35, 6.283185307179586);
}

void advanceCelestialSystem(black_hole& hole, std::vector<planet>& planets,
                            double delta_seconds) {
    if (delta_seconds <= 0.0 || planets.empty()) return;
    constexpr double softening = 0.08;
    constexpr int max_substeps = 32;
    const auto step_count = std::clamp(static_cast<int>(std::ceil(delta_seconds / 0.004)),
                                       1, max_substeps);
    const auto step = delta_seconds / step_count;
    for (int substep = 0; substep < step_count; ++substep) {
        std::vector<glm::dvec3> planet_acceleration(planets.size(), glm::dvec3{0.0});
        glm::dvec3 hole_acceleration{0.0};
        for (std::size_t i = 0; i < planets.size(); ++i) {
            const auto offset = hole.position - planets[i].position;
            const auto distance_squared = glm::dot(offset, offset) + softening * softening;
            const auto inverse_cube = 1.0 / (distance_squared * std::sqrt(distance_squared));
            planet_acceleration[i] += scaled_celestial_G * hole.mass * offset * inverse_cube;
            hole_acceleration -= scaled_celestial_G * planets[i].mass * offset * inverse_cube;
            for (std::size_t j = i + 1; j < planets.size(); ++j) {
                const auto pair_offset = planets[j].position - planets[i].position;
                const auto pair_squared = glm::dot(pair_offset, pair_offset) + softening * softening;
                const auto pair_inverse_cube = 1.0 / (pair_squared * std::sqrt(pair_squared));
                planet_acceleration[i] += scaled_celestial_G * planets[j].mass * pair_offset * pair_inverse_cube;
                planet_acceleration[j] -= scaled_celestial_G * planets[i].mass * pair_offset * pair_inverse_cube;
            }
        }
        hole.velocity += hole_acceleration * step;
        hole.position += hole.velocity * step;
        for (std::size_t i = 0; i < planets.size(); ++i) {
            planets[i].velocity += planet_acceleration[i] * step;
            planets[i].position += planets[i].velocity * step;
            planets[i].update(step);
        }
        const auto capture_radius = std::max(hole.event_horizon, 0.01);
        for (auto it = planets.begin(); it != planets.end();) {
            if (glm::length(it->position - hole.position) <= capture_radius) {
                const auto old_mass = hole.mass;
                hole.mass += it->mass;
                if (old_mass > 0.0) hole.event_horizon *= hole.mass / old_mass;
                it = planets.erase(it);
            } else {
                ++it;
            }
        }
    }
}

} // namespace opengl_sim::physics
