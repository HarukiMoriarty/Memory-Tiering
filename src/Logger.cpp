#include "Logger.hpp"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

void Logger::init(const std::string& logFile) {
    // Setup console logging
    logging::add_console_log(
        std::cout,
        keywords::format = "[%TimeStamp%] [%Severity%] %Message%"
    );

    // Setup file logging
    logging::add_file_log(
        keywords::file_name = logFile,
        keywords::rotation_size = 10 * 1024 * 1024,
        keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
    );

    logging::add_common_attributes();
}