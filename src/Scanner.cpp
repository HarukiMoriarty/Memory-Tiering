#include "Scanner.hpp"
#include "Logger.hpp"
#include "Server.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Scanner::Scanner(PageTable& page_table)
    : page_table_(page_table), running_(false) {
}

// Check if a page is hot based on both access count and time threshold
bool Scanner::classifyHotPage(const PageMetadata& page, size_t min_access_count) const {
    return page.access_count >= min_access_count;
}

// Check if a page is cold based on both access count and time threshold
bool Scanner::classifyColdPage(const PageMetadata& page, boost::chrono::milliseconds time_threshold) const {
    auto current_time = boost::chrono::steady_clock::now();
    auto time_since_last_access = boost::chrono::duration_cast<boost::chrono::milliseconds>(
        current_time - page.last_access_time);
    return time_since_last_access >= time_threshold;
}

// Continuously classify pages using scanNext()
void Scanner::runClassifier(RingBuffer<MemMoveReq>& move_page_buffer, size_t min_access_count, boost::chrono::milliseconds time_threshold, size_t num_tiers, Server& server) {
    running_ = true;
    while (running_ && !server.shouldShutdown()) {
        size_t page_id = page_table_.getNextPageId();
        PageMetadata page = page_table_.scanNext();

        switch (page.page_layer) {
        case PageLayer::NUMA_LOCAL: {
            if (num_tiers == 2) {
                // For two tiers, treat NUMA_LOCAL and NUMA_REMOTE as a single DRAM tier
                if (classifyColdPage(page, time_threshold)) {
                    LOG_DEBUG("Cold page detected in DRAM: " << page.page_address);
                    MemMoveReq msg(page_id, PageLayer::PMEM);
                    while (!move_page_buffer.push(msg)) {
                        boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
                    }
                }
            } else {
                // For three tiers, handle NUMA_LOCAL cold pages
                // Only detect cold pages for local NUMA
                if (classifyColdPage(page, time_threshold)) {
                    LOG_DEBUG("Cold page detected in NUMA_LOCAL: " << page.page_address);
                    MemMoveReq msg(page_id, PageLayer::NUMA_REMOTE);
                    while (!move_page_buffer.push(msg)) {
                        boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
                    }
                }
            }
            break;
        }

        case PageLayer::NUMA_REMOTE: {
            // Check cold first, then hot if not cold
            if (classifyColdPage(page, time_threshold)) {
                LOG_DEBUG("Cold page detected in NUMA_REMOTE: " << page.page_address);
                MemMoveReq msg(page_id, PageLayer::PMEM);
                while (!move_page_buffer.push(msg)) {
                    boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
                }
            }
            else if (classifyHotPage(page, min_access_count)) {
                LOG_DEBUG("Hot page detected in NUMA_REMOTE: " << page.page_address);
                MemMoveReq msg(page_id, PageLayer::NUMA_LOCAL);
                while (!move_page_buffer.push(msg)) {
                    boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
                }
            }
            break;
        }

        case PageLayer::PMEM: {
            // Only detect hot pages for PMEM
            LOG_DEBUG("Hot page detected in PMEM: " << page.page_address);
            if (classifyHotPage(page, min_access_count)) {
                if (num_tiers == 2) {
                    // Move hot pages from PMEM to DRAM in a two-tier setup
                    MemMoveReq msg(page_id, PageLayer::NUMA_LOCAL);
                    while (!move_page_buffer.push(msg)) {
                        boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
                    }
                } else {
                    // Move hot pages from PMEM to NUMA_REMOTE in a three-tier setup
                    MemMoveReq msg(page_id, PageLayer::NUMA_REMOTE);
                    while (!move_page_buffer.push(msg)) {
                        boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
                    }
                }
            }
            break;
        }
        }

        // Sleep for a short duration after whole table iteration
        if (page_id == page_table_.size() - 1) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
            LOG_DEBUG("Finished scanning all pages in one round!");
        }
    }
}


void Scanner::stopClassifier() {
    running_ = false;
}