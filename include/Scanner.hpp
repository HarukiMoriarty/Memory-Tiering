#ifndef POLICY_CLASSIFIER_HPP
#define POLICY_CLASSIFIER_HPP

#include "PageTable.hpp"
#include "RingBuffer.hpp"
#include "Common.hpp"

#include <chrono>

class Scanner {
private:
    PageTable& page_table_;
    bool running_; // To control the continuous scanning process

public:
    // Constructor
    Scanner(PageTable& page_table);

    // Check if a single page is hot
    bool classifyHotPage(const PageMetadata& page, size_t min_access_count) const;

    // Check if a single page is cold
    bool classifyColdPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const;

    // Continuously classify pages using scanNext()
    void runClassifier(RingBuffer<MemMoveReq>& move_page_buffer, size_t min_access_count, boost::chrono::milliseconds time_threshold);

    // Stop the continuous classifier
    void stopClassifier();

};

#endif // POLICY_CLASSIFIER_HPP