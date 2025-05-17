#ifndef CLOCK_RING_HPP
#define CLOCK_RING_HPP

#include <cstddef>
#include <atomic>
#include <cassert>

struct ClockRingNode {
    size_t page_id;
    std::atomic<bool> ref_bit;
    ClockRingNode* prev;
    ClockRingNode* next;

    ClockRingNode(size_t pid) : page_id(pid), ref_bit(true), prev(nullptr), next(nullptr) {}
};

class ClockRing {
public:
    ClockRing(size_t capacity);
    ~ClockRing();

    // Disable copy
    ClockRing(const ClockRing&) = delete;
    ClockRing& operator=(const ClockRing&) = delete;

    bool insert(size_t page_id, ClockRingNode*& out_ptr);
    void remove(ClockRingNode*& node_ptr);
    size_t findEvictionCandidate();

    static void markAccessed(ClockRingNode* node);

    size_t size() const;
    bool empty() const;

private:
    ClockRingNode* hand_;
    size_t size_;
    size_t capacity_;
};

#endif // CLOCK_RING_HPP