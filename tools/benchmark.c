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
#include <string.h>
#include <numa.h> 

#define PAGE_SIZE 4096
#define PAGE_NUM 100000
#define ITERATIONS 1
// #define PMEM_FILE "/mnt/pmem1-aos/latency_test"  

int page_access_track[PAGE_NUM] = { 0 };
int offset_count = 100000;
int* offsets;

typedef enum {
    READ = 0,
    WRITE = 1,
    READ_WRITE = 2,
} mem_access_mode;

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

// Allocate memory using mmap (total PAGE_NUM pages)
void* allocate_pages(size_t size, size_t number) {
    void* mem = mmap(NULL, size * number, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

// // Allocate memory on PMEM using mmap (total PAGE_NUM pages)
// void* allocate_pmem_pages(int fd, size_t size, size_t number) {
//     void* mem = mmap(NULL, size * number, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
//     if (mem == MAP_FAILED) {
//         perror("mmap PMEM failed");
//         exit(EXIT_FAILURE);
//     }
//     return mem;
// }

// Allocate and bind memory to a specific NUMA node, ensuring physical allocation
void* allocate_and_bind_to_numa(size_t size, size_t number, int numa_node) {
    // Step 1: Allocate memory using mmap
    void* addr = mmap(NULL, size * number, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }
    // Step 2: Define the nodemask for the target NUMA node
    unsigned long nodemask = (1UL << numa_node);
    // Step 3: Directly call the mbind syscall to bind memory to the NUMA node
    if (syscall(SYS_mbind, addr, size, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0) {
        perror("mbind syscall failed");
        munmap(addr, size);
        return NULL;
    }

    // Step 4: Access (touch) the memory to ensure physical allocation
    memset(addr, 0, size * number); // Initialize all memory to zero
    return addr;
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
        pages[i] = (char*)addr + i * size; // Calculate the address of each page
        nodes[i] = target_node;          // Set target node for each page
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

// Access the random memory and measure the time taken
uint64_t access_random_page(void* addr, size_t page_num, mem_access_mode mode) {
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t total_time = 0;

    for (size_t i = 0; i < offset_count; i++) {

        int offset = offsets[i];

        uint64_t start_time = get_time_ns();
        flush_cache(addr);
        // Perform a simple read/write operation
        switch (mode)
        {
        case READ:
            uint64_t value_1;
            memcpy(&value_1, (const void*)&page[offset], sizeof(uint64_t)); // Read from the page
            break;
        case WRITE:
            uint64_t value_2 = 44;
            memcpy((void*)&page[offset], &value_2, sizeof(uint64_t));     // Write to a random page
            break;
        case READ_WRITE:
            // The following method should not work might because not write through
            // value = page[offset];    // Read from the page
            // page[offset] = 44;       // Write to a random page
            uint64_t value_3;
            memcpy(&value_3, (const void*)&page[offset], sizeof(uint64_t));
            uint64_t value_4 = 44;
            memcpy((void*)&page[offset], &value_4, sizeof(uint64_t));
            break;
        default:
            break;
        }

        total_time += get_time_ns() - start_time;
    }

#ifdef DEBUG
    page_access_track[random_page]++;
#endif

    return total_time / offset_count;
}

void print_page_access_track() {
    // print in a more compact way (10 per line)
    for (int i = 0; i < PAGE_NUM; i++) {
        if (i % 10 == 0) {
            printf("\n");
        }
        printf("%d ", page_access_track[i]);
    }
    printf("\n");
    // clear the page access track
    for (int i = 0; i < PAGE_NUM; i++) {
        page_access_track[i] = 0;
    }
}

int main() {
    printf("NUMA and PMEM Page Allocation, Migration, and Access Benchmark\n");

   if (numa_available() < 0) {
        fprintf(stderr, "NUMA is not supported on this system\n");
        return 1;
    }

    // Set the random seed
    srand(time(NULL));

    // Generate random offsets
    offsets = (int*)malloc(offset_count * sizeof(int));
    for (size_t i = 0; i < offset_count; i++) {
        offsets[i] = (rand() % PAGE_NUM) * PAGE_SIZE / sizeof(uint64_t);
    };

    // Create and prepare the PMEM file
    // int fd = open(PMEM_FILE, O_RDWR, 0666);
    // if (fd < 0) {
    //     perror("open PMEM file failed");
    //     return 1;
    // }

    uint64_t total_alloc_time = 0;
    uint64_t total_local_access_time = 0;

    uint64_t numa_move_time = 0;
    uint64_t pmem_move_time = 0;
    uint64_t total_remote_access_time = 0;
    uint64_t total_pmem_move_access_time = 0;

    uint64_t remote_alloc_time = 0;
    uint64_t total_remote_to_pmem_access_time = 0;
    uint64_t total_remote_alloc_access_time = 0;

    uint64_t total_pmem_alloc_time = 0;
    uint64_t total_pmem_access_time = 0;
    uint64_t total_pmem_to_remote_access_time = 0;

    // Allocate a DRAM page and measure the allocation time
    uint64_t start_time = get_time_ns();
    void* dram_page = allocate_pages(PAGE_SIZE, PAGE_NUM);
    uint64_t alloc_time = get_time_ns() - start_time;
    total_alloc_time += alloc_time;

    // Access the DRAM page locally and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_local_access_time += access_random_page(dram_page, PAGE_NUM, READ);
    }

#ifdef DEBUG
    print_page_access_track();
#endif

    // Move the DRAM page to another NUMA node and measure the migration time
    start_time = get_time_ns();
    move_pages_to_node(dram_page, PAGE_SIZE, PAGE_NUM, 1); // Move to NUMA node 1
    uint64_t move_time = get_time_ns() - start_time;
    numa_move_time += move_time;

    // Access the DRAM page on the remote NUMA node and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_remote_access_time += access_random_page(dram_page, PAGE_NUM, READ);
    }
    
    // Cleanup DRAM page
    munmap(dram_page, PAGE_SIZE*PAGE_NUM);

    dram_page = allocate_pages(PAGE_SIZE, PAGE_NUM);
    // Also move the page from Local DRAM to PMEM
    start_time = get_time_ns();
    move_pages_to_node(dram_page, PAGE_SIZE, PAGE_NUM, 2); // Move to NUMA node 2
    move_time = get_time_ns() - start_time;
    pmem_move_time += move_time;

    // Access the DRAM page on the pmem node and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_pmem_move_access_time += access_random_page(dram_page, PAGE_NUM, READ);
    }
    munmap(dram_page, PAGE_SIZE*PAGE_NUM);

    // Allocate a page on a remote NUMA node and measure the allocation time
    start_time = get_time_ns();
    void* remote_page = allocate_and_bind_to_numa(PAGE_SIZE, PAGE_NUM, 1); // Allocate on NUMA node 1
    remote_alloc_time = get_time_ns() - start_time;

    // Access the remote NUMA page and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_remote_alloc_access_time += access_random_page(remote_page, PAGE_NUM, READ);
    }

    // Move the remote NUMA page to PMEM and measure the migration time
    start_time = get_time_ns();
    move_pages_to_node(remote_page, PAGE_SIZE, PAGE_NUM, 2); // Move to PMEM node 2
    uint64_t remote_to_pmem_move_time = get_time_ns() - start_time;

    // Access the page on PMEM after the move and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_remote_to_pmem_access_time += access_random_page(remote_page, PAGE_NUM, READ);
    }

    // Cleanup remote NUMA page
    munmap(remote_page, PAGE_SIZE*PAGE_NUM);

    //Allocate a PMEM page and measure the allocation time
    start_time = get_time_ns();
    void* pmem_page = allocate_and_bind_to_numa(PAGE_SIZE, PAGE_NUM, 2);
    uint64_t pmem_alloc_time = get_time_ns() - start_time;
    total_pmem_alloc_time += pmem_alloc_time;

    //Access the PMEM page and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_pmem_access_time += access_random_page(pmem_page, PAGE_NUM, READ);
    }

    // Move the PMEM page to a remote NUMA node and measure the migration time
    start_time = get_time_ns();
    move_pages_to_node(pmem_page, PAGE_SIZE, PAGE_NUM, 1); // Move to NUMA node 1
    uint64_t pmem_to_remote_move_time = get_time_ns() - start_time;

    // Access the page on remote NUMA after the move and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_pmem_to_remote_access_time += access_random_page(pmem_page, PAGE_NUM, READ);
    }
    // Cleanup PMEM page
    munmap(pmem_page, PAGE_SIZE*PAGE_NUM);

    // close(fd);

    // Calculate and print the averages
    double avg_alloc_time = (double)total_alloc_time;
    double avg_local_access_time = (double)total_local_access_time / ITERATIONS;
    double avg_numa_move_time = (double)numa_move_time / PAGE_NUM;
    double avg_remote_access_time = (double)total_remote_access_time / ITERATIONS;
    double avg_pmem_move_time = (double)pmem_move_time / PAGE_NUM;
    double avg_pmem_move_access_time = (double)total_pmem_move_access_time / ITERATIONS;
    double avg_remote_alloc_time = (double)remote_alloc_time;
    double avg_remote_alloc_access_time = (double)total_remote_alloc_access_time / ITERATIONS;
    double avg_remote_to_pmem_move_time = (double)remote_to_pmem_move_time / PAGE_NUM;
    double avg_remote_to_pmem_access_time = (double)total_remote_to_pmem_access_time / ITERATIONS;
    double avg_pmem_alloc_time = (double)total_pmem_alloc_time;
    double avg_pmem_access_time = (double)total_pmem_access_time / ITERATIONS;
    double avg_pmem_to_remote_move_time = (double)pmem_to_remote_move_time / PAGE_NUM;
    double avg_pmem_to_remote_access_time = (double)total_pmem_to_remote_access_time / ITERATIONS;

    printf("Average time to allocate a DRAM page: %.2f ns\n", avg_alloc_time);
    printf("Average time to access a DRAM page locally: %.2f ns\n", avg_local_access_time);
    printf("Average time to move a DRAM page to another NUMA node: %.2f ns\n", avg_numa_move_time);
    printf("Average time to access a DRAM page on a remote NUMA node: %.2f ns\n", avg_remote_access_time);
    printf("Average time to move a DRAM page to Pmem node: %.2f ns\n", avg_pmem_move_time);
    printf("Average time to access a DRAM page on a remote Pmem node: %.2f ns\n", avg_pmem_move_access_time);
    printf("Average time to allocate a remote NUMA page: %.2f ns\n", avg_remote_alloc_time);
    printf("Average time to access a remote NUMA page: %.2f ns\n", avg_remote_alloc_access_time);
    printf("Average time to move a remote NUMA page to PMEM: %.2f ns\n", avg_remote_to_pmem_move_time);
    printf("Average time to access a page on PMEM after moving from remote NUMA: %.2f ns\n", avg_remote_to_pmem_access_time);
    printf("Average time to allocate a PMEM page: %.2f ns\n", avg_pmem_alloc_time);
    printf("Average time to access a PMEM page: %.2f ns\n", avg_pmem_access_time);
    printf("Average time to move a PMEM page to remote NUMA: %.2f ns\n", avg_pmem_to_remote_move_time);
    printf("Average time to access a page on remote NUMA after moving from PMEM: %.2f ns\n", avg_pmem_to_remote_access_time);

    return 0;
}
