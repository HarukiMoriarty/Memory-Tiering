#include "Metrics.hpp"
#include "Logger.hpp"

void Metrics::printMetrics() const {
    LOG_INFO("======== Memory Access Metrics ========");
    LOG_INFO("Access Counts:");
    LOG_INFO("  NUMA Local:  " << local_access_count_.load());
    LOG_INFO("  NUMA Remote: " << remote_access_count_.load());
    LOG_INFO("  PMEM:        " << pmem_access_count_.load());

    LOG_INFO("Migration Counts:");
    LOG_INFO("  Local -> Remote: " << local_to_remote_count_.load());
    LOG_INFO("  Remote -> Local: " << remote_to_local_count_.load());
    LOG_INFO("  PMEM -> Remote: " << pmem_to_remote_count_.load());
    LOG_INFO("  Remote -> PMEM: " << remote_to_pmem_count_.load());
    LOG_INFO("===================================");
}

void Metrics::printMetricsTwoTiers() const {
    LOG_INFO("======== Memory Access Metrics (Two Tiers) ========");
    LOG_INFO("Access Counts:");
    LOG_INFO("  DRAM: "  << (local_access_count_.load()));
    LOG_INFO("  PMEM: " << pmem_access_count_.load());

    LOG_INFO("Migration Counts:");
    LOG_INFO("  DRAM -> PMEM: " << local_to_pmem_count_.load());
    LOG_INFO("  PMEM -> DRAM: " << pmem_to_local_count_.load());
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
}