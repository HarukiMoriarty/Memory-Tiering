#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <sstream>
#include <random>

enum class OperationType {
    READ,
    WRITE,
};

enum class AccessPattern {
    UNIFORM,
    SKEWED_70_20_10
};

class MemoryAccessGenerator {
private:
    AccessPattern pattern_;
    std::mt19937 rng_;
    double memory_size_;

public:
    MemoryAccessGenerator(AccessPattern pattern, double memory_size)
        : pattern_(pattern), memory_size_(memory_size) {
        std::random_device rd;
        rng_ = std::mt19937(rd());
    }

    int generateOffset() {  // Now returns int instead of void*
        std::uniform_real_distribution<double> uniform(0, 1.0);

        switch (pattern_) {
        case AccessPattern::UNIFORM: {
            std::uniform_int_distribution<int> dist(0, memory_size_);
            return dist(rng_);
        }

        case AccessPattern::SKEWED_70_20_10: {
            double rand = uniform(rng_);
            if (rand < 0.7) {
                // 70% of accesses go to first 10% of memory
                std::uniform_int_distribution<int> hot(0, memory_size_ * 0.1);
                return hot(rng_);
            }
            else if (rand < 0.9) {
                // 20% of accesses go to next 20% of memory
                std::uniform_int_distribution<int> medium(memory_size_ * 0.1, memory_size_ * 0.3);
                return medium(rng_);
            }
            else {
                // 10% of accesses go to remaining 70% of memory
                std::uniform_int_distribution<int> cold(memory_size_ * 0.3, memory_size_);
                return cold(rng_);
            }
        }
        }
        return 0;
    }
};


struct Message {
    int client_id;
    int offset;
    OperationType op_type;

    Message(int id, int off, OperationType op)
        : client_id(id), offset(off), op_type(op) {
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "Client " << client_id
            << ", Offset: " << offset
            << ", Operation: " << (op_type == OperationType::READ ? "READ" : "WRITE");
        return ss.str();
    }
};

struct MemMoveReq {
    int page_id;
    int layer_id;

    MemMoveReq(int page_id, int layer_id)
        : page_id(page_id), layer_id(layer_id) {
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "Page " << page_id
            << " to Layer: " << layer_id;
        return ss.str();
    }
};

#endif // COMMON_H