#ifndef METRICS_H
#define METRICS_H

#include <atomic>
#include <cstdint>

/**
 * Memory metrics collector with atomic counters
 */
class Metrics {
public:
    static Metrics& getInstance() {
        static Metrics instance;
        return instance;
    }

    // Deleted copy/move constructors and assignment operators
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    Metrics(Metrics&&) = delete;
    Metrics& operator=(Metrics&&) = delete;

    // Access counters
    inline void incrementLocalAccess() { local_access_count_++; }
    inline void incrementRemoteAccess() { remote_access_count_++; }
    inline void incrementPmemAccess() { pmem_access_count_++; }

    // Migration counters
    inline void incrementLocalToRemote() { local_to_remote_count_++; }
    inline void incrementRemoteToLocal() { remote_to_local_count_++; }
    inline void incrementPmemToRemote() { pmem_to_remote_count_++; }
    inline void incrementRemoteToPmem() { remote_to_pmem_count_++; }

    // Print current metrics (call periodically or at program end)
    void printMetrics() const;

    // Reset all counters
    void reset();

private:
    Metrics() = default;  // Private constructor for singleton

    // Access counters
    std::atomic<uint64_t> local_access_count_{ 0 };
    std::atomic<uint64_t> remote_access_count_{ 0 };
    std::atomic<uint64_t> pmem_access_count_{ 0 };

    // Migration counters
    std::atomic<uint64_t> local_to_remote_count_{ 0 };
    std::atomic<uint64_t> remote_to_local_count_{ 0 };
    std::atomic<uint64_t> pmem_to_remote_count_{ 0 };
    std::atomic<uint64_t> remote_to_pmem_count_{ 0 };
};

#endif