#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace opengl_sim::core {

enum class LogLevel {
    trace,
    debug,
    info,
    warning,
    error,
    critical,
    off,
};

class Logger {
public:
    explicit Logger(LogLevel level = LogLevel::info) noexcept;

    void setLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;
    void setFile(const std::filesystem::path& path);
    void log(LogLevel level, std::string_view message) const;

    void trace(std::string_view message) const;
    void debug(std::string_view message) const;
    void info(std::string_view message) const;
    void warning(std::string_view message) const;
    void error(std::string_view message) const;
    void critical(std::string_view message) const;

    [[nodiscard]] static LogLevel parseLevel(std::string_view text);
    [[nodiscard]] static std::string_view levelName(LogLevel level) noexcept;

private:
    std::atomic<LogLevel> level_;
    mutable std::mutex output_mutex_;
    mutable std::ofstream file_;
};

} // namespace opengl_sim::core
