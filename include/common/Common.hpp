#ifndef COMMON_H
#define COMMON_H

#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "Logger.hpp"

// ========================== Shared Enums and Structs ==========================

/**
 * Defines the type of memory operation that can be performed
 */
enum class OperationType
{
  READ,  // Read operation
  WRITE, // Write operation
  END    // End operation
};

/**
 * Defines different memory access patterns for simulation
 */
enum class AccessPattern
{
  UNIFORM, // Uniform random access across all memory
  HOT,     // Access concentrated (20%) on randomly distributed memory
  ZIPFIAN  // Access follows Zipf's law distribution
};

/**
 * Represents different memory layers in the tiered memory system
 */
enum class PageLayer
{
  NUMA_LOCAL,  // Local NUMA node
  NUMA_REMOTE, // Remote NUMA node
  PMEM         // Persistent memory
};

inline std::ostream& operator<<(std::ostream& os, const PageLayer& layer)
{
  switch (layer)
  {
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

struct LayerInfo
{
  size_t count;
  size_t capacity;

  LayerInfo() : count(0), capacity(0) {}

  inline bool isFull() const { return count >= capacity; }
};

// ========================== Client-Side Structures ==========================

/**
 * Configuration structure for client access pattern and loading settings
 */
struct ClientConfig
{
  AccessPattern pattern;
  std::vector<size_t> tier_sizes;
  double zipf_s;
};

/**
 * Represents a message from a client containing memory operation details
 */
struct ClientMessage
{
  size_t client_id;      // Unique client identifier for the client
  size_t pid;            // Page identifier to access
  size_t p_offset;       // Access offset inside a page
  OperationType op_type; // Type of operation to perform

  ClientMessage(size_t client_id, size_t pid, size_t p_offset, OperationType op_type)
    : client_id(client_id), pid(pid), p_offset(p_offset), op_type(op_type) {
  }

  std::string toString() const
  {
    std::stringstream ss;
    ss << "Client " << client_id << ", PageId: " << pid << ", Offset: " << p_offset
      << ", Operation: " << (op_type == OperationType::READ ? "READ" : "WRITE");
    return ss.str();
  }
};

// ========================== Server-Side Structures ==========================

/**
 * Configuration structure for server memory tiers
 */
struct ServerMemoryConfig
{
  size_t num_tiers;
  LayerInfo local_numa;
  LayerInfo remote_numa;
  LayerInfo pmem;
};

/**
 * Policy configuration parameters for page access and migration
 */
struct LRUPolicyConfig
{
  size_t hot_threshold_ms;
  size_t cold_threshold_ms;
};

struct FrequencyPolicyConfig
{
  size_t hot_access_count;
  size_t cold_access_count;
};

struct HybridPolicyConfig
{
  size_t hot_threshold_ms;
  size_t cold_threshold_ms;
  size_t hot_access_count;
  size_t cold_access_count;
  double weight_recency;
  double weight_frequency;
};

using PolicyVariant = std::variant<LRUPolicyConfig, FrequencyPolicyConfig, HybridPolicyConfig>;

struct PolicyConfig
{
  PolicyVariant config;
  std::string policy_type; // "lru", "frequency", "hybrid"
  size_t scan_interval;
};

#endif // COMMON_H