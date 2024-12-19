#include "ConfigParser.hpp"
#include "Logger.hpp"

/**
 * Constructor: initializes command line options with default values
 */
ConfigParser::ConfigParser()
    : options_("MemoryTiering", "Concurrent Ring Buffer Demonstration"),
    buffer_size_(10),
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
        ("s,mem-sizes", "Memory size for each tiering",
            cxxopts::value<std::vector<size_t>>())
        ("h,help", "Print usage information");
}

/**
 * Parses command line arguments and validates configuration
 * @param argc Argument count
 * @param argv Argument values
 * @return true if parsing successful, false otherwise
 */
bool ConfigParser::parse(int argc, char* argv[]) {
    auto result = options_.parse(argc, argv);

    // Handle help request
    if (result.count("help")) {
        std::cout << options_.help() << std::endl;
        help_requested_ = true;
        return false;
    }

    // Parse basic parameters
    buffer_size_ = result["buffer-size"].as<size_t>();
    message_count_ = result["messages"].as<size_t>();

    // Parse client configurations
    auto patterns = result["patterns"].as<std::vector<std::string>>();
    auto client_addr_space_sizes = result["client-addr-space-sizes"].as<std::vector<size_t>>();
    auto mem_sizes = result["mem-sizes"].as<std::vector<size_t>>();

    // Validate client configuration counts match
    if (patterns.size() != client_addr_space_sizes.size()) {
        LOG_ERROR("Error: Number of patterns must match number of memory sizes");
        return false;
    }

    // Process each client's configuration
    for (size_t i = 0; i < patterns.size(); i++) {
        ClientConfig config;
        config.addr_space_size = client_addr_space_sizes[i];

        // Parse access pattern type
        if (patterns[i] == "uniform") {
            config.pattern = AccessPattern::UNIFORM;
        }
        else if (patterns[i] == "skewed") {
            config.pattern = AccessPattern::SKEWED_70_20_10;
        }
        else {
            LOG_ERROR("Invalid pattern type: " << patterns[i]);
            return false;
        }

        client_configs_.push_back(config);
    }

    // Parse server memory configuration
    // Expected order: local_numa_size, remote_numa_size, pmem_size
    if (mem_sizes.size() != 3) {
        LOG_ERROR("Error: Server configuration must have exactly three memory sizes");
        return false;
    }

    server_memory_config_.local_numa_size = mem_sizes[0];
    server_memory_config_.remote_numa_size = mem_sizes[1];
    server_memory_config_.pmem_size = mem_sizes[2];

    return true;
}