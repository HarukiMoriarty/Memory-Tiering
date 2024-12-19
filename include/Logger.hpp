#ifndef LOGGER_H
#define LOGGER_H

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <memory>
#include <string>
#include <sstream>

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void init(const std::string& logFile = "app_%N.log");

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

// Simple macros that use BOOST_LOG_TRIVIAL directly
#define LOG_TRACE(msg) BOOST_LOG_TRIVIAL(trace) << msg
#define LOG_DEBUG(msg) BOOST_LOG_TRIVIAL(debug) << msg
#define LOG_INFO(msg)  BOOST_LOG_TRIVIAL(info) << msg
#define LOG_WARN(msg)  BOOST_LOG_TRIVIAL(warning) << msg
#define LOG_ERROR(msg) BOOST_LOG_TRIVIAL(error) << msg
#define LOG_FATAL(msg) BOOST_LOG_TRIVIAL(fatal) << msg

#endif