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
  double memory_space_size_; // Total memory size to generate accesses for

  // Distribution for read/write type
  std::uniform_real_distribution<double> type_dis{0.0, 1.0};

  // Distribution for skewed access
  std::unordered_map<size_t, size_t> hot_pages_;
  std::unordered_map<size_t, size_t> warm_pages_;
  std::unordered_map<size_t, size_t> cold_pages_;

public:
  /**
   * Constructor for memory access pattern generator
   */
  MemoryAccessGenerator(AccessPattern pattern, double memory_space_size);

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