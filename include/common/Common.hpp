#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <sstream>
#include <random>

/**
 * Defines the type of memory operation that can be performed
 */
enum class OperationType {
    READ,   // Read operation
    WRITE,  // Write operation
    END //Ene operation
};

/**
 * Defines different memory access patterns for simulation
 */
enum class AccessPattern {
    UNIFORM,            // Uniform random access across all memory
    SKEWED_70_20_10     // 70% access to 10% memory, 20% to next 20%, 10% to rest
};

/**
 * Represents different memory layers in the tiered memory system
 */
enum class PageLayer {
    NUMA_LOCAL,     // Local NUMA node
    NUMA_REMOTE,    // Remote NUMA node
    PMEM            // Persistent memory
};

/**
 * Stream operator for PageLayer enum to enable string conversion
 * @param os Output stream
 * @param layer PageLayer to convert
 * @return Modified output stream
 */
inline std::ostream& operator<<(std::ostream& os, const PageLayer& layer) {
    switch (layer) {
    case PageLayer::NUMA_LOCAL:
        os << "NUMA_LOCAL";
        break;
    case PageLayer::NUMA_REMOTE:
        os << "NUMA_REMOTE";
        break;
    case PageLayer::PMEM:
        os << "PMEM";
        break;
    }
    return os;
}

/**
 * Configuration structure for server memory tiers
 */
struct ServerMemoryConfig {
    size_t num_tiers;
    size_t local_numa_size;   // Size of local NUMA memory
    size_t remote_numa_size;  // Size of remote NUMA memory
    size_t pmem_size;         // Size of persistent memory
};

struct PolicyConfig {
    size_t hot_access_cnt;
    size_t cold_access_interval;
};

/**
 * Generates memory access patterns according to specified distribution
 */
class MemoryAccessGenerator {
private:
    AccessPattern pattern_;     // Type of access pattern to generate
    std::mt19937 rng_;          // Random number generator
    double memory_size_;        // Total memory size to generate accesses for

public:
    /**
     * Constructor for memory access pattern generator
     * @param pattern Access pattern type to use
     * @param memory_size Total memory size for generating offsets
     */
    MemoryAccessGenerator(AccessPattern pattern, double memory_size)
        : pattern_(pattern), memory_size_(memory_size) {
        std::random_device rd;
        rng_ = std::mt19937(rd());
    }

    /**
     * Generates a memory offset according to the specified access pattern
     * @return Generated memory offset
     */
    size_t generateOffset() {
        std::uniform_real_distribution<double> uniform(0, 1.0);

        switch (pattern_) {
        case AccessPattern::UNIFORM: {
            std::uniform_int_distribution<size_t> dist(0, memory_size_ - 1);
            return dist(rng_);
        }

        case AccessPattern::SKEWED_70_20_10: {
            double rand = uniform(rng_);
            if (rand < 0.7) {
                // 70% of accesses go to first 10% of memory (hot pages)
                std::uniform_int_distribution<size_t> hot(0, memory_size_ * 0.1 - 1);
                return hot(rng_);
            }
            else if (rand < 0.9) {
                // 20% of accesses go to next 20% of memory (warm pages)
                std::uniform_int_distribution<size_t> medium(memory_size_ * 0.1, memory_size_ * 0.3 - 1);
                return medium(rng_);
            }
            else {
                // 10% of accesses go to remaining 70% of memory (cold pages)
                std::uniform_int_distribution<size_t> cold(memory_size_ * 0.3, memory_size_ - 1);
                return cold(rng_);
            }
        }
        }
        return 0;
    }
};

/**
 * Represents a message from a client containing memory operation details
 */
struct ClientMessage {
    size_t client_id;        // Unique client identifier for the client
    size_t offset;           // Page identifier offset to access
    OperationType op_type;   // Type of operation to perform

    /**
     * Constructor for client message
     * @param id Client identifier
     * @param off Page identifier offset
     * @param op Operation type
     */
    ClientMessage(size_t id, size_t off, OperationType op)
        : client_id(id), offset(off), op_type(op) {
    }

    /**
     * Converts message to string representation for logging/debugging
     * @return String representation of the message
     */
    std::string toString() const {
        std::stringstream ss;
        ss << "Client " << client_id
            << ", Offset: " << offset
            << ", Operation: " << (op_type == OperationType::READ ? "READ" : "WRITE");
        return ss.str();
    }
};

/**
 * Represents a memory movement request between different memory tiers
 */
struct MemMoveReq {
    size_t page_id;     // Page identifier to move
    PageLayer layer_id; // Destination layer for the page

    /**
     * Constructor for memory movement request
     * @param page_id   Page identifier
     * @param layer_id  Destination layer
     */
    MemMoveReq(size_t page_id, PageLayer layer_id)
        : page_id(page_id), layer_id(layer_id) {
    }

    /**
     * Converts request to string representation for logging/debugging
     * @return String representation of the request
     */
    std::string toString() const {
        std::stringstream ss;
        ss << "Page " << page_id
            << " to Layer: " << layer_id;
        return ss.str();
    }
};

#endif // COMMON_H