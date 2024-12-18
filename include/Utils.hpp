#ifndef UTILS_HPP
#define UTILS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <linux/mempolicy.h>
#include <sys/syscall.h>
#include <errno.h>
#include <emmintrin.h>
#include <string.h>

// Default page size (adjust if needed)
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define PAGE_NUM 100000
#define ITERATIONS 1
#define PMEM_FILE "/mnt/pmem1-aos/latency_test"
#define MAP_SIZE 2097152       
#define OFFSET_COUNT 100000

typedef enum  {
    READ = 0,
    WRITE = 1,
    READ_WRITE = 2,
} mem_access_mode;

// Get the current timestamp in nanoseconds
inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Flush a cache line to ensure the memory operation is not served from CPU cache
inline void flush_cache(void* addr) {
    _mm_clflush(addr);
}

// Initialize the random offset map (used before each benchmark iteration)
inline int* init_offsets() {
    int* offsets = (int*)malloc(OFFSET_COUNT * sizeof(int));
    if (!offsets) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < OFFSET_COUNT; i++) {
        offsets[i] = (rand() % PAGE_NUM) * PAGE_SIZE / sizeof(uint64_t);
    }

    return offsets;
}

//======================================
// Allocation
//======================================

// Allocate memory pages on DRAM
// size: size of each page (e.g., PAGE_SIZE)
// number: number of pages to allocate
inline void* allocate_pages(size_t size, size_t number) {
    void* mem = mmap(NULL, size * number, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

// Allocate pages on PMEM
// fd: file descriptor of the PMEM file/region
// size: size of each page
// number: number of pages to allocate
inline void* allocate_pmem_pages(int fd, size_t size, size_t number) {
    void* mem = mmap(NULL, size * number, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap PMEM failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

//======================================
// Page Migration
//======================================

// Move a single page to another NUMA node
// addr: address of the page start
// target_node: target NUMA node
inline void move_page_to_node(void* addr, int target_node) {
    void* pages[1] = { addr };
    int nodes[1] = { target_node };
    int status[1];

    if (syscall(SYS_move_pages, 0, 1, pages, nodes, status, MPOL_MF_MOVE) != 0) {
        perror("move_pages failed");
        exit(EXIT_FAILURE);
    }
}

// Move multiple pages starting from 'addr' to another NUMA node
// addr: starting address of pages
// size: size of each page (e.g., PAGE_SIZE)
// number: number of pages to move
// target_node: target NUMA node
inline void move_pages_to_node(void* addr, size_t size, size_t number, int target_node) {
    void** pages = (void**)malloc(number * sizeof(void*));
    int* nodes = (int*)malloc(number * sizeof(int));
    int* status = (int*)malloc(number * sizeof(int));
    if (!pages || !nodes || !status) {
        perror("Memory allocation failed");
        free(pages);
        free(nodes);
        free(status);
        exit(EXIT_FAILURE);
    }

    // Populate pages array and nodes array
    for (size_t i = 0; i < number; i++) {
        pages[i] = (char*)addr + i * size; // Calculate the address of each page
        nodes[i] = target_node;           // Set target node for each page
    }

    // Move pages
    if (syscall(SYS_move_pages, 0, number, pages, nodes, status, MPOL_MF_MOVE) != 0) {
        perror("move_pages failed");
        free(pages);
        free(nodes);
        free(status);
        exit(EXIT_FAILURE);
    }

    // Check the status array for results
    for (size_t i = 0; i < number; i++) {
        if (status[i] < 0) {
            fprintf(stderr, "Failed to move page %zu: error code %d\n", i, status[i]);
        }
    }

    free(pages);
    free(nodes);
    free(status);
}

// Migrate one page from one tier to another
inline void migrate_page(void* addr, PageLayer current_tier, PageLayer target_tier) {
    // TODO: Handle migration logic
    // Case 1: If both current and target tiers are NUMA nodes
    if ((current_tier == PageLayer::NUMA_LOCAL || current_tier == PageLayer::NUMA_REMOTE) &&
        (target_tier == PageLayer::NUMA_LOCAL || target_tier == PageLayer::NUMA_REMOTE)) {
        // TODO: simply use move_page_to_node
        // numa_local = 0, numa_remote = 1
        int target_node = target_tier == PageLayer::NUMA_LOCAL ? 0 : 1;
        move_page_to_node(addr, target_node);
    }
    // Case 2: If one tier is in PMEM
    else if (current_tier == PageLayer::PMEM) {
        // TODO: select one page in target_tier for exchange
    }
    else if (target_tier == PageLayer::PMEM) {
        // TODO: select one page in current_tier for exchange
    }
    else {
        // TODO: potential corner case...
    }
}

//======================================
// Memory Access
//======================================

// Access the specified memory page
inline uint64_t access_page(void* addr, mem_access_mode mode) {
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t start_time = get_time_ns();

    flush_cache(addr);
    uint64_t value_1, value_2 = 44;
    // Perform a simple read/write operation
    switch (mode)
    {
    case READ:
        memcpy(&value_1, (const void*)&page, sizeof(uint64_t));
        break;
    case WRITE:
        memcpy((void*)&page, &value_2, sizeof(uint64_t));
        break;
    default:
        break;
    }

    return (get_time_ns() - start_time)/OFFSET_COUNT;
}

// Access the random memory and measure the time taken
inline uint64_t access_random_page(void* addr, int* offsets, mem_access_mode mode) {
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t start_time = get_time_ns();

    for (size_t i = 0; i < OFFSET_COUNT; i++) {
        flush_cache(addr);
        int offset = offsets[i];
        uint64_t value_1, value_2 = 44;
        // Perform a simple read/write operation
        switch (mode)
        {
        case READ:
            memcpy(&value_1, (const void*)&page[offset], sizeof(uint64_t));
            break;
        case WRITE:
            memcpy((void*)&page[offset], &value_2, sizeof(uint64_t));
            break;
        default:
            break;
        }
    }

    return (get_time_ns() - start_time)/OFFSET_COUNT;
}

#endif // UTILS_HPP
