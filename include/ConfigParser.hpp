#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>
#include <cxxopts.hpp>
#include <iostream>

#include "Common.hpp"

class ConfigParser {
public:
    struct ClientConfig {
        AccessPattern pattern;
        size_t addr_space_size;
    };

    ConfigParser();
    bool parse(int argc, char* argv[]);

    // Getters for configuration
    size_t getBufferSize() const { return buffer_size_; }
    size_t getMessageCount() const { return message_count_; }
    const std::vector<ClientConfig>& getClientConfigs() const { return client_configs_; }
    const ServerMemoryConfig& getServerMemoryConfig() const { return server_memory_config_; }

    bool isHelpRequested() const { return help_requested_; }


private:
    cxxopts::Options options_;
    size_t buffer_size_;
    size_t message_count_;
    std::vector<ClientConfig> client_configs_;
    ServerMemoryConfig server_memory_config_;
    bool help_requested_;
};

#endif // CONFIGPARSER_H
