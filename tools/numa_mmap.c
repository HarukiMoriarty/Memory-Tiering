#define _GNU_SOURCE
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

#define PAGE_SIZE 4096
#define ITERATIONS 1000 
#define PMEM_FILE "/mnt/pmem-aos/latency_test"
#define DAX_DEVICE "/dev/dax1.0"  
#define MAP_SIZE 2097152       

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

// Allocate memory on PMEM using mmap
void* allocate_pmem_page(int fd, size_t size) {
    void* mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap PMEM failed");
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
    *page = 44;     // Write to a random offset
    volatile uint64_t value = *page; // Read from the page

    return get_time_ns() - start_time;
}

int main() {
    printf("NUMA and PMEM Page Allocation, Migration, and Access Benchmark\n");

    // Create and prepare the PMEM file
    int fd = open(PMEM_FILE, O_RDWR | O_CREAT | O_DIRECT, 0666);
    if (fd < 0) {
        perror("open PMEM file failed");
        return 1;
    }

    // Resize the PMEM file to fit PAGE_SIZE
    if (ftruncate(fd, PAGE_SIZE) != 0) {
        perror("ftruncate PMEM file failed");
        close(fd);
        return 1;
    }

    // Access PMEM via devdax
    int dax_fd = open(DAX_DEVICE, O_RDWR);
    if (dax_fd < 0) {
        perror("open failed asd");
        return 1;
    }

    // Map the device into memory
    void* mapped_addr = allocate_pmem_page(dax_fd, MAP_SIZE);

    uint64_t total_alloc_time = 0;
    uint64_t total_local_access_time = 0;
    uint64_t total_move_time = 0;
    uint64_t total_remote_access_time = 0;

    uint64_t total_pmem_alloc_time = 0;
    uint64_t total_pmem_access_time = 0;
    uint64_t total_pmem_devdax_alloc_time = 0;
    uint64_t total_pmem_devdax_access_time = 0;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Step 1: Allocate a DRAM page and measure the allocation time
        uint64_t start_time = get_time_ns();
        void* dram_page = allocate_page();
        uint64_t alloc_time = get_time_ns() - start_time;

        // Step 2: Access the DRAM page locally and measure the access time
        uint64_t local_access_time = access_page(dram_page);

        // Step 3: Move the DRAM page to another NUMA node and measure the migration time
        start_time = get_time_ns();
        move_page_to_node(dram_page, 1); // Move to NUMA node 1
        uint64_t move_time = get_time_ns() - start_time;

        // Step 4: Access the DRAM page on the remote NUMA node and measure the access time
        uint64_t remote_access_time = access_page(dram_page);

        // Cleanup DRAM page
        munmap(dram_page, PAGE_SIZE);

        // Step 5: Allocate a PMEM page and measure the allocation time
        start_time = get_time_ns();
        void* pmem_page = allocate_pmem_page(fd, PAGE_SIZE);
        uint64_t pmem_alloc_time = get_time_ns() - start_time;

        // Step 6: Access the PMEM page and measure the access time
        uint64_t pmem_access_time = access_page(pmem_page);

        // Cleanup PMEM page
        munmap(pmem_page, PAGE_SIZE);

        // Step 7: Allocate a PMEM range via devdax and measure the allocation time
        start_time = get_time_ns();
        void* pmem_devdax_range = allocate_pmem_page(dax_fd, PAGE_SIZE);
        uint64_t pmem_devdax_alloc_time = get_time_ns() - start_time;

        // Step 8: Access the PMEM range via devdax and measure the access time
        uint64_t pmem_devdax_access_time = access_page(pmem_devdax_range);

        // Accumulate times
        total_alloc_time += alloc_time;
        total_local_access_time += local_access_time;
        total_move_time += move_time;
        total_remote_access_time += remote_access_time;
        total_pmem_alloc_time += pmem_alloc_time;
        total_pmem_access_time += pmem_access_time;
        total_pmem_devdax_alloc_time += pmem_devdax_alloc_time;
        total_pmem_devdax_access_time += pmem_devdax_access_time;
    }

    close(fd);

    // Calculate and print the averages
    double avg_alloc_time = (double)total_alloc_time / ITERATIONS;
    double avg_local_access_time = (double)total_local_access_time / ITERATIONS;
    double avg_move_time = (double)total_move_time / ITERATIONS;
    double avg_remote_access_time = (double)total_remote_access_time / ITERATIONS;
    double avg_pmem_alloc_time = (double)total_pmem_alloc_time / ITERATIONS;
    double avg_pmem_access_time = (double)total_pmem_access_time / ITERATIONS;
    double avg_pmem_devdax_alloc_time = (double)total_pmem_devdax_alloc_time / ITERATIONS;
    double avg_pmem_devdax_access_time = (double)total_pmem_devdax_access_time / ITERATIONS;

    printf("Average time to allocate a DRAM page: %.2f ns\n", avg_alloc_time);
    printf("Average time to access a DRAM page locally: %.2f ns\n", avg_local_access_time);
    printf("Average time to move a DRAM page to another NUMA node: %.2f ns\n", avg_move_time);
    printf("Average time to access a DRAM page on a remote NUMA node: %.2f ns\n", avg_remote_access_time);
    printf("Average time to allocate a PMEM page: %.2f ns\n", avg_pmem_alloc_time);
    printf("Average time to access a PMEM page: %.2f ns\n", avg_pmem_access_time);
    printf("Average time to allocate a PMEM range via devdax: %.2f ns\n", avg_pmem_devdax_alloc_time);
    printf("Average time to access a PMEM range via devdax: %.2f ns\n", avg_pmem_devdax_access_time);

    return 0;
}
