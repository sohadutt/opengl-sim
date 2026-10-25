#pragma once

#include "opengl_sim/app/scene.hpp"
#include "opengl_sim/core/logger.hpp"
#include "opengl_sim/physics/black_hole.hpp"
#include "opengl_sim/physics/grid_plane.hpp"
#include "opengl_sim/physics/light_particle_field.hpp"
#include "opengl_sim/physics/planet.hpp"

#include <cstddef>
#include <vector>

namespace opengl_sim::app {

class CelestialScene final : public Scene {
public:
    explicit CelestialScene(core::Logger& logger);

    [[nodiscard]] std::string_view id() const noexcept override { return "celestial"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "CELESTIAL MODE"; }
    void configureUI(ui::UserInterface& interface) override;
    void update(double delta_seconds) override;
    void draw(render::Renderer& renderer, const render::Camera& camera,
              int width, int height) override;

private:
    [[nodiscard]] glm::vec3 randomPlanetColor();

    physics::black_hole black_hole_{{0.0, 0.0, 0.0}, 1.0, 0.24, 0.5};
    physics::light_particle_field particles_;
    physics::grid_plane grid_{{0.0, 0.0, 0.0}, 1.0, 10.0, 0.01, 0.8};
    std::vector<physics::planet> planets_;
    std::size_t selected_planet_ = 0;
    bool show_grid_ = true;
};

} // namespace opengl_sim::app
