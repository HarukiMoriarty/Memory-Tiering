#ifndef COMMON_H
#define COMMON_H

#include <sstream>
#include <string>

#include "Logger.hpp"

/**
 * Defines the type of memory operation that can be performed
 */
enum class OperationType {
  READ,  // Read operation
  WRITE, // Write operation
  END    // End operation
};

/**
 * Defines different memory access patterns for simulation
 */
enum class AccessPattern {
  UNIFORM,        // Uniform random access across all memory
  SKEWED_70_20_10 // 70% access to 10% memory, 20% to next 20%, 10% to rest
};

/**
 * Represents different memory layers in the tiered memory system
 */
enum class PageLayer {
  NUMA_LOCAL,  // Local NUMA node
  NUMA_REMOTE, // Remote NUMA node
  PMEM         // Persistent memory
};

/**
 * Stream operator for PageLayer enum to enable string conversion
 * @param os Output stream
 * @param layer PageLayer to convert
 * @return Modified output stream
 */
inline std::ostream &operator<<(std::ostream &os, const PageLayer &layer) {
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
 * Configuration structure for client access pattern and loading setting
 */
struct ClientConfig {
  AccessPattern pattern;
  std::vector<size_t> tier_sizes;
};

struct LayerInfo {
  size_t count;
  size_t capacity;

  LayerInfo() : count(0), capacity(0) {}

  inline bool isFull() const { return count >= capacity; }
};

/**
 * Configuration structure for server memory tiers
 */
struct ServerMemoryConfig {
  size_t num_tiers;
  LayerInfo local_numa;
  LayerInfo remote_numa;
  LayerInfo pmem;
};

struct PolicyConfig {
  size_t hot_access_interval;
  size_t cold_access_interval;
};

/**
 * Represents a message from a client containing memory operation details
 */
struct ClientMessage {
  size_t client_id;      // Unique client identifier for the client
  size_t pid;            // Page identifier to access
  size_t p_offset;       // Access offset inside a page
  OperationType op_type; // Type of operation to perform

  /**
   * Constructor for client message
   * @param id Client identifier
   * @param pid Page identifier
   * @param p_offset Offset in one page
   * @param op Operation type
   */
  ClientMessage(size_t client_id, size_t pid, size_t p_offset,
                OperationType op_type)
      : client_id(client_id), pid(pid), p_offset(p_offset), op_type(op_type) {}

  /**
   * Converts message to string representation for logging/debugging
   * @return String representation of the message
   */
  std::string toString() const {
    std::stringstream ss;
    ss << "Client " << client_id << ", PageId: " << pid
       << ", Offset: " << p_offset << ", Operation: "
       << (op_type == OperationType::READ ? "READ" : "WRITE");
    return ss.str();
  }
};

#endif // COMMON_H