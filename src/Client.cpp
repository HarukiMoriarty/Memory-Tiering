#include "Client.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Client::Client(RingBuffer<Message>& buffer, int message_cnt) : buffer_(buffer), message_cnt_(message_cnt) {}

void Client::run() {
    for (int i = 0; i < message_cnt_; ++i) {
        void* dummy_address = reinterpret_cast<void*>(i * 4096);
        OperationType op = (i % 2 == 0) ? OperationType::READ : OperationType::WRITE;
        Message msg(dummy_address, op);

        while (!buffer_.push(msg)) {
            boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
        }

        std::cout << "Client sent: " << msg.toString() << std::endl;
    }
}
