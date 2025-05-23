#include "Server.hpp"

Server::Server(RingBuffer<ClientMessage>& client_buffer,
  const std::vector<ClientConfig>& client_configs,
  ServerMemoryConfig* server_config, PolicyConfig* policy_config,
  bool use_cache_ring, size_t sample_rate, const std::string periodic_metric_filename)
  : client_buffer_(client_buffer), sample_rate_(sample_rate),
  server_config_(server_config),
  periodic_metric_filename_(periodic_metric_filename) {
  // Calculate load memory pages
  size_t client_total_page = 0;
  if (server_config_->num_tiers == 2) {
    for (ClientConfig client : client_configs) {
      base_page_id_.push_back(client_total_page);
      client_total_page += client.tier_sizes[0];
      client_total_page += client.tier_sizes[1];
    }
  }
  else if (server_config_->num_tiers == 3) {
    for (ClientConfig client : client_configs) {
      base_page_id_.push_back(client_total_page);
      client_total_page += client.tier_sizes[0];
      client_total_page += client.tier_sizes[1];
      client_total_page += client.tier_sizes[2];
    }
  }

  page_table_ = new PageTable(client_configs, server_config, use_cache_ring);
  page_table_->initPageTable();

  scanner_ = new Scanner(page_table_, policy_config);

  client_done_flags_ = std::vector<bool>(client_configs.size(), false);
}

Server::~Server() {
  delete page_table_;
  delete scanner_;
}

// Helper function to handle a ClientMessage
void Server::handleClientMessage(const ClientMessage& msg) {
  if (msg.op_type == OperationType::END) {
    client_done_flags_[msg.client_id] = true;
    LOG_DEBUG("Client " << msg.client_id << " sent END command.");

    // Check if all clients are done
    if (std::all_of(client_done_flags_.begin(), client_done_flags_.end(),
      [](bool done) { return done; })) {
      LOG_INFO("All clients sent END command.");
      signalShutdown();
    }
    return;
  }

  size_t page_index = base_page_id_[msg.client_id] + msg.pid;
  page_table_->accessPage(page_index, msg.op_type);
}

void Server::_runManagerThread() {
  LOG_INFO("Manager thread start!");
  while (!_shouldShutdown()) {
    ClientMessage client_msg(0, 0, 0, OperationType::READ);
    bool didwork = false;

    // Get memory request from client
    if (client_buffer_.pop(client_msg)) {
      LOG_DEBUG("Server received: " << client_msg.toString());
      handleClientMessage(client_msg);
      didwork = true;
    }

    // Sleep if no works was done
    if (!didwork) {
      boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
    }
  }
  LOG_INFO("Manager thread exiting...");
}

void Server::_runScannerThread() {
  LOG_INFO("Scanner thread start!");
  scanner_->runScanner(server_config_->num_tiers);
  LOG_INFO("Policy thread exiting...");
}

void Server::_runPeriodicalMetricsThread() {
  LOG_INFO("Periodical metric thread start!");
  auto metric_start_time = boost::chrono::steady_clock::now();
  Metrics& metrics = Metrics::getInstance();
  while (!_shouldShutdown()) {
    boost::this_thread::sleep_for(boost::chrono::seconds(sample_rate_));
    auto metric_end_time = boost::chrono::steady_clock::now();
    auto metric_duration = boost::chrono::duration_cast<boost::chrono::seconds>(metric_end_time - metric_start_time).count();
    metrics.periodicalMetrics(server_config_, metric_duration, periodic_metric_filename_);
  }
  LOG_DEBUG("Policy thread exiting...");
}

void Server::signalShutdown() {
  scanner_->signalShutdown();
  boost::lock_guard<boost::mutex> lock(manager_shutdown_mutex_);
  manager_shutdown_flag_ = true;
}

bool Server::_shouldShutdown() {
  boost::lock_guard<boost::mutex> lock(manager_shutdown_mutex_);
  return manager_shutdown_flag_;
}

// Main function to start threads
void Server::start() {
  boost::thread server_thread(&Server::_runManagerThread, this);
  boost::thread policy_thread(&Server::_runScannerThread, this);
  boost::thread periodical_metric_thread(&Server::_runPeriodicalMetricsThread,
    this);
  // Join threads
  server_thread.join();
  policy_thread.join();
  periodical_metric_thread.join();

  LOG_INFO("All threads exited. Server shutdown complete.");
}
