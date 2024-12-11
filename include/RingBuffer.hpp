#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <boost/circular_buffer.hpp>

template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity) : buf_(capacity) {}

    bool push(const T& item) {
        if (!buf_.full()) {
            buf_.push_back(item);
            return true;
        }
        return false;
    }

    bool pop(T& item) {
        if (!buf_.empty()) {
            item = buf_.front();
            buf_.pop_front();
            return true;
        }
        return false;
    }

private:
    boost::circular_buffer<T> buf_;
};

#endif // RING_BUFFER_H
