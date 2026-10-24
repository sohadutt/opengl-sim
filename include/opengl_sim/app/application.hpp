#pragma once

#include "opengl_sim/core/config.hpp"
#include "opengl_sim/core/logger.hpp"

namespace opengl_sim::app {

class Application {
public:
    Application(const core::Settings& settings, core::Logger& logger) noexcept;

    [[nodiscard]] int run();

private:
    const core::Settings& settings_;
    core::Logger& logger_;
};

} // namespace opengl_sim::app
