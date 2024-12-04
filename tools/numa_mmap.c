#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <linux/mempolicy.h>
#include <sys/syscall.h>
#include <errno.h>
#include <emmintrin.h>

#define PAGE_SIZE 4096 // Assuming 4KB page size
#define ITERATIONS 100 // Number of runs

// Function to get the current timestamp in nanoseconds
uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Flush cache line
void flush_cache(void* addr) {
    _mm_clflush(addr);
}

// Allocate memory using mmap
void* allocate_page() {
    void* mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

// Move a single page to another NUMA node
void move_page_to_node(void* addr, int target_node) {
    void* pages[1] = { addr };
    int nodes[1] = { target_node };
    int status[1];

    if (syscall(SYS_move_pages, 0, 1, pages, nodes, status, MPOL_MF_MOVE) != 0) {
        perror("move_pages failed");
        exit(EXIT_FAILURE);
    }
}

// Access the memory and measure the time taken
uint64_t access_page(void* addr) {
    flush_cache(addr);
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t start_time = get_time_ns();

    // Perform a simple read/write operation
    *page = 44;          // Write to the page
    volatile uint64_t value = *page; // Read from the page

    (void)value; // Prevent unused variable warning
    return get_time_ns() - start_time;
}

int main() {
    printf("NUMA Page Allocation, Migration, and Access Benchmark\n");

    uint64_t total_alloc_time = 0;
    uint64_t total_local_access_time = 0;
    uint64_t total_move_time = 0;
    uint64_t total_remote_access_time = 0;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Step 1: Allocate a page and measure the allocation time
        uint64_t start_time = get_time_ns();
        void* page = allocate_page();
        uint64_t alloc_time = get_time_ns() - start_time;

        // Step 2: Access the page locally and measure the access time
        uint64_t local_access_time = access_page(page);

        // Step 3: Move the page to another NUMA node and measure the migration time
        start_time = get_time_ns();
        move_page_to_node(page, 1); // Move to NUMA node 1
        uint64_t move_time = get_time_ns() - start_time;

        // Step 4: Access the page on the remote NUMA node and measure the access time
        uint64_t remote_access_time = access_page(page);

        // Cleanup
        munmap(page, PAGE_SIZE);

        // Accumulate times
        total_alloc_time += alloc_time;
        total_local_access_time += local_access_time;
        total_move_time += move_time;
        total_remote_access_time += remote_access_time;
    }

    // Calculate and print the averages
    double avg_alloc_time = (double)total_alloc_time / ITERATIONS;
    double avg_local_access_time = (double)total_local_access_time / ITERATIONS;
    double avg_move_time = (double)total_move_time / ITERATIONS;
    double avg_remote_access_time = (double)total_remote_access_time / ITERATIONS;

    printf("Average time to allocate a page on local NUMA node: %.2f ns\n", avg_alloc_time);
    printf("Average time to access a page on local NUMA node: %.2f ns\n", avg_local_access_time);
    printf("Average time to move a page to another NUMA node: %.2f ns\n", avg_move_time);
    printf("Average time to access a page on remote NUMA node: %.2f ns\n", avg_remote_access_time);

    return 0;
}
