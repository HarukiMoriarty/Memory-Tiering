#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <boost/circular_buffer.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>

template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity) : buf_(capacity) {}

    bool push(const T& item) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        if (buf_.full()) {
            return false;
        }
        buf_.push_back(item);
        not_empty_.notify_one();
        return true;
    }

    bool pop(T& item) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        if (buf_.empty()) {
            return false;
        }
        item = buf_.front();
        buf_.pop_front();
        not_full_.notify_one();
        return true;
    }

private:
    boost::circular_buffer<T> buf_;
    mutable boost::mutex mutex_;
    boost::condition_variable not_full_;
    boost::condition_variable not_empty_;
};

#endif // RING_BUFFER_H
