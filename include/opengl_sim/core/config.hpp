#pragma once

#include "opengl_sim/core/logger.hpp"

#include <filesystem>
#include <string>

namespace opengl_sim::core {

struct Settings {
    LogLevel log_level = LogLevel::info;
    std::filesystem::path log_file = "logs/simulator.log";

    int window_width = 1280;
    int window_height = 720;
    std::string window_title = "Gravity and Light Simulator";
    bool vertical_sync = true;

};

[[nodiscard]] Settings loadSettings(const std::filesystem::path& path, Logger& logger);

} // namespace opengl_sim::core
