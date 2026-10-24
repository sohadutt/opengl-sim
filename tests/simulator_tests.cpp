#include "opengl_sim/core/config.hpp"
#include "opengl_sim/core/logger.hpp"
#include "opengl_sim/physics/grid_plane.hpp"
#include "opengl_sim/physics/light_particle_field.hpp"
#include "opengl_sim/physics/planet.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int assertions = 0;

void expect(bool condition, const std::string& message) {
    ++assertions;
    if (!condition) {
        throw std::runtime_error{message};
    }
}

void expectNear(double actual, double expected, double tolerance, const std::string& message) {
    expect(std::abs(actual - expected) <= tolerance,
           message + " (expected " + std::to_string(expected) + ", got " +
               std::to_string(actual) + ")");
}

void testGridPlane() {
    opengl_sim::physics::grid_plane grid;
    grid.position = {1.0, 2.0, 3.0};
    grid.size = 4.0;
    grid.spacing = 1.0;

    const auto endpoints = grid.grid_lines();
    expect(endpoints.size() == 20, "A 4-unit grid at 1-unit spacing should have 20 endpoints");
    expect(endpoints.front() == glm::dvec3{ -1.0, 2.0, 1.0 },
           "Grid endpoints should include the translated first vertical line");
    expect(endpoints.back() == glm::dvec3{ 3.0, 2.0, 5.0 },
           "Grid endpoints should include the translated last horizontal line");

    grid.spacing = 0.0;
    bool threw = false;
    try {
        static_cast<void>(grid.grid_lines());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "Grid generation should reject zero spacing");
}

void testParticleFieldTime() {
    opengl_sim::physics::light_particle_field particles;
    particles.update(2.5);
    expectNear(particles.elapsed_time, 2.5, 1e-12, "Particle time should advance by delta time");

    particles.update(-1.0);
    expectNear(particles.elapsed_time, 2.5, 1e-12, "Negative delta time should not change particle time");

    particles.elapsed_time = 9999.5;
    particles.update(1.0);
    expectNear(particles.elapsed_time, 0.5, 1e-9, "Particle time should wrap at its cycle limit");
}

void testPlanetRotation() {
    opengl_sim::physics::planet world;
    world.update(2.0);
    expectNear(world.rotation, 0.7, 1e-12, "Planet rotation should advance at its configured rate");

    world.update(0.0);
    expectNear(world.rotation, 0.7, 1e-12, "Zero delta time should not change planet rotation");

    world.rotation = 6.2;
    world.update(1.0);
    expect(world.rotation >= 0.0 && world.rotation < 6.283185307179586,
           "Planet rotation should remain within one full turn");
}

void testSettings(const std::filesystem::path& temporary_directory) {
    using namespace opengl_sim::core;
    Logger logger{LogLevel::off};
    const auto settings_file = temporary_directory / "simulator.conf";
    {
        std::ofstream file{settings_file};
        file << "[app]\n"
             << "log_level = DEBUG\n"
             << "log_file = \"logs/test output.log\"\n"
             << "[window]\n"
             << "width = 1024 # inline comment\n"
             << "height = 600\n"
             << "title = Test Simulator\n"
             << "vertical_sync = off\n";
    }

    const auto settings = loadSettings(settings_file, logger);
    expect(settings.log_level == LogLevel::debug, "Settings should parse log levels case-insensitively");
    expect(settings.log_file == std::filesystem::path{"logs/test output.log"},
           "Settings should parse quoted paths");
    expect(settings.window_width == 1024 && settings.window_height == 600,
           "Settings should parse window dimensions");
    expect(settings.window_title == "Test Simulator", "Settings should parse the window title");
    expect(!settings.vertical_sync, "Settings should parse false boolean values");

    {
        std::ofstream file{settings_file};
        file << "[window]\nwidth = -3\nheight = 0\nvertical_sync = perhaps\n";
    }
    const auto invalid_settings = loadSettings(settings_file, logger);
    expect(invalid_settings.window_width == 1280 && invalid_settings.window_height == 720,
           "Invalid window dimensions should fall back to defaults");
    expect(invalid_settings.vertical_sync,
           "An invalid boolean should leave the default setting unchanged");

    const auto defaults = loadSettings(temporary_directory / "missing.conf", logger);
    expect(defaults.window_width == 1280 && defaults.window_height == 720,
           "A missing settings file should return default dimensions");
}

void testLogLevelParsing() {
    using opengl_sim::core::LogLevel;
    using opengl_sim::core::Logger;
    expect(Logger::parseLevel("warn") == LogLevel::warning, "Logger should accept the warn alias");
    expect(Logger::parseLevel("OFF") == LogLevel::off, "Logger should accept case-insensitive levels");

    bool threw = false;
    try {
        static_cast<void>(Logger::parseLevel("verbose"));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "Unknown log levels should be rejected");
}

} // namespace

int main() {
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary_directory = std::filesystem::temp_directory_path() /
                                     ("opengl-sim-tests-" + std::to_string(timestamp));

    try {
        std::filesystem::create_directories(temporary_directory);
        testGridPlane();
        testParticleFieldTime();
        testPlanetRotation();
        testSettings(temporary_directory);
        testLogLevelParsing();
        std::filesystem::remove_all(temporary_directory);
        std::cout << "Passed " << assertions << " assertions.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary_directory, ignored);
        std::cerr << "Test failure after " << assertions << " assertions: " << exception.what() << '\n';
        return 1;
    }
}
