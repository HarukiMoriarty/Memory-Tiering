#ifndef SERVER_H
#define SERVER_H

#include <boost/chrono.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <string>

#include "Common.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "PageTable.hpp"
#include "RingBuffer.hpp"
#include "Scanner.hpp"
#include "Utils.hpp"

class Server {
public:
  Server(RingBuffer<ClientMessage>& client_buffer,
    const std::vector<ClientConfig>& client_configs,
    ServerMemoryConfig* server_config, PolicyConfig* policy_config,
    size_t sample_rate);
  ~Server();

  void handleClientMessage(const ClientMessage& msg);

  void start();
  void signalShutdown();

private:
  // start function
  void _runManagerThread();
  void _runScannerThread();
  void _runPeriodicalMetricsThread();

  // shutdown function
  bool _shouldShutdown();

  // private variable
  RingBuffer<ClientMessage>& client_buffer_;
  PageTable* page_table_;
  Scanner* scanner_;
  size_t sample_rate_;

  ServerMemoryConfig* server_config_;
  PolicyConfig* policy_config_;
  std::vector<bool> client_done_flags_;

  // Base page id for each memory layer
  std::vector<size_t> base_page_id_;

  bool manager_shutdown_flag_ = false;
  boost::mutex manager_shutdown_mutex_;
};

#endif // SERVER_H
