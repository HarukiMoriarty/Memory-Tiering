#ifndef METRICS_H
#define METRICS_H

#include <atomic>
#include <cstdint>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/extended_p_square.hpp>

namespace acc = boost::accumulators;

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
    inline void incrementLocalToPmem() { local_to_pmem_count_++; }
    inline void incrementPmemToLocal() { pmem_to_local_count_++; }

    // Latency recording
    inline void recordAccessLatency(uint64_t latency_ns) {
        access_latency_(latency_ns);
        total_latency_ += latency_ns;
    }

    inline void recordMigrationLatency(uint64_t latency_ns) {
        migration_latency_(latency_ns);
        total_latency_ += latency_ns;
    }

    // Print current metrics (call periodically or at program end)
    void printMetricsThreeTiers() const;
    void printMetricsTwoTiers() const;

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
    std::atomic<uint64_t> local_to_pmem_count_{ 0 };
    std::atomic<uint64_t> pmem_to_local_count_{ 0 };

    // Latency tracking
    static constexpr double probabilities[] = { 0.0, 0.50, 0.99 };

    using AccumulatorType = acc::accumulator_set<uint64_t,
        acc::stats<
        acc::tag::mean,
        acc::tag::min,
        acc::tag::max,
        acc::tag::extended_p_square
        >
    >;

    AccumulatorType access_latency_{ acc::tag::extended_p_square::probabilities = probabilities };
    AccumulatorType migration_latency_{ acc::tag::extended_p_square::probabilities = probabilities };

    // Total latency tracking for throughput calculation
    std::atomic<uint64_t> total_latency_{ 0 };
};

#endif