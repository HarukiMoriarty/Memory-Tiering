#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <linux/mempolicy.h>
#include <sys/syscall.h>
#include <string.h>
#include <numa.h>
#include <emmintrin.h>

#define PAGE_SIZE 4096
#define PAGE_NUM 100000
#define ITERATIONS 1
#define OFFSET_COUNT 100000

typedef enum {
    READ = 0,
    WRITE = 1,
    READ_WRITE = 2,
} MemAccessMode;

uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void flush_cache(void* addr) {
    _mm_clflush(addr);
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

    // Step 5: Verify the memory is actually on the requested NUMA node
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

// Move multiple pages to another NUMA node
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

    // Populate pages array and nodes array
    for (size_t i = 0; i < number; i++) {
        pages[i] = (char*)addr + i * size;
        nodes[i] = target_node;
    }

    // Move pages
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

// Improved function to accurately measure memory access latency
uint64_t access_memory(void* addr, int* offsets, int offset_count, MemAccessMode mode) {
    uint64_t total_time = 0;
    
    // Perform multiple measurements for statistical validity
    for (int i = 0; i < offset_count; i++) {
        int page_idx = offsets[i] / (PAGE_SIZE / sizeof(uint64_t));
        int offset_in_page = (offsets[i] % (PAGE_SIZE / sizeof(uint64_t)));
        volatile uint64_t* access_addr = (volatile uint64_t*)addr + 
                                         page_idx * (PAGE_SIZE / sizeof(uint64_t)) + 
                                         offset_in_page;
        
        // Force a complete cache flush of the target address
        _mm_clflush((void*)access_addr);
        _mm_mfence(); // Ensure flush completes before measurement
        _mm_lfence(); // Prevent instruction reordering
        
        uint64_t start = get_time_ns();
        
        if (mode == READ) {
            // Force a read from memory
            uint64_t value;
            asm volatile(
                "movq (%1), %0"
                : "=r" (value)
                : "r" (access_addr)
                : "memory"
            );
        }
        else if (mode == WRITE) {
            uint64_t value = 0xDEADBEEF;
            asm volatile(
                "movnti %1, (%0)\n\t"           
                :
                : "r" (access_addr), "r" (value)
                : "memory"
            );
        }
        
        _mm_lfence(); // Prevent instruction reordering
        _mm_mfence(); // Ensure flush completes before measurement
        
        uint64_t end = get_time_ns();
        total_time += (end - start);
    }
    
    return total_time / offset_count;
}

typedef struct {
    double alloc_time;
    double access_time;
    double move_time;
    double moved_access_time;
} BenchmarkResult;

BenchmarkResult benchmark_memory_ops(int source_node, int target_node, MemAccessMode mode) {
    BenchmarkResult result = {0};
    
    // Allocate memory on source node
    uint64_t start_time = get_time_ns();
    void* memory = allocate_and_bind_to_numa(PAGE_SIZE, PAGE_NUM, source_node);
    result.alloc_time = (double)(get_time_ns() - start_time);
    
    if (!memory) {
        fprintf(stderr, "Failed to allocate memory on node %d\n", source_node);
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
    result.access_time = (double)access_memory(memory, offsets, OFFSET_COUNT, mode);
    
    // Move memory to target node
    start_time = get_time_ns();
    move_pages_to_node(memory, PAGE_SIZE, PAGE_NUM, target_node);
    result.move_time = (double)(get_time_ns() - start_time) / PAGE_NUM;
    
    // Access memory after moving to target node
    result.moved_access_time = (double)access_memory(memory, offsets, OFFSET_COUNT, mode);
    
    // Cleanup
    free(offsets);
    munmap(memory, PAGE_SIZE * PAGE_NUM);
    
    return result;
}

int main() {
    printf("NUMA and PMEM Page Allocation, Migration, and Access Benchmark\n");

    // Check NUMA availability
    if (numa_available() < 0) {
        fprintf(stderr, "NUMA is not supported on this system\n");
        return 1;
    }

    // Set random seed
    srand(time(NULL));
    
    // Define node configurations
    const int LOCAL_NODE = 0;   // Local DRAM
    const int REMOTE_NODE = 1;  // Remote DRAM
    const int PMEM_NODE = 2;    // Persistent Memory
    
    printf("Running benchmarks...\n");
    
    // Benchmark 1: Local DRAM -> Remote DRAM
    printf("\n--- Local DRAM to Remote DRAM Benchmark ---\n");
    BenchmarkResult local_to_remote = benchmark_memory_ops(LOCAL_NODE, REMOTE_NODE, READ);
    printf("Local DRAM allocation time: %.2f ns\n", local_to_remote.alloc_time);
    printf("Local DRAM access time: %.2f ns\n", local_to_remote.access_time);
    printf("Migration time to Remote DRAM: %.2f ns per page\n", local_to_remote.move_time);
    printf("Remote DRAM access time after migration: %.2f ns\n", local_to_remote.moved_access_time);

    // Benchmark 2: Remote DRAM -> Local DRAM
    printf("\n--- Remote DRAM to Local DRAM Benchmark ---\n");
    BenchmarkResult remote_to_local = benchmark_memory_ops(REMOTE_NODE, LOCAL_NODE, READ);
    printf("Remote DRAM allocation time: %.2f ns\n", remote_to_local.alloc_time);
    printf("Remote DRAM access time: %.2f ns\n", remote_to_local.access_time);
    printf("Migration time to Local DRAM: %.2f ns per page\n", remote_to_local.move_time);
    printf("Local DRAM access time after migration: %.2f ns\n", remote_to_local.moved_access_time);

    // Benchmark 3: Local DRAM -> PMEM
    printf("\n--- Local DRAM to PMEM Benchmark ---\n");
    BenchmarkResult local_to_pmem = benchmark_memory_ops(LOCAL_NODE, PMEM_NODE, READ);
    printf("Local DRAM allocation time: %.2f ns\n", local_to_pmem.alloc_time);
    printf("Local DRAM access time: %.2f ns\n", local_to_pmem.access_time);
    printf("Migration time to PMEM: %.2f ns per page\n", local_to_pmem.move_time);
    printf("PMEM access time after migration: %.2f ns\n", local_to_pmem.moved_access_time);

    // Benchmark 4: Remote DRAM -> PMEM
    printf("\n--- Remote DRAM to PMEM Benchmark ---\n");
    BenchmarkResult remote_to_pmem = benchmark_memory_ops(REMOTE_NODE, PMEM_NODE, READ);
    printf("Remote DRAM allocation time: %.2f ns\n", remote_to_pmem.alloc_time);
    printf("Remote DRAM access time: %.2f ns\n", remote_to_pmem.access_time);
    printf("Migration time to PMEM: %.2f ns per page\n", remote_to_pmem.move_time);
    printf("PMEM access time after migration: %.2f ns\n", remote_to_pmem.moved_access_time);

    // Benchmark 5: PMEM -> Remote DRAM
    printf("\n--- PMEM to Remote DRAM Benchmark ---\n");
    BenchmarkResult pmem_to_remote = benchmark_memory_ops(PMEM_NODE, REMOTE_NODE, READ);
    printf("PMEM allocation time: %.2f ns\n", pmem_to_remote.alloc_time);
    printf("PMEM access time: %.2f ns\n", pmem_to_remote.access_time);
    printf("Migration time to Remote DRAM: %.2f ns per page\n", pmem_to_remote.move_time);
    printf("Remote DRAM access time after migration: %.2f ns\n", pmem_to_remote.moved_access_time);
    
    // Benchmark 6: PMEM -> Local DRAM
    printf("\n--- PMEM to Local DRAM Benchmark ---\n");
    BenchmarkResult pmem_to_local = benchmark_memory_ops(PMEM_NODE, LOCAL_NODE, READ);
    printf("PMEM allocation time: %.2f ns\n", pmem_to_local.alloc_time);
    printf("PMEM access time: %.2f ns\n", pmem_to_local.access_time);
    printf("Migration time to Local DRAM: %.2f ns per page\n", pmem_to_local.move_time);
    printf("Local DRAM access time after migration: %.2f ns\n", pmem_to_local.moved_access_time);

    // Now run the same tests with WRITE operations
    printf("\n\n=== WRITE OPERATIONS BENCHMARKS ===\n");
    
    // Benchmark 1: Local DRAM -> Remote DRAM (WRITE)
    printf("\n--- Local DRAM to Remote DRAM Benchmark (WRITE) ---\n");
    local_to_remote = benchmark_memory_ops(LOCAL_NODE, REMOTE_NODE, WRITE);
    printf("Local DRAM allocation time: %.2f ns\n", local_to_remote.alloc_time);
    printf("Local DRAM write time: %.2f ns\n", local_to_remote.access_time);
    printf("Migration time to Remote DRAM: %.2f ns per page\n", local_to_remote.move_time);
    printf("Remote DRAM write time after migration: %.2f ns\n", local_to_remote.moved_access_time);

    // Benchmark 2: Remote DRAM -> Local DRAM (WRITE)
    printf("\n--- Remote DRAM to Local DRAM Benchmark (WRITE) ---\n");
    remote_to_local = benchmark_memory_ops(REMOTE_NODE, LOCAL_NODE, WRITE);
    printf("Remote DRAM allocation time: %.2f ns\n", remote_to_local.alloc_time);
    printf("Remote DRAM write time: %.2f ns\n", remote_to_local.access_time);
    printf("Migration time to Local DRAM: %.2f ns per page\n", remote_to_local.move_time);
    printf("Local DRAM write time after migration: %.2f ns\n", remote_to_local.moved_access_time);

    // Benchmark 3: Local DRAM -> PMEM (WRITE)
    printf("\n--- Local DRAM to PMEM Benchmark (WRITE) ---\n");
    local_to_pmem = benchmark_memory_ops(LOCAL_NODE, PMEM_NODE, WRITE);
    printf("Local DRAM allocation time: %.2f ns\n", local_to_pmem.alloc_time);
    printf("Local DRAM write time: %.2f ns\n", local_to_pmem.access_time);
    printf("Migration time to PMEM: %.2f ns per page\n", local_to_pmem.move_time);
    printf("PMEM write time after migration: %.2f ns\n", local_to_pmem.moved_access_time);

    // Benchmark 4: Remote DRAM -> PMEM (WRITE)
    printf("\n--- Remote DRAM to PMEM Benchmark (WRITE) ---\n");
    remote_to_pmem = benchmark_memory_ops(REMOTE_NODE, PMEM_NODE, WRITE);
    printf("Remote DRAM allocation time: %.2f ns\n", remote_to_pmem.alloc_time);
    printf("Remote DRAM write time: %.2f ns\n", remote_to_pmem.access_time);
    printf("Migration time to PMEM: %.2f ns per page\n", remote_to_pmem.move_time);
    printf("PMEM write time after migration: %.2f ns\n", remote_to_pmem.moved_access_time);

    // Benchmark 5: PMEM -> Remote DRAM (WRITE)
    printf("\n--- PMEM to Remote DRAM Benchmark (WRITE) ---\n");
    pmem_to_remote = benchmark_memory_ops(PMEM_NODE, REMOTE_NODE, WRITE);
    printf("PMEM allocation time: %.2f ns\n", pmem_to_remote.alloc_time);
    printf("PMEM write time: %.2f ns\n", pmem_to_remote.access_time);
    printf("Migration time to Remote DRAM: %.2f ns per page\n", pmem_to_remote.move_time);
    printf("Remote DRAM write time after migration: %.2f ns\n", pmem_to_remote.moved_access_time);
    
    // Benchmark 6: PMEM -> Local DRAM (WRITE)
    printf("\n--- PMEM to Local DRAM Benchmark (WRITE) ---\n");
    pmem_to_local = benchmark_memory_ops(PMEM_NODE, LOCAL_NODE, WRITE);
    printf("PMEM allocation time: %.2f ns\n", pmem_to_local.alloc_time);
    printf("PMEM write time: %.2f ns\n", pmem_to_local.access_time);
    printf("Migration time to Local DRAM: %.2f ns per page\n", pmem_to_local.move_time);
    printf("Local DRAM write time after migration: %.2f ns\n", pmem_to_local.moved_access_time);

    return 0;
}