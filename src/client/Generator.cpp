#include "Generator.hpp"

MemoryAccessGenerator::MemoryAccessGenerator(AccessPattern pattern,
                                             double memory_space_size)
    : pattern_(pattern), memory_space_size_(memory_space_size) {
  std::random_device rd;
  rng_ = std::mt19937(rd());

  // Pre-generation skewed distribution
  if (pattern == AccessPattern::SKEWED_70_20_10) {
    std::vector<size_t> all_pages(memory_space_size_);
    for (size_t i = 0; i < memory_space_size_; i++) {
      all_pages[i] = i;
    }

    std::shuffle(all_pages.begin(), all_pages.end(), rng_);

    size_t hot_count = std::ceil(memory_space_size_ * 0.1);
    size_t warm_count = std::ceil(memory_space_size_ * 0.2);
    size_t cold_count = memory_space_size_ - hot_count - warm_count;
    assert(hot_count + warm_count + cold_count == memory_space_size_);

    for (size_t i = 0; i < hot_count; i++) {
      hot_pages_[i] = all_pages[i];
    }

    for (size_t i = 0; i < warm_count; i++) {
      warm_pages_[i] = all_pages[hot_count + i];
    }

    for (size_t i = 0; i < cold_count; i++) {
      cold_pages_[i] = all_pages[hot_count + warm_count + i];
    }
  }
}

size_t MemoryAccessGenerator::generatePid() {
  std::uniform_real_distribution<double> uniform(0, 1.0);

  switch (pattern_) {
  case AccessPattern::UNIFORM: {
    std::uniform_int_distribution<size_t> dist(0, memory_space_size_ - 1);
    return dist(rng_);
  }

  case AccessPattern::SKEWED_70_20_10: {
    // Now we just assume all the access come to 20% pages
    // TODO: 70-20-10
    std::uniform_int_distribution<size_t> warm_dist(0, warm_pages_.size() - 1);
    return warm_pages_[warm_dist(rng_)];
  }
  }
  return 0;
}

OperationType MemoryAccessGenerator::generateType(double rw_ratio) {
  if (type_dis(rng_) < rw_ratio) {
    return OperationType::READ;
  }
  return OperationType::WRITE;
}