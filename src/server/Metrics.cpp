#include "Metrics.hpp"
#include "Logger.hpp"

void Metrics::printMetricsThreeTiers() const {
    LOG_INFO("======== Memory Access Metrics ========");
    LOG_INFO("Access Counts:");
    LOG_INFO("  NUMA Local:  " << local_access_count_.load());
    LOG_INFO("  NUMA Remote: " << remote_access_count_.load());
    LOG_INFO("  PMEM:        " << pmem_access_count_.load());

    LOG_INFO("Access Latency (ns):");
    LOG_INFO("  Min:  " << acc::min(access_latency_));
    LOG_INFO("  P50:  " << acc::extended_p_square(access_latency_)[1]);  // 50th percentile
    LOG_INFO("  P99:  " << acc::extended_p_square(access_latency_)[2]);  // 99th percentile
    LOG_INFO("  Max:  " << acc::max(access_latency_));
    LOG_INFO("  Mean: " << acc::mean(access_latency_));

    LOG_INFO("Migration Counts:");
    LOG_INFO("  Local -> Remote: " << local_to_remote_count_.load());
    LOG_INFO("  Remote -> Local: " << remote_to_local_count_.load());
    LOG_INFO("  PMEM -> Remote: " << pmem_to_remote_count_.load());
    LOG_INFO("  Remote -> PMEM: " << remote_to_pmem_count_.load());
    LOG_INFO("  Local -> PMEM: " << local_to_pmem_count_.load());
    LOG_INFO("  PMEM -> Local: " << pmem_to_local_count_.load());

    if (total_latency_.load() > 0) {
        LOG_INFO("Throughput:");
        uint64_t total_access = local_access_count_.load() + remote_access_count_.load() + pmem_access_count_.load();
        double throughput = static_cast<double>(total_access) * 1e9 / static_cast<double>(total_latency_.load());
        LOG_INFO("  Throughput: " << throughput << " ops/sec");
    }
    LOG_INFO("===================================");
}

void Metrics::printMetricsTwoTiers() const {
    LOG_INFO("======== Memory Access Metrics (Two Tiers) ========");
    LOG_INFO("Access Counts:");
    LOG_INFO("  DRAM: " << local_access_count_.load());
    LOG_INFO("  PMEM: " << pmem_access_count_.load());

    LOG_INFO("Access Latency (ns):");
    LOG_INFO("  Min:  " << acc::min(access_latency_));
    LOG_INFO("  P50:  " << acc::extended_p_square(access_latency_)[1]);
    LOG_INFO("  P99:  " << acc::extended_p_square(access_latency_)[2]);
    LOG_INFO("  Max:  " << acc::max(access_latency_));
    LOG_INFO("  Mean: " << acc::mean(access_latency_));

    LOG_INFO("Migration Counts:");
    LOG_INFO("  DRAM -> PMEM: " << local_to_pmem_count_.load());
    LOG_INFO("  PMEM -> DRAM: " << pmem_to_local_count_.load());

    if (total_latency_.load() > 0) {
        LOG_INFO("Throughput:");
        uint64_t total_access = local_access_count_.load() + pmem_access_count_.load();
        double throughput = static_cast<double>(total_access) * 1e9 / static_cast<double>(total_latency_.load());
        LOG_INFO("  Throughput: " << throughput << " ops/sec");
    }
    LOG_INFO("==========================================");
}

void Metrics::reset() {
    local_access_count_ = 0;
    remote_access_count_ = 0;
    pmem_access_count_ = 0;
    local_to_remote_count_ = 0;
    remote_to_local_count_ = 0;
    pmem_to_remote_count_ = 0;
    remote_to_pmem_count_ = 0;
    local_to_pmem_count_ = 0;
    pmem_to_local_count_ = 0;
    total_latency_ = 0;
    access_latency_ = AccumulatorType{ acc::tag::extended_p_square::probabilities = probabilities };
}