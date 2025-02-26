#ifndef GENERATOR_H
#define GENERATOR_H

#include "Common.hpp"
#include <random>

/**
 * Generates memory access patterns according to specified distribution
 */
class MemoryAccessGenerator {
private:
  AccessPattern pattern_;    // Type of access pattern to generate
  std::mt19937 rng_;         // Random number generator
  double memory_space_size_; // Total memory size to generate accesses for
  std::uniform_real_distribution<double> type_dis{0.0, 1.0};

  // index -> page id
  std::unordered_map<size_t, size_t> hot_pages_;
  std::unordered_map<size_t, size_t> warm_pages_;
  std::unordered_map<size_t, size_t> cold_pages_;

public:
  /**
   * Constructor for memory access pattern generator
   * @param pattern Access pattern type to use
   * @param memory_space_size Total memory size for generating offsets
   */
  MemoryAccessGenerator(AccessPattern pattern, double memory_space_size);

  /**
   * Generates a page identifier according to the specified access pattern
   * @return Generated page identifier
   */
  size_t generatePid();

  OperationType generateType(double rw_ratio);
};

#endif // GENERATOR_H