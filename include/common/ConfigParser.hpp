#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>
#include <sstream>
#include <vector>
#include <iostream>

#include "cxxopts.hpp"
#include "Common.hpp"

class ConfigParser {
public:
    ConfigParser();
    bool parse(int argc, char* argv[]);

    // Getters for configuration.
    size_t getBufferSize() const { return buffer_size_; }
    size_t getMessageCount() const { return message_count_; }
    const std::vector<ClientConfig>& getClientConfigs() const { return client_configs_; }
    const ServerMemoryConfig& getServerMemoryConfig() const { return server_memory_config_; }
    const PolicyConfig& getPolicyConfig() const { return policy_config_; }
    const std::string& getLatencyOutputFile() const { return cdf_output_file_; }
    const double& getRwRatio() const { return rw_ratio_; }

    bool isHelpRequested() const { return help_requested_; }

private:
    bool parseBasicConfig(const cxxopts::ParseResult& result);
    bool parseServerConfig(const cxxopts::ParseResult& result);
    bool parseClientConfigs(const std::vector<std::string>& patterns,
        const std::vector<std::string>& tier_sizes);
    bool validateMemoryConfiguration();
    void printConfig() const;

    static std::vector<std::string> split(const std::string& s, char delim);
    static std::string getTierName(size_t tier_idx, size_t num_tiers);

    cxxopts::Options options_;
    size_t buffer_size_;
    size_t message_count_;
    std::vector<ClientConfig> client_configs_;
    ServerMemoryConfig server_memory_config_;
    double rw_ratio_;
    PolicyConfig policy_config_;
    std::string cdf_output_file_;
    bool help_requested_;
};

#endif // CONFIGPARSER_H
