#include "opengl_sim/app/scenes/celestial_scene.hpp"

#include "opengl_sim/physics/simulation.hpp"
#include "opengl_sim/render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace opengl_sim::app {
namespace {

constexpr std::size_t maximum_planets = 32;

} // namespace

CelestialScene::CelestialScene(core::Logger&) {
    physics::planet starter{{0.35, 0.12, -2.2}, 0.52, {0.16F, 0.58F, 0.95F}};
    starter.velocity = {0.37, 0.0, 0.06};
    planets_.push_back(starter);
}

glm::vec3 CelestialScene::randomPlanetColor() {
    static std::mt19937 engine{std::random_device{}()};
    static std::uniform_real_distribution<float> channel{0.15F, 0.95F};
    return {channel(engine), channel(engine), channel(engine)};
}

void CelestialScene::configureUI(ui::UserInterface& interface) {
    interface.addSlider("PLANET SIZE", [this] {
        return planets_.empty() ? 0.52 : planets_[selected_planet_].radius;
    }, [this](double value) {
        if (!planets_.empty()) planets_[selected_planet_].radius = value;
    }, 0.1, 2.0);
    interface.addSlider("PLANET MASS", [this] {
        return planets_.empty() ? 0.04 : planets_[selected_planet_].mass;
    }, [this](double value) {
        if (!planets_.empty()) planets_[selected_planet_].mass = value;
    }, 0.005, 0.25);
    interface.addInfo([this] {
        return planets_.empty() ? std::string{"NO PLANETS"} :
            std::string{"PLANET "} + std::to_string(selected_planet_ + 1) + "/" +
                std::to_string(planets_.size());
    });
    interface.addButton("ADD PLANET", [this] {
        if (planets_.size() >= maximum_planets) return;
        const auto index = planets_.size();
        const auto angle = static_cast<double>(index) * 2.399963229728653;
        const auto distance = 2.2 + 0.35 * static_cast<double>(index % 6);
        physics::planet body;
        body.position = black_hole_.position + glm::dvec3{
            std::cos(angle) * distance, 0.15 * static_cast<double>(index % 3),
            std::sin(angle) * distance};
        const auto orbital_speed = std::sqrt(physics::scaled_celestial_G *
                                             std::max(black_hole_.mass, 0.05) / distance);
        body.velocity = {-std::sin(angle) * orbital_speed, 0.0,
                         std::cos(angle) * orbital_speed};
        body.color = randomPlanetColor();
        planets_.push_back(body);
        selected_planet_ = planets_.size() - 1;
    });
    interface.addButton("NEXT PLANET", [this] {
        if (!planets_.empty()) selected_planet_ = (selected_planet_ + 1) % planets_.size();
    });
    interface.addButton("CHANGE PLANET COLOUR", [this] {
        if (!planets_.empty()) planets_[selected_planet_].color = randomPlanetColor();
    });
    interface.addSlider("GRID SPACING", grid_.spacing, 0.1, 5.0);
    interface.addSlider("GRID SIZE", grid_.size, 1.0, 20.0);
    interface.addSlider("GRID THICKNESS", grid_.line_thickness, 0.002, 0.05);
    interface.addSlider("GRID OPACITY", grid_.line_opacity, 0.05, 1.0);
    interface.addSlider("PARTICLE COUNT", particles_.count, 1, 1'000'000);
    interface.addSlider("PARTICLE SPEED", particles_.speed, 0.1, 6.0);
    interface.addSlider("PARTICLE SIZE", particles_.size, 1.0, 12.0);
    interface.addToggle("TRAILS", particles_.trails_enabled);
    interface.addSlider("MASS", black_hole_.mass, 0.1, 10.0);
    interface.addSlider("SPIN", black_hole_.spin, -1.0, 1.0);
    interface.addSlider("EVENT HORIZON", black_hole_.event_horizon, 0.05, 0.8);
    interface.addToggle("GRID", show_grid_);
    interface.setHeatmapLegend("PARTICLE HEAT");
    interface.addButton("RESET BLACK HOLE", [this] {
        black_hole_.mass = 1.0;
        black_hole_.spin = 0.5;
        black_hole_.event_horizon = 0.24;
        black_hole_.velocity = {};
    });
}

void CelestialScene::update(double delta_seconds) {
    particles_.update(delta_seconds);
    physics::advanceCelestialSystem(black_hole_, planets_, delta_seconds);
    if (planets_.empty()) selected_planet_ = 0;
    else if (selected_planet_ >= planets_.size()) selected_planet_ = planets_.size() - 1;
}

void CelestialScene::draw(render::Renderer& renderer, const render::Camera& camera,
                          int width, int height) {
    renderer.draw(black_hole_, grid_, show_grid_, particles_, planets_, camera, width, height);
}

} // namespace opengl_sim::app
