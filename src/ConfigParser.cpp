#include "ConfigParser.hpp"
#include <iostream>

ConfigParser::ConfigParser()
    : options_("MemoryTiering", "Concurrent Ring Buffer Demonstration"),
    buffer_size_(10),  // Default values
    message_count_(100),
    help_requested_(false) {

    options_.add_options()
        ("b,buffer-size", "Size of ring buffer",
            cxxopts::value<size_t>()->default_value("10"))
        ("m,messages", "Number of messages per client",
            cxxopts::value<size_t>()->default_value("100"))
        ("p,patterns", "Memory access patterns for each client (uniform/skewed)",
            cxxopts::value<std::vector<std::string>>())
        ("c,client-addr-space-sizes", "Address space size for each client",
            cxxopts::value<std::vector<size_t>>())
        ("m,mem-sizes", "Memory size for each tiering",
            cxxopts::value<std::vector<size_t>>())
        ("h,help", "Print usage information");
}

bool ConfigParser::parse(int argc, char* argv[]) {
    auto result = options_.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options_.help() << std::endl;
        help_requested_ = true;
        return false;
    }

    buffer_size_ = result["buffer-size"].as<size_t>();
    message_count_ = result["messages"].as<size_t>();
    auto patterns = result["patterns"].as<std::vector<std::string>>();
    auto client_addr_space_sizes = result["client-addr-space-sizes"].as<std::vector<size_t>>();
    auto mem_sizes = result["mem-sizes"].as<std::vector<size_t>>();

    if (patterns.size() != client_addr_space_sizes.size()) {
        std::cerr << "Error: Number of patterns must match number of memory sizes" << std::endl;
        return false;
    }

    for (size_t i = 0; i < patterns.size(); i++) {
        ClientConfig config;
        config.addr_space_size = client_addr_space_sizes[i];

        if (patterns[i] == "uniform") {
            config.pattern = AccessPattern::UNIFORM;
        }
        else if (patterns[i] == "skewed") {
            config.pattern = AccessPattern::SKEWED_70_20_10;
        }
        else {
            std::cerr << "Invalid pattern type: " << patterns[i] << std::endl;
            return false;
        }

        client_configs_.push_back(config);
    }

    // Server configuration, order: local_numa_size, remote_numa_size, pmem_size
    if (mem_sizes.size() != 3) {
        std::cerr << "Error: Server configuration must have exactly three memory size" << std::endl;
        return false;
    }

    server_memory_config_.local_numa_size = mem_sizes[0];
    server_memory_config_.remote_numa_size = mem_sizes[1];
    server_memory_config_.pmem_size = mem_sizes[2];


    return true;
}