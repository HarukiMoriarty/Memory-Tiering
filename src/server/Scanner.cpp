#include "Scanner.hpp"
#include "Logger.hpp"
#include "Server.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Scanner::Scanner(PageTable& page_table)
    : page_table_(page_table), running_(false) {
}

// Check if a page is hot based on time threshold
bool Scanner::classifyHotPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const {
    auto current_time = boost::chrono::steady_clock::now();
    auto time_since_last_access = boost::chrono::duration_cast<boost::chrono::milliseconds>(
        current_time - page.last_access_time);
    LOG_DEBUG("Time since last access: " << time_since_last_access.count() << "ms (threshold: " << time_threshold.count() << "ms)");
    return time_since_last_access <= time_threshold;
}

// Check if a page is cold based on both access count and time threshold
bool Scanner::classifyColdPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const {
    auto current_time = boost::chrono::steady_clock::now();
    auto time_since_last_access = boost::chrono::duration_cast<boost::chrono::milliseconds>(
        current_time - page.last_access_time);
    return time_since_last_access >= time_threshold;
}

// Continuously classify pages
void Scanner::runScanner(boost::chrono::milliseconds hot_time_threshold, boost::chrono::milliseconds cold_time_threshold, size_t num_tiers) {
    running_ = true;

    // Sleep for hot time thershold time.
    boost::this_thread::sleep_for(hot_time_threshold);

    while (running_) {
        size_t page_id = page_table_.scanNext();
        PageMetadata page_meta_data = page_table_.getPage(page_id);

        switch (page_meta_data.page_layer) {
        case PageLayer::NUMA_LOCAL: {
            if (classifyColdPage(page_meta_data, cold_time_threshold)) {
                LOG_DEBUG("Cold page detected in DRAM: " << page_id);
                if (num_tiers == 2) {
                    page_table_.migratePage(page_id, PageLayer::PMEM);
                }
                else {
                    page_table_.migratePage(page_id, PageLayer::NUMA_REMOTE);

                }
            }
            break;
        }

        case PageLayer::NUMA_REMOTE: {
            // Check cold first, then hot if not cold
            if (classifyColdPage(page_meta_data, cold_time_threshold)) {
                LOG_DEBUG("Cold page detected in NUMA_REMOTE: " << page_id);
                page_table_.migratePage(page_id, PageLayer::PMEM);
            }
            else if (classifyHotPage(page_meta_data, hot_time_threshold)) {
                LOG_DEBUG("Hot page detected in NUMA_REMOTE: " << page_id);
                page_table_.migratePage(page_id, PageLayer::NUMA_LOCAL);
            }
            break;
        }

        case PageLayer::PMEM: {
            // Only detect hot pages for PMEM
            if (classifyHotPage(page_meta_data, hot_time_threshold)) {
                LOG_DEBUG("Hot page detected in PMEM: " << page_id);
                if (num_tiers == 2) {
                    // Move hot pages from PMEM to DRAM in a two-tier setup
                    page_table_.migratePage(page_id, PageLayer::NUMA_LOCAL);
                }
                else {
                    // Move hot pages from PMEM to NUMA_REMOTE in a three-tier setup
                    page_table_.migratePage(page_id, PageLayer::NUMA_REMOTE);
                }
            }
            break;
        }
        }

        // Sleep for a short duration after whole table iteration
        if (page_id == page_table_.size() - 1) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            LOG_DEBUG("Finished scanning all pages in one round!");
        }
    }
}


void Scanner::stopClassifier() {
    running_ = false;
}