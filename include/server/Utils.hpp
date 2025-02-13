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

//======================================
// Constants and Configurations
//======================================

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096  // Default system page size
#endif

#define PAGE_NUM 100000      // Number of pages to allocate
#define ITERATIONS 1         // Number of test iterations
#define OFFSET_COUNT 100000  // Number of random offsets for testing

/**
 * Memory access modes for benchmarking
 */
typedef enum {
    READ = 0,        // Read-only operations
    WRITE = 1,       // Write-only operations
    READ_WRITE = 2,  // Combined read-write operations
} mem_access_mode;

//======================================
// Utility Functions
//======================================

/**
 * Get current timestamp in nanoseconds
 * @return Current time in nanoseconds
 */
inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * Flush cache line to ensure memory operation isn't served from CPU cache
 * @param addr Address to flush from cache
 */
inline void flush_cache(void* addr) {
    _mm_clflush(addr);
}

/**
 * Initialize random offset array for memory access patterns
 * @return Array of random page offsets
 */
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
// Memory Allocation
//======================================

/**
 * Allocate memory pages on DRAM
 * @param size Size of each page
 * @param number Number of pages to allocate
 * @return Pointer to allocated memory
 */
inline void* allocate_pages(size_t size, size_t number) {
    void* mem = mmap(NULL, size * number,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
        -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

/**
 * Allocate and bind memory pages to specific NUMA node
 * @param size Size of each page
 * @param number Number of pages to allocate
 * @param numa_node Target NUMA node
 * @return Pointer to allocated memory
 */
inline void* allocate_and_bind_to_numa(size_t size, size_t number, int numa_node) {
    // Allocate memory
    void* addr = mmap(NULL, size * number,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }

    // Bind to NUMA node
    unsigned long nodemask = (1UL << numa_node);
    if (syscall(SYS_mbind, addr, size, MPOL_BIND,
        &nodemask, sizeof(nodemask) * 8,
        MPOL_MF_MOVE | MPOL_MF_STRICT) != 0) {
        perror("mbind syscall failed");
        munmap(addr, size * number);
        return NULL;
    }

    // Initialize memory
    memset(addr, 0, size * number);
    return addr;
}

//======================================
// Page Migration
//======================================

/**
 * Move a single page to specified NUMA node
 * @param addr Page address
 * @param target_node Target NUMA node
 */
inline void move_page_to_node(void* addr, int target_node) {
    void* pages[1] = { addr };
    int nodes[1] = { target_node };
    int status[1];

    if (syscall(SYS_move_pages, 0, 1, pages, nodes, status, MPOL_MF_MOVE) != 0) {
        perror("move_pages failed");
        exit(EXIT_FAILURE);
    }
}

/**
 * Move multiple pages to specified NUMA node
 * @param addr Starting address of pages
 * @param size Size of each page
 * @param number Number of pages to move
 * @param target_node Target NUMA node
 */
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

    // Setup page and node arrays
    for (size_t i = 0; i < number; i++) {
        pages[i] = (char*)addr + i * size;
        nodes[i] = target_node;
    }

    // Perform migration
    if (syscall(SYS_move_pages, 0, number, pages, nodes, status, MPOL_MF_MOVE) != 0) {
        perror("move_pages failed");
        free(pages);
        free(nodes);
        free(status);
        exit(EXIT_FAILURE);
    }

    // Check migration status
    for (size_t i = 0; i < number; i++) {
        if (status[i] < 0) {
            fprintf(stderr, "Failed to move page %zu: error code %d\n", i, status[i]);
        }
    }

    free(pages);
    free(nodes);
    free(status);
}

/**
 * Migrate a page between memory tiers
 * @param addr Page address
 * @param current_tier Current memory tier
 * @param target_tier Target memory tier
 */
inline void migrate_page(void* addr, PageLayer current_tier, PageLayer target_tier) {
    int target_node = (target_tier == PageLayer::NUMA_LOCAL) ? 0 :
        (target_tier == PageLayer::NUMA_REMOTE) ? 1 : 2;
    return move_page_to_node(addr, target_node);
}

//======================================
// Memory Access Operations
//======================================

/**
 * Access a specific memory page
 * @param addr Page address
 * @param mode Memory access mode (read/write)
 * @return Access time in nanoseconds
 */
inline uint64_t access_page(void* addr, mem_access_mode mode) {
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t start_time = get_time_ns();

    flush_cache(addr);
    uint64_t value_1, value_2 = 44;

    switch (mode) {
    case READ:
        memcpy(&value_1, (const void*)&page, sizeof(uint64_t));
        break;
    case WRITE:
        memcpy((void*)&page, &value_2, sizeof(uint64_t));
        break;
    default:
        break;
    }

    return (get_time_ns() - start_time);
}

/**
 * Access random memory pages and measure time
 * @param addr Base address
 * @param offsets Array of random offsets
 * @param mode Memory access mode
 * @return Total access time in nanoseconds
 */
inline uint64_t access_random_page(void* addr, int* offsets, mem_access_mode mode) {
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t start_time = get_time_ns();

    for (size_t i = 0; i < OFFSET_COUNT; i++) {
        flush_cache(addr);
        int offset = offsets[i];
        uint64_t value_1, value_2 = 44;

        switch (mode) {
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

    return (get_time_ns() - start_time);
}

#endif // UTILS_HPP
