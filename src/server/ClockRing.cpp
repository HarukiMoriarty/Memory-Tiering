#include "ClockRing.hpp"

ClockRing::ClockRing(size_t capacity) : hand_(nullptr), size_(0), capacity_(capacity) {}

ClockRing::~ClockRing() {
    while (size_ > 0) {
        ClockRingNode* node = hand_;
        hand_ = hand_->next;
        delete node;
        size_--;
    }
    hand_ = nullptr;
}

bool ClockRing::insert(size_t page_id, ClockRingNode*& out_ptr) {
    if (size_ >= capacity_) return false;

    ClockRingNode* node = new ClockRingNode(page_id);
    out_ptr = node;

    if (!hand_) {
        hand_ = node->next = node->prev = node;
    }
    // insert before the hand, as the tail of the ring
    else {
        ClockRingNode* tail = hand_->prev;
        tail->next = node;
        node->prev = tail;
        node->next = hand_;
        hand_->prev = node;
    }

    ++size_;
    return true;
}

void ClockRing::remove(ClockRingNode*& node_ptr) {
    assert(!node_ptr && "Cannot remove nullptr");

    if (node_ptr->next == node_ptr) {
        hand_ = nullptr;
    }
    else {
        node_ptr->prev->next = node_ptr->next;
        node_ptr->next->prev = node_ptr->prev;
        if (hand_ == node_ptr)
            hand_ = node_ptr->next;
    }

    delete node_ptr;
    node_ptr = nullptr;
    --size_;
}

void ClockRing::markAccessed(ClockRingNode* node) {
    if (node) node->ref_bit.store(true, std::memory_order_relaxed);
}

size_t ClockRing::findEvictionCandidate() {
    assert(hand_ && "Clock ring is empty");

    while (true) {
        if (!hand_->ref_bit.load(std::memory_order_relaxed)) {
            size_t victim = hand_->page_id;
            ClockRingNode* evicted = hand_;
            hand_ = hand_->next;
            remove(evicted);
            return victim;
        }
        else {
            hand_->ref_bit.store(false, std::memory_order_relaxed);
            hand_ = hand_->next;
        }
    }
}

size_t ClockRing::size() const {
    return size_;
}

bool ClockRing::empty() const {
    return size_ == 0;
}