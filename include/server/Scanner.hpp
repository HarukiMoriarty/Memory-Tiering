#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "Common.hpp"
#include "Logger.hpp"
#include "PageTable.hpp"
#include "RingBuffer.hpp"

#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <iostream>

class Server;

class Scanner {
private:
  PageTable *page_table_;
  PolicyConfig *policy_config_;

  bool scanner_shutdown_flag_ = false;
  boost::mutex scanner_shutdown_mutex_;

  bool _shouldShutdown();

public:
  // Constructor
  Scanner(PageTable *page_table, PolicyConfig *policy_config);

  // Check if a single page is hot
  bool classifyHotPage(const uint64_t last_access_time) const;

  // Check if a single page is cold
  bool classifyColdPage(const uint64_t last_access_time) const;

  // Continuously classify pages
  void runScanner(size_t num_tiers);

  // Stop scanner
  void signalShutdown();
};

#endif // SCANNER_HPP