#ifndef GENERATOR_H
#define GENERATOR_H

#include <random>
#include "Common.hpp"

/**
 * Generates memory access patterns according to specified distribution
 */
class MemoryAccessGenerator {
private:
    AccessPattern pattern_;             // Type of access pattern to generate
    std::mt19937 rng_;                  // Random number generator
    double memory_space_size_;          // Total memory size to generate accesses for

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
};

#endif // GENERATOR_H