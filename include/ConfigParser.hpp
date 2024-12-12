#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>
#include <cxxopts.hpp>

class ConfigParser {
public:
    ConfigParser();
    bool parse(int argc, char* argv[]);

    // Getters for configuration
    int getBufferSize() const { return buffer_size_; }
    int getMessageCount() const { return message_count_; }
    bool isHelpRequested() const { return help_requested_; }

private:
    cxxopts::Options options_;
    int buffer_size_;
    int message_count_;
    bool help_requested_;
};

#endif // CONFIGPARSER_H
