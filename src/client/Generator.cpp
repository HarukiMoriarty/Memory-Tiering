#include "Generator.hpp"

MemoryAccessGenerator::MemoryAccessGenerator(AccessPattern pattern, double memory_space_size)
    : pattern_(pattern), memory_space_size_(memory_space_size) {
    std::random_device rd;
    rng_ = std::mt19937(rd());
}

size_t MemoryAccessGenerator::generatePid() {
    std::uniform_real_distribution<double> uniform(0, 1.0);

    switch (pattern_) {
    case AccessPattern::UNIFORM: {
        std::uniform_int_distribution<size_t> dist(0, memory_space_size_ - 1);
        return dist(rng_);
    }

    case AccessPattern::SKEWED_70_20_10: {
        double rand = uniform(rng_);
        if (rand < 0.7) {
            // 70% of accesses go to first 10% of memory (hot pages)
            std::uniform_int_distribution<size_t> hot(0, memory_space_size_ * 0.1 - 1);
            return hot(rng_);
        }
        else if (rand < 0.9) {
            // 20% of accesses go to next 20% of memory (warm pages)
            std::uniform_int_distribution<size_t> medium(memory_space_size_ * 0.1, memory_space_size_ * 0.3 - 1);
            return medium(rng_);
        }
        else {
            // 10% of accesses go to remaining 70% of memory (cold pages)
            std::uniform_int_distribution<size_t> cold(memory_space_size_ * 0.3, memory_space_size_ - 1);
            return cold(rng_);
        }
    }
    }
    return 0;
}