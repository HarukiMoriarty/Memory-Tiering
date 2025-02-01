#include "Client.hpp"
#include "Logger.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Client::Client(RingBuffer<ClientMessage>& buffer, size_t client_id, size_t message_cnt, size_t memory_space_size, AccessPattern pattern)
    : buffer_(buffer),
    client_id_(client_id),
    message_cnt_(message_cnt),
    generator_(pattern, memory_space_size) {
}

void Client::run() {
    for (size_t i = 0; i < message_cnt_; ++i) {
        size_t pid = generator_.generatePid();
        // TODO: for now we do not generate offset
        size_t p_offset = 0;
        OperationType op = (i % 2 == 0) ? OperationType::READ : OperationType::WRITE;

        ClientMessage msg(client_id_, pid, p_offset, op);
        while (!buffer_.push(msg)) {
            boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
        }

        LOG_DEBUG("Client sent: " << msg.toString());
    }

    ClientMessage end_msg(client_id_, 0, 0, OperationType::END);
    while (!buffer_.push(end_msg)) {
        boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
    }
    LOG_DEBUG("Client sent END message.");
}
