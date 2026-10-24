#include "opengl_sim/core/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace opengl_sim::core {
namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool parseBool(const std::string& text) {
    const auto value = lowercase(text);
    if (value == "true" || value == "yes" || value == "on" || value == "1") return true;
    if (value == "false" || value == "no" || value == "off" || value == "0") return false;
    throw std::invalid_argument{"Expected true or false"};
}

int parseInt(const std::string& text) {
    std::size_t parsed_characters = 0;
    const int value = std::stoi(text, &parsed_characters);
    if (parsed_characters != text.size()) {
        throw std::invalid_argument{"Unexpected characters after integer"};
    }
    return value;
}

} // namespace

Settings loadSettings(const std::filesystem::path& path, Logger& logger) {
    Settings settings;
    std::ifstream file{path};
    if (!file) {
        logger.warning("Could not open settings file '" + path.string() + "'; using defaults.");
        return settings;
    }

    logger.info("Loading settings from " + path.string());
    std::string section;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            logger.warning("Ignoring malformed settings line " + std::to_string(line_number));
            continue;
        }

        const auto key = section + "." + lowercase(trim(line.substr(0, equals)));
        auto value = trim(line.substr(equals + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        try {
            if (key == "app.log_level") {
                settings.log_level = Logger::parseLevel(value);
            } else if (key == "app.log_file") {
                settings.log_file = value;
            } else if (key == "window.width") {
                settings.window_width = parseInt(value);
            } else if (key == "window.height") {
                settings.window_height = parseInt(value);
            } else if (key == "window.title") {
                settings.window_title = value;
            } else if (key == "window.vertical_sync") {
                settings.vertical_sync = parseBool(value);
            } else {
                logger.warning("Unknown setting '" + key + "' on line " + std::to_string(line_number));
            }
        } catch (const std::exception& exception) {
            logger.warning("Invalid value for '" + key + "' on line " +
                           std::to_string(line_number) + ": " + exception.what());
        }
    }

    if (settings.window_width <= 0) {
        logger.warning("window.width must be positive; using 1280.");
        settings.window_width = 1280;
    }
    if (settings.window_height <= 0) {
        logger.warning("window.height must be positive; using 720.");
        settings.window_height = 720;
    }
    return settings;
}

} // namespace opengl_sim::core
