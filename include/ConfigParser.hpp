#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>
#include <cxxopts.hpp>

#include "Common.h"

class ConfigParser {
public:
    struct ClientConfig {
        AccessPattern pattern;
        int memory_size;
    };

    ConfigParser();
    bool parse(int argc, char* argv[]);

    // Getters for configuration
    int getBufferSize() const { return buffer_size_; }
    int getMessageCount() const { return message_count_; }
    const std::vector<ClientConfig>& getClientConfigs() const { return client_configs_; }
    bool isHelpRequested() const { return help_requested_; }

private:
    cxxopts::Options options_;
    int buffer_size_;
    int message_count_;
    std::vector<ClientConfig> client_configs_;
    bool help_requested_;
};

#endif // CONFIGPARSER_H
