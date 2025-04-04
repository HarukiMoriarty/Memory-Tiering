#ifndef LOGGER_H
#define LOGGER_H

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <string>

class Logger {
public:
  enum LogLevel { trace, debug, info, warning, error, fatal };

  static Logger& getInstance();
  void init();
  void setLogLevel(LogLevel level);
  static LogLevel getLogLevelFromEnv();

private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};

// Simple macros that use BOOST_LOG_TRIVIAL directly
#define LOG_TRACE(msg) BOOST_LOG_TRIVIAL(trace) << msg
#define LOG_DEBUG(msg) BOOST_LOG_TRIVIAL(debug) << msg
#define LOG_INFO(msg) BOOST_LOG_TRIVIAL(info) << msg
#define LOG_WARN(msg) BOOST_LOG_TRIVIAL(warning) << msg
#define LOG_ERROR(msg) BOOST_LOG_TRIVIAL(error) << msg
#define LOG_FATAL(msg) BOOST_LOG_TRIVIAL(fatal) << msg

#endif