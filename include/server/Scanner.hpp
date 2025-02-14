#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "PageTable.hpp"
#include "RingBuffer.hpp"
#include "Common.hpp"
#include "Logger.hpp"

#include <chrono>
#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

class Server;

class Scanner {
private:
    PageTable& page_table_;
    bool running_;

public:
    // Constructor
    Scanner(PageTable& page_table);

    // Check if a single page is hot
    bool classifyHotPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const;

    // Check if a single page is cold
    bool classifyColdPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const;

    // Continuously classify pages
    void runScanner(boost::chrono::milliseconds hot_time_threshold, boost::chrono::milliseconds cold_time_threshold, size_t num_tiers);

    // Stop scanner
    void stopClassifier();

};

#endif // SCANNER_HPP