#include "opengl_sim/app/application.hpp"
#include "opengl_sim/core/config.hpp"
#include "opengl_sim/core/logger.hpp"

#include <cstdlib>
#include <filesystem>
#include <exception>

#ifndef SIMULATOR_CONFIG_FILE
#define SIMULATOR_CONFIG_FILE "config/simulator.conf"
#endif

int main() {
    opengl_sim::core::Logger logger;
    const auto project_root =
        std::filesystem::path{SIMULATOR_CONFIG_FILE}.parent_path().parent_path();
    logger.setFile(project_root / "logs/simulator.log");

    try {
        const auto settings = opengl_sim::core::loadSettings(
            std::filesystem::path{SIMULATOR_CONFIG_FILE}, logger);
        logger.setLevel(settings.log_level);
        auto log_file = settings.log_file;
        if (log_file.is_relative()) {
            log_file = project_root / log_file;
        }
        logger.setFile(log_file);

        opengl_sim::app::Application application{settings, logger};
        return application.run();
    } catch (const std::exception& exception) {
        logger.critical(exception.what());
        return EXIT_FAILURE;
    }
}
