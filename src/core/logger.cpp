#include "opengl_sim/core/logger.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace opengl_sim::core {
namespace {

std::string lowercase(std::string_view text) {
  std::string result{text};
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return result;
}

} // namespace

Logger::Logger(LogLevel level) noexcept : level_(level) {}

void Logger::setLevel(LogLevel level) noexcept {
  level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::level() const noexcept {
  return level_.load(std::memory_order_relaxed);
}

void Logger::setFile(const std::filesystem::path &path) {
  std::lock_guard lock{output_mutex_};
  file_.close();
  file_.clear();

  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
  }
  if (error) {
    std::cerr << "[ERROR] Could not create log directory '"
              << path.parent_path().string() << "': " << error.message() << '\n';
    return;
  }

  file_.open(path, std::ios::out | std::ios::app);
  if (!file_) {
    std::cerr << "[ERROR] Could not open log file '" << path.string() << "'.\n";
  }
}

void Logger::log(LogLevel message_level, std::string_view message) const {
  const auto configured_level = level();
  if (configured_level == LogLevel::off || message_level < configured_level) {
    return;
  }

  const auto now = std::time(nullptr);
  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &now);
#else
  localtime_r(&now, &local_time);
#endif

  std::ostringstream entry;
  entry << '[' << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "] ["
        << levelName(message_level) << "] " << message;

  std::lock_guard lock{output_mutex_};
  auto &output = message_level >= LogLevel::warning ? std::cerr : std::clog;
  output << entry.str() << '\n';
  if (file_.is_open()) {
    file_ << entry.str() << '\n';
    file_.flush();
  }
}

void Logger::trace(std::string_view message) const {
  log(LogLevel::trace, message);
}
void Logger::debug(std::string_view message) const {
  log(LogLevel::debug, message);
}
void Logger::info(std::string_view message) const {
  log(LogLevel::info, message);
}
void Logger::warning(std::string_view message) const {
  log(LogLevel::warning, message);
}
void Logger::error(std::string_view message) const {
  log(LogLevel::error, message);
}
void Logger::critical(std::string_view message) const {
  log(LogLevel::critical, message);
}

LogLevel Logger::parseLevel(std::string_view text) {
  const auto value = lowercase(text);
  if (value == "trace")
    return LogLevel::trace;
  if (value == "debug")
    return LogLevel::debug;
  if (value == "info")
    return LogLevel::info;
  if (value == "warning" || value == "warn")
    return LogLevel::warning;
  if (value == "error")
    return LogLevel::error;
  if (value == "critical")
    return LogLevel::critical;
  if (value == "off")
    return LogLevel::off;
  throw std::invalid_argument{"Unknown log level: " + std::string{text}};
}

std::string_view Logger::levelName(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::trace:
    return "TRACE";
  case LogLevel::debug:
    return "DEBUG";
  case LogLevel::info:
    return "INFO";
  case LogLevel::warning:
    return "WARN";
  case LogLevel::error:
    return "ERROR";
  case LogLevel::critical:
    return "CRITICAL";
  case LogLevel::off:
    return "OFF";
  }
  return "UNKNOWN";
}

} // namespace opengl_sim::core
