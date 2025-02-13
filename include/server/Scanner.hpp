#ifndef POLICY_CLASSIFIER_HPP
#define POLICY_CLASSIFIER_HPP

#include "PageTable.hpp"
#include "RingBuffer.hpp"
#include "Common.hpp"

#include <chrono>

class Server;

class Scanner {
private:
    PageTable& page_table_;
    bool running_; // To control the continuous scanning process

public:
    // Constructor
    Scanner(PageTable& page_table);

    // Check if a single page is hot
    bool classifyHotPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const;

    // Check if a single page is cold
    bool classifyColdPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const;

    // Continuously classify pages using scanNext()
    void runScanner(boost::chrono::milliseconds hot_time_threshold, boost::chrono::milliseconds cold_time_threshold, size_t num_tiers);

    // Stop the continuous classifier
    void stopClassifier();

};

#endif // POLICY_CLASSIFIER_HPP