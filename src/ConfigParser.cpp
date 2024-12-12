#include "ConfigParser.hpp"
#include <iostream>

ConfigParser::ConfigParser()
    : options_("MyRingBufferProgram", "Concurrent Ring Buffer Demonstration"),
    buffer_size_(10),  // Default values
    message_count_(100),
    help_requested_(false) {

    options_.add_options()
        ("b,buffer-size", "Size of ring buffer",
            cxxopts::value<int>()->default_value("10"))
        ("m,messages", "Number of messages to send",
            cxxopts::value<int>()->default_value("100"))
        ("h,help", "Print usage information");
}

bool ConfigParser::parse(int argc, char* argv[]) {
    auto result = options_.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options_.help() << std::endl;
        help_requested_ = true;
        return false;
    }

    buffer_size_ = result["buffer-size"].as<int>();
    message_count_ = result["messages"].as<int>();

    return true;
}