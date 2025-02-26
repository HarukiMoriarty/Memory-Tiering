#include "Client.hpp"
#include "Logger.hpp"

#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>

Client::Client(size_t client_id, RingBuffer<ClientMessage> &buffer,
               size_t running_time, size_t memory_space_size,
               AccessPattern pattern, double rw_ratio)
    : buffer_(buffer), client_id_(client_id), running_time_(running_time),
      generator_(pattern, memory_space_size), rw_ratio_(rw_ratio) {}

void Client::run() {
  boost::chrono::steady_clock::time_point start_time =
      boost::chrono::steady_clock::now();
  boost::chrono::seconds total_duration(running_time_);

  while (true) {
    boost::chrono::steady_clock::time_point current_time =
        boost::chrono::steady_clock::now();
    auto elapsed = boost::chrono::duration_cast<boost::chrono::seconds>(
        current_time - start_time);

    if (elapsed >= total_duration) {
      break;
    }

    size_t pid = generator_.generatePid();
    // TODO: for now we do not generate offset
    size_t p_offset = 0;
    OperationType op = generator_.generateType(rw_ratio_);

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
