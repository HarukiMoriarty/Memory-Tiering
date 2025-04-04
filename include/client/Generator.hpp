#ifndef GENERATOR_H
#define GENERATOR_H

#include "Common.hpp"

#include <random>
#include <unordered_map>

/**
 * Generates memory access patterns according to specified distribution
 */
class MemoryAccessGenerator {
private:
  AccessPattern pattern_;    // Type of access pattern to generate
  std::mt19937 rng_;         // Random number generator
  size_t memory_space_size_; // Total memory size to generate accesses for

  // Operation type
  std::uniform_real_distribution<double> type_dis{ 0.0, 1.0 };

  // UNIFORM
  std::uniform_int_distribution<size_t> uniform_dist_;

  // HOT
  std::unordered_map<size_t, size_t> hot_pages_;
  std::uniform_int_distribution<size_t> hot_dist_;

  // ZIPF
  std::vector<double> zipf_prob_;
  std::discrete_distribution<size_t> zipf_dist_;

public:
  /**
   * Constructor for memory access pattern generator
   */
  MemoryAccessGenerator(AccessPattern pattern, size_t memory_space_size);

  /**
   * Generates a page identifier according to the specified access pattern
   */
  size_t generatePid();

  /**
   * Generates a operation read/write type
   */
  OperationType generateType(double rw_ratio);
};

#endif // GENERATOR_H