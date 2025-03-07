#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <linux/mempolicy.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <string.h>
#include <numa.h>
#include <cpuid.h>      
#include <sys/ioctl.h>
#include <asm/unistd.h>

#define PAGE_SIZE 4096
// Problem: numa local only has 1GB space left
#define PAGE_NUM 350000
#define ITERATIONS 1
#define OFFSET_COUNT 350000

// CPU instruction support flags
static int has_clflushopt = 0;
static int has_clwb = 0;

typedef enum {
    READ = 0,
    WRITE_NORMAL = 1,        // Regular store
    WRITE_NTSTORE = 2,       // Non-temporal store
    WRITE_CLFLUSH = 3,       // Store + clflush
    WRITE_CLFLUSHOPT = 4,    // Store + clflushopt
    WRITE_CLWB = 5,          // Store + clwb
} MemAccessMode;

typedef struct {
    int fd_dtlb_load_misses;
    int fd_dtlb_store_misses;
    uint64_t dtlb_load_misses;
    uint64_t dtlb_store_misses;
} TLBCounters;

static inline void asm_read(volatile void* addr, uint64_t* value) {
    asm volatile(
        "movq (%1), %0"
        : "=r" (*value)
        : "r" (addr)
        : "memory"
        );
}

static inline void asm_write(volatile void* addr, uint64_t value) {
    asm volatile(
        "movq %1, (%0)"
        :
    : "r" (addr), "r" (value)
        : "memory"
        );
}

static inline void asm_ntstore(volatile void* addr, uint64_t value) {
    asm volatile(
        "movnti %1, (%0)"
        :
    : "r" (addr), "r" (value)
        : "memory"
        );
}

static inline void asm_clflush(void* addr) {
    asm volatile(
        "clflush (%0)"
        :
    : "r" (addr)
        : "memory"
        );
}

static inline void asm_clflushopt(void* addr) {
    asm volatile(
        "clflushopt (%0)"
        :
    : "r" (addr)
        : "memory"
        );
}

static inline void asm_clwb(void* addr) {
    asm volatile(
        "clwb (%0)"
        :
    : "r" (addr)
        : "memory"
        );
}

static inline void asm_mfence() {
    asm volatile("mfence" ::: "memory");
}

static inline void asm_lfence() {
    asm volatile("lfence" ::: "memory");
}

TLBCounters* setup_tlb_counters() {
    TLBCounters* counters = malloc(sizeof(TLBCounters));
    if (!counters) {
        perror("Failed to allocate memory for TLB counters");
        return NULL;
    }

    struct perf_event_attr pe;

    // Set up DTLB load miss counter
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HW_CACHE;
    pe.size = sizeof(struct perf_event_attr);
    // Read TLB statistic
    pe.config = (PERF_COUNT_HW_CACHE_DTLB) |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    pe.disabled = 1;
    // Exclusive kernel event
    pe.exclude_kernel = 1;
    // Exclusive hypervisor event
    pe.exclude_hv = 1;

    counters->fd_dtlb_load_misses = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (counters->fd_dtlb_load_misses == -1) {
        perror("Error opening DTLB load miss counter");
        free(counters);
        return NULL;
    }

    // Set up DTLB store miss counter
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HW_CACHE;
    pe.size = sizeof(struct perf_event_attr);
    // Write TLB statistic
    pe.config = (PERF_COUNT_HW_CACHE_DTLB) |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    counters->fd_dtlb_store_misses = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (counters->fd_dtlb_store_misses == -1) {
        perror("Error opening DTLB store miss counter");
        close(counters->fd_dtlb_load_misses);
        free(counters);
        return NULL;
    }

    return counters;
}

void start_tlb_counting(TLBCounters* counters) {
    if (!counters) return;

    ioctl(counters->fd_dtlb_load_misses, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_dtlb_store_misses, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_dtlb_load_misses, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(counters->fd_dtlb_store_misses, PERF_EVENT_IOC_ENABLE, 0);
}

void stop_tlb_counting(TLBCounters* counters) {
    if (!counters) return;

    ioctl(counters->fd_dtlb_load_misses, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(counters->fd_dtlb_store_misses, PERF_EVENT_IOC_DISABLE, 0);

    read(counters->fd_dtlb_load_misses, &counters->dtlb_load_misses, sizeof(uint64_t));
    read(counters->fd_dtlb_store_misses, &counters->dtlb_store_misses, sizeof(uint64_t));
}

void cleanup_tlb_counters(TLBCounters* counters) {
    if (counters) {
        close(counters->fd_dtlb_load_misses);
        close(counters->fd_dtlb_store_misses);
        free(counters);
    }
}

void check_cpu_features() {
    unsigned int eax, ebx, ecx, edx;

    if (__get_cpuid(0, &eax, &ebx, &ecx, &edx) && eax >= 7) {
        // Get feature flags from leaf 7
        __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);

        // Check for CLFLUSHOPT (bit 23 of ebx)
        has_clflushopt = (ebx >> 23) & 1;

        // Check for CLWB (bit 24 of ebx)
        has_clwb = (ebx >> 24) & 1;
    }

    printf("CPU Features: CLFLUSHOPT: %s, CLWB: %s\n",
        has_clflushopt ? "Supported" : "Not supported",
        has_clwb ? "Supported" : "Not supported");
}

const char* access_mode_to_string(MemAccessMode mode) {
    switch (mode) {
    case READ: return "READ";
    case WRITE_NORMAL: return "WRITE (normal)";
    case WRITE_NTSTORE: return "WRITE (ntstore)";
    case WRITE_CLFLUSH: return "WRITE (clflush)";
    case WRITE_CLFLUSHOPT: return "WRITE (clflushopt)";
    case WRITE_CLWB: return "WRITE (clwb)";
    default: return "UNKNOWN";
    }
}

uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void* allocate_and_bind_to_numa(size_t size, size_t number, int numa_node) {
    // Step 1: Allocate memory using mmap
    void* addr = mmap(NULL, size * number, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }

    // Step 2: Directly call the mbind syscall to bind memory to the NUMA node
    unsigned long nodemask = (1UL << numa_node);
    if (syscall(SYS_mbind, addr, size * number, MPOL_BIND, &nodemask,
        sizeof(nodemask) * 8, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0) {
        perror("mbind syscall failed");
        munmap(addr, size * number);
        return NULL;
    }

    // Step 3: Access the memory to ensure physical allocation
    memset(addr, 1, size * number);

    // Step 4: Verify the memory is actually on the requested NUMA node
    void** pages = malloc(number * sizeof(void*));
    int* status = malloc(number * sizeof(int));

    if (pages && status) {
        for (size_t i = 0; i < number; i++) {
            pages[i] = (char*)addr + i * size;
        }

        // Get the actual location of each page
        if (syscall(SYS_move_pages, 0, number, pages, NULL, status, 0) == 0) {
            int correct_pages = 0;
            for (size_t i = 0; i < number; i++) {
                if (status[i] == numa_node) {
                    correct_pages++;
                }
            }

            if (correct_pages != number) {
                fprintf(stderr, "Warning: Only %d/%zu pages (%.1f%%) correctly allocated on node %d\n",
                    correct_pages, number, (100.0 * correct_pages) / number, numa_node);
            }
        }

        free(pages);
        free(status);
    }
    return addr;
}

void move_pages_to_node(void* addr, size_t size, size_t number, int target_node) {
    void** pages = malloc(number * sizeof(void*));
    int* nodes = malloc(number * sizeof(int));
    int* status = malloc(number * sizeof(int));

    if (!pages || !nodes || !status) {
        perror("Memory allocation failed");
        free(pages);
        free(nodes);
        free(status);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < number; i++) {
        pages[i] = (char*)addr + i * size;
        nodes[i] = target_node;
    }

    if (syscall(SYS_move_pages, 0, number, pages, nodes, status, MPOL_MF_MOVE) != 0) {
        perror("move_pages failed");
        free(pages);
        free(nodes);
        free(status);
        exit(EXIT_FAILURE);
    }

    free(pages);
    free(nodes);
    free(status);
}

typedef struct {
    double time;
    uint64_t tlb_load_misses;
    uint64_t tlb_store_misses;
} AccessResult;

AccessResult access_memory_with_tlb(void* addr, int* offsets, int offset_count, MemAccessMode mode, TLBCounters* tlb_counters) {
    AccessResult result = { 0 };
    uint64_t value = 0xDEADBEEF; // Value to write
    uint64_t total_time = 0;

    // Start TLB counters
    start_tlb_counting(tlb_counters);

    for (int i = 0; i < offset_count; i++) {
        int page_idx = offsets[i] / (PAGE_SIZE / sizeof(uint64_t));
        int offset_in_page = (offsets[i] % (PAGE_SIZE / sizeof(uint64_t)));
        volatile uint64_t* access_addr = (volatile uint64_t*)addr +
            page_idx * (PAGE_SIZE / sizeof(uint64_t)) +
            offset_in_page;

        asm_clflush((void*)access_addr);
        asm_mfence(); // Ensure flush completes before measurement
        asm_lfence(); // Prevent instruction reordering

        uint64_t start = get_time_ns();

        if (mode == READ) {
            // Force a read from memory
            uint64_t read_value;
            asm_read(access_addr, &read_value);
        }
        else if (mode == WRITE_NORMAL) {
            // Regular store
            asm_write(access_addr, value);
        }
        else if (mode == WRITE_NTSTORE) {
            // Non-temporal store
            asm_ntstore(access_addr, value);
        }
        else if (mode == WRITE_CLFLUSH) {
            // Store + clflush
            asm_write(access_addr, value);
            asm_clflush((void*)access_addr);
        }
        else if (mode == WRITE_CLFLUSHOPT) {
            // Store + clflushopt
            asm_write(access_addr, value);
            asm_clflushopt((void*)access_addr);
        }
        else if (mode == WRITE_CLWB) {
            // Store + clwb
            asm_write(access_addr, value);
            asm_clwb((void*)access_addr);
        }

        asm_lfence(); // Prevent instruction reordering
        asm_mfence(); // Ensure flush completes before measurement

        uint64_t end = get_time_ns();
        total_time += (end - start);
    }

    // Stop TLB counters and get results
    stop_tlb_counting(tlb_counters);

    result.time = (double)total_time / offset_count;
    result.tlb_load_misses = tlb_counters->dtlb_load_misses;
    result.tlb_store_misses = tlb_counters->dtlb_store_misses;

    return result;
}

typedef struct {
    double alloc_time;
    AccessResult source_access;
    double move_time;
    AccessResult dest_access;
} BenchmarkResult;

BenchmarkResult benchmark_memory_ops(int source_node, int target_node, MemAccessMode mode) {
    BenchmarkResult result = { 0 };

    TLBCounters* tlb_counters = setup_tlb_counters();
    if (!tlb_counters) {
        fprintf(stderr, "Warning: TLB counters not available\n");
    }

    // Allocate memory on source node
    uint64_t start_time = get_time_ns();
    void* memory = allocate_and_bind_to_numa(PAGE_SIZE, PAGE_NUM, source_node);
    result.alloc_time = (double)(get_time_ns() - start_time);

    if (!memory) {
        fprintf(stderr, "Failed to allocate memory on node %d\n", source_node);
        cleanup_tlb_counters(tlb_counters);
        return result;
    }

    // Generate random offsets for access
    int* offsets = malloc(OFFSET_COUNT * sizeof(int));
    for (int i = 0; i < OFFSET_COUNT; i++) {
        int page = rand() % PAGE_NUM;
        int offset_in_page = rand() % (PAGE_SIZE / sizeof(uint64_t));
        offsets[i] = page * (PAGE_SIZE / sizeof(uint64_t)) + offset_in_page;
    }

    // Access memory on source node
    result.source_access = access_memory_with_tlb(memory, offsets, OFFSET_COUNT, mode, tlb_counters);

    // Move memory to target node
    start_time = get_time_ns();
    move_pages_to_node(memory, PAGE_SIZE, PAGE_NUM, target_node);
    result.move_time = (double)(get_time_ns() - start_time) / PAGE_NUM;

    // Access memory after moving to target node
    result.dest_access = access_memory_with_tlb(memory, offsets, OFFSET_COUNT, mode, tlb_counters);

    // Cleanup
    free(offsets);
    munmap(memory, (size_t)PAGE_SIZE * (size_t)PAGE_NUM);
    cleanup_tlb_counters(tlb_counters);

    return result;
}

void print_access_latency_table_with_tlb(const char* operation_type,
    const AccessResult* local_local, const AccessResult* local_remote, const AccessResult* local_pmem,
    const AccessResult* remote_local, const AccessResult* remote_remote, const AccessResult* remote_pmem,
    const AccessResult* pmem_local, const AccessResult* pmem_remote, const AccessResult* pmem_pmem) {

    printf("\n--- %s Access Latency (ns) with TLB Misses ---\n", operation_type);
    printf("%-12s | %-33s | %-33s | %-33s\n", "Source/Dest", "Local", "Remote", "PMEM");
    printf("-------------+-----------------------------------+-----------------------------------+-------------------------------------\n");

    // Print each row with latency and TLB miss information
    printf("%-12s | %7.2f ns (L:%7lu, S:%7lu) | %7.2f ns (L:%7lu, S:%7lu) | %7.2f ns (L:%7lu, S:%7lu)\n",
        "Local",
        local_local->time, local_local->tlb_load_misses, local_local->tlb_store_misses,
        local_remote->time, local_remote->tlb_load_misses, local_remote->tlb_store_misses,
        local_pmem->time, local_pmem->tlb_load_misses, local_pmem->tlb_store_misses);

    printf("%-12s | %7.2f ns (L:%7lu, S:%7lu) | %7.2f ns (L:%7lu, S:%7lu) | %7.2f ns (L:%7lu, S:%7lu)\n",
        "Remote",
        remote_local->time, remote_local->tlb_load_misses, remote_local->tlb_store_misses,
        remote_remote->time, remote_remote->tlb_load_misses, remote_remote->tlb_store_misses,
        remote_pmem->time, remote_pmem->tlb_load_misses, remote_pmem->tlb_store_misses);

    printf("%-12s | %7.2f ns (L:%7lu, S:%7lu) | %7.2f ns (L:%7lu, S:%7lu) | %7.2f ns (L:%7lu, S:%7lu)\n",
        "PMEM",
        pmem_local->time, pmem_local->tlb_load_misses, pmem_local->tlb_store_misses,
        pmem_remote->time, pmem_remote->tlb_load_misses, pmem_remote->tlb_store_misses,
        pmem_pmem->time, pmem_pmem->tlb_load_misses, pmem_pmem->tlb_store_misses);

    printf("\nL: DTLB Load Misses, S: DTLB Store Misses\n");
}

// Function to print the Migration Latency table
void print_migration_latency_table(const char* operation_type,
    double local_remote, double local_pmem,
    double remote_local, double remote_pmem,
    double pmem_local, double pmem_remote) {
    printf("\n--- %s Migration Latency per Page (ns) ---\n", operation_type);
    printf("%-12s | %-12s | %-12s | %-12s\n", "Source/Dest", "Local", "Remote", "PMEM");
    printf("-------------+--------------+--------------+-------------\n");
    printf("%-12s | %-12s | %-12.2f | %-12.2f\n", "Local", "N/A", local_remote, local_pmem);
    printf("%-12s | %-12.2f | %-12s | %-12.2f\n", "Remote", remote_local, "N/A", remote_pmem);
    printf("%-12s | %-12.2f | %-12.2f | %-12s\n", "PMEM", pmem_local, pmem_remote, "N/A");
}

void run_benchmark_for_mode(MemAccessMode mode) {
    const int LOCAL_NODE = 0;   // Local DRAM
    const int REMOTE_NODE = 1;  // Remote DRAM
    const int PMEM_NODE = 2;    // Persistent Memory

    const char* mode_str = access_mode_to_string(mode);
    printf("\n=== %s OPERATIONS BENCHMARKS ===\n", mode_str);

    BenchmarkResult local_to_local = benchmark_memory_ops(LOCAL_NODE, LOCAL_NODE, mode);
    BenchmarkResult local_to_remote = benchmark_memory_ops(LOCAL_NODE, REMOTE_NODE, mode);
    BenchmarkResult local_to_pmem = benchmark_memory_ops(LOCAL_NODE, PMEM_NODE, mode);
    BenchmarkResult remote_to_local = benchmark_memory_ops(REMOTE_NODE, LOCAL_NODE, mode);
    BenchmarkResult remote_to_remote = benchmark_memory_ops(REMOTE_NODE, REMOTE_NODE, mode);
    BenchmarkResult remote_to_pmem = benchmark_memory_ops(REMOTE_NODE, PMEM_NODE, mode);
    BenchmarkResult pmem_to_local = benchmark_memory_ops(PMEM_NODE, LOCAL_NODE, mode);
    BenchmarkResult pmem_to_remote = benchmark_memory_ops(PMEM_NODE, REMOTE_NODE, mode);
    BenchmarkResult pmem_to_pmem = benchmark_memory_ops(PMEM_NODE, PMEM_NODE, mode);

    AccessResult* local_local_access = &local_to_local.source_access;
    AccessResult* local_remote_access = &local_to_remote.dest_access;
    AccessResult* local_pmem_access = &local_to_pmem.dest_access;
    AccessResult* remote_local_access = &remote_to_local.dest_access;
    AccessResult* remote_remote_access = &remote_to_remote.source_access;
    AccessResult* remote_pmem_access = &remote_to_pmem.dest_access;
    AccessResult* pmem_local_access = &pmem_to_local.dest_access;
    AccessResult* pmem_remote_access = &pmem_to_remote.dest_access;
    AccessResult* pmem_pmem_access = &pmem_to_pmem.source_access;

    print_access_latency_table_with_tlb(mode_str,
        local_local_access, local_remote_access, local_pmem_access,
        remote_local_access, remote_remote_access, remote_pmem_access,
        pmem_local_access, pmem_remote_access, pmem_pmem_access);

    // Print Migration Latency table
    print_migration_latency_table(mode_str,
        local_to_remote.move_time, local_to_pmem.move_time,
        remote_to_local.move_time, remote_to_pmem.move_time,
        pmem_to_local.move_time, pmem_to_remote.move_time);
}

int main() {
    printf("NUMA and PMEM Page Allocation, Migration, and Access Benchmark with TLB Monitoring\n");

    check_cpu_features();

    if (numa_available() < 0) {
        fprintf(stderr, "NUMA is not supported on this system\n");
        return 1;
    }

    srand(time(NULL));

    printf("Running benchmarks for different memory access methods...\n");

    // Run READ benchmark
    run_benchmark_for_mode(READ);

    // Run different WRITE method benchmarks
    run_benchmark_for_mode(WRITE_NORMAL);
    run_benchmark_for_mode(WRITE_NTSTORE);
    run_benchmark_for_mode(WRITE_CLFLUSH);

    if (has_clflushopt) {
        run_benchmark_for_mode(WRITE_CLFLUSHOPT);
    }

    if (has_clwb) {
        run_benchmark_for_mode(WRITE_CLWB);
    }

    printf("\nBenchmarks complete.\n");

    return 0;
}