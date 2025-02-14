#include "Metrics.hpp"

void Metrics::printMetricsThreeTiers() const {
    LOG_INFO("======== Memory Access Metrics ========");
    LOG_INFO("Access Counts:");
    LOG_INFO("  NUMA Local:  " << local_access_count_.load());
    LOG_INFO("  NUMA Remote: " << remote_access_count_.load());
    LOG_INFO("  PMEM:        " << pmem_access_count_.load());

    LOG_INFO("Access Latency (ns):");
    LOG_INFO("  Min:  " << acc::min(access_latency_));
    LOG_INFO("  P10:  " << acc::extended_p_square(access_latency_)[0]);
    LOG_INFO("  P20:  " << acc::extended_p_square(access_latency_)[1]);
    LOG_INFO("  P30:  " << acc::extended_p_square(access_latency_)[2]);
    LOG_INFO("  P40:  " << acc::extended_p_square(access_latency_)[3]);
    LOG_INFO("  P50:  " << acc::extended_p_square(access_latency_)[4]);
    LOG_INFO("  P60:  " << acc::extended_p_square(access_latency_)[5]);
    LOG_INFO("  P70:  " << acc::extended_p_square(access_latency_)[6]);
    LOG_INFO("  P80:  " << acc::extended_p_square(access_latency_)[7]);
    LOG_INFO("  P90:  " << acc::extended_p_square(access_latency_)[8]);
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
    LOG_INFO("  P10:  " << acc::extended_p_square(access_latency_)[0]);
    LOG_INFO("  P20:  " << acc::extended_p_square(access_latency_)[1]);
    LOG_INFO("  P30:  " << acc::extended_p_square(access_latency_)[2]);
    LOG_INFO("  P40:  " << acc::extended_p_square(access_latency_)[3]);
    LOG_INFO("  P50:  " << acc::extended_p_square(access_latency_)[4]);
    LOG_INFO("  P60:  " << acc::extended_p_square(access_latency_)[5]);
    LOG_INFO("  P70:  " << acc::extended_p_square(access_latency_)[6]);
    LOG_INFO("  P80:  " << acc::extended_p_square(access_latency_)[7]);
    LOG_INFO("  P90:  " << acc::extended_p_square(access_latency_)[8]);
    LOG_INFO("  Max   " << acc::max(access_latency_));
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

void Metrics::outputLatencyCDFToFile(const std::string& filename) const {
    std::ofstream outfile(filename);
    if (!outfile) {
        LOG_ERROR("Failed to open file: " << filename);
        return;
    }

    // Write header
    outfile << "percentile,latency_ns\n";

    outfile << "Min," << acc::min(access_latency_) << "\n";
    // Write each percentile point
    for (size_t i = 0; i < std::size(probabilities); i++) {
        double percentile = probabilities[i];
        uint64_t latency = acc::extended_p_square(access_latency_)[i];
        outfile << percentile << "," << latency << "\n";
    }
    outfile << "Max," << acc::max(access_latency_) << "\n";
    outfile << "Mean," << acc::mean(access_latency_) << "\n";

    outfile.close();
    LOG_INFO("CDF data written to: " << filename);
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