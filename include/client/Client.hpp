#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "Common.hpp"
#include "Generator.hpp"
#include "RingBuffer.hpp"

class Client {
public:
  Client(size_t client_id, RingBuffer<ClientMessage> &buffer,
         size_t running_time, size_t memory_space_size, AccessPattern pattern,
         double rw_ratio);
  void run();

private:
  RingBuffer<ClientMessage> &buffer_;
  size_t client_id_;
  size_t running_time_;
  MemoryAccessGenerator generator_;
  double rw_ratio_;
};

#endif // CLIENT_H
