#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Common.hpp"
#include "cxxopts.hpp"

class ConfigParser {
public:
  ConfigParser();
  bool parse(int argc, char *argv[]);

  // Getters for configuration.
  size_t getBufferSize() const { return buffer_size_; }
  const size_t getRunningTime() const { return running_time_; }
  const std::vector<ClientConfig> &getClientConfigs() const {
    return client_configs_;
  }
  const ServerMemoryConfig &getServerMemoryConfig() const {
    return server_memory_config_;
  }
  const PolicyConfig &getPolicyConfig() const { return policy_config_; }
  const std::string &getLatencyOutputFile() const { return cdf_output_file_; }
  const double &getRwRatio() const { return rw_ratio_; }

  bool isHelpRequested() const { return help_requested_; }

private:
  bool _parseBasicConfig(const cxxopts::ParseResult &result);
  bool _parseServerConfig(const cxxopts::ParseResult &result);
  bool _parseClientConfigs(const cxxopts::ParseResult &result);
  bool _validateMemoryConfiguration();
  void _printConfig() const;

  static std::vector<std::string> _split(const std::string &s, char delim);
  static std::string _getTierName(size_t tier_idx, size_t num_tiers);

  cxxopts::Options options_;
  bool help_requested_;

  size_t buffer_size_;
  size_t running_time_;
  double rw_ratio_;

  std::vector<ClientConfig> client_configs_;
  ServerMemoryConfig server_memory_config_;
  PolicyConfig policy_config_;

  std::string cdf_output_file_;
};

#endif // CONFIGPARSER_H
