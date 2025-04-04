#include "Logger.hpp"
#include <algorithm>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <cctype>
#include <cstdlib>
#include <string>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

Logger& Logger::getInstance() {
  static Logger instance;
  return instance;
}

Logger::LogLevel Logger::getLogLevelFromEnv() {
  const char* level = std::getenv("LOG_LEVEL");
  if (!level) {
    return LogLevel::info; // Default level set to info
  }

  std::string levelStr = level;
  std::transform(levelStr.begin(), levelStr.end(), levelStr.begin(), ::tolower);

  if (levelStr == "trace")
    return LogLevel::trace;
  if (levelStr == "debug")
    return LogLevel::debug;
  if (levelStr == "info")
    return LogLevel::info;
  if (levelStr == "warning" || levelStr == "warn")
    return LogLevel::warning;
  if (levelStr == "error")
    return LogLevel::error;
  if (levelStr == "fatal")
    return LogLevel::fatal;

  return LogLevel::info; // Default to info if not recognized
}

void Logger::setLogLevel(LogLevel level) {
  logging::core::get()->set_filter(
    logging::trivial::severity >=
    static_cast<logging::trivial::severity_level>(level));
}

void Logger::init() {
  // We only use console logging
  logging::add_console_log(
    std::cout, keywords::format = "[%TimeStamp%] [%Severity%] %Message%");

  logging::add_common_attributes();

  // Set initial log level from environment
  setLogLevel(getLogLevelFromEnv());
}