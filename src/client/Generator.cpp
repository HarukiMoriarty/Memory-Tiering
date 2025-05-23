#include "Generator.hpp"

MemoryAccessGenerator::MemoryAccessGenerator(AccessPattern pattern,
  size_t memory_space_size, double zipf_s)
  : pattern_(pattern), memory_space_size_(memory_space_size) {
  std::random_device rd;
  rng_ = std::mt19937(rd());

  // HOT distribution
  if (pattern == AccessPattern::UNIFORM) {
    uniform_dist_ =
      std::uniform_int_distribution<size_t>(0, memory_space_size_ - 1);
  }
  else if (pattern == AccessPattern::HOT) {
    std::vector<size_t> all_pages(memory_space_size_);
    for (size_t i = 0; i < memory_space_size_; i++) {
      all_pages[i] = i;
    }

    std::shuffle(all_pages.begin(), all_pages.end(), rng_);
    size_t hot_count = std::ceil(memory_space_size_ * 0.2);

    for (size_t i = 0; i < hot_count; i++) {
      hot_pages_[i] = all_pages[i];
    }

    hot_dist_ = std::uniform_int_distribution<size_t>(0, hot_count - 1);
  }
  else if (pattern == AccessPattern::ZIPFIAN) {
    zipf_prob_.resize(memory_space_size_);

    for (size_t i = 1; i <= memory_space_size_; i++) {
      zipf_prob_[i - 1] = 1.0 / std::pow(i, zipf_s);
    }

    std::shuffle(zipf_prob_.begin(), zipf_prob_.end(), rng_);

    zipf_dist_ = std::discrete_distribution<size_t>(zipf_prob_.begin(),
      zipf_prob_.end());
  }
}

size_t MemoryAccessGenerator::generatePid() {
  std::uniform_real_distribution<double> uniform(0, 1.0);

  switch (pattern_) {
  case AccessPattern::UNIFORM: {
    return uniform_dist_(rng_);
  }

  case AccessPattern::HOT: {
    return hot_pages_[hot_dist_(rng_)];
  }

  case AccessPattern::ZIPFIAN: {
    return zipf_dist_(rng_);
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