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
#define PAGE_NUM 100
#define ITERATIONS 10000 
#define PMEM_FILE "/mnt/pmem-aos/latency_test"
#define DAX_DEVICE "/dev/dax1.0"  
#define MAP_SIZE 2097152       

int page_access_track[PAGE_NUM] = {0};

typedef enum  {
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

// Allocate memory on PMEM using mmap (total PAGE_NUM pages)
void* allocate_pmem_pages(int fd, size_t size, size_t number) {
    void* mem = mmap(NULL, size * number, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, fd, 0);
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
    flush_cache(addr);
    volatile uint64_t* page = (volatile uint64_t*)addr;
    uint64_t start_time = get_time_ns();

    int random_page = rand() % page_num;
    int offset = random_page * PAGE_SIZE / sizeof(uint64_t);
    volatile uint64_t value;
    // Perform a simple read/write operation
    switch (mode)
    {
    case READ:
        value = page[offset]; // Read from the page
        break;
    case WRITE:
        page[offset] = 44;     // Write to a random page
        break;
    case READ_WRITE:
        value = page[offset]; // Read from the page
        page[offset] = 44;     // Write to a random page
        break;
    default:
        break;
    }

#ifdef DEBUG
    page_access_track[random_page]++;
#endif

    return get_time_ns() - start_time;
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

    // Set the random seed
    srand(time(NULL));

    // Create and prepare the PMEM file
    int fd = open(PMEM_FILE, O_RDWR | O_CREAT | O_DIRECT, 0666);
    if (fd < 0) {
        perror("open PMEM file failed");
        return 1;
    }

    // Resize the PMEM file to fit PAGE_SIZE
    if (ftruncate(fd, PAGE_SIZE * PAGE_NUM) != 0) {
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

    uint64_t total_alloc_time = 0;
    uint64_t total_local_access_time = 0;
    uint64_t total_move_time = 0;
    uint64_t total_remote_access_time = 0;

    uint64_t total_pmem_alloc_time = 0;
    uint64_t total_pmem_access_time = 0;
    uint64_t total_pmem_devdax_alloc_time = 0;
    uint64_t total_pmem_devdax_access_time = 0;

    // Step 1: Allocate a DRAM page and measure the allocation time
    uint64_t start_time = get_time_ns();
    void* dram_page = allocate_pages(PAGE_SIZE, PAGE_NUM);
    uint64_t alloc_time = get_time_ns() - start_time;
    total_alloc_time += alloc_time;

    // Step 2: Access the DRAM page locally and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_local_access_time += access_random_page(dram_page, PAGE_NUM, READ_WRITE);
    }

#ifdef DEBUG
    print_page_access_track();
#endif

    // Step 3: Move the DRAM page to another NUMA node and measure the migration time
    start_time = get_time_ns();
    move_pages_to_node(dram_page, PAGE_SIZE, PAGE_NUM, 1); // Move to NUMA node 1
    uint64_t move_time = get_time_ns() - start_time;
    total_move_time += move_time;

    // Step 4: Access the DRAM page on the remote NUMA node and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_remote_access_time += access_random_page(dram_page, PAGE_NUM, READ_WRITE);
    }

    // Cleanup DRAM page
    munmap(dram_page, PAGE_SIZE);

    // Step 5: Allocate a PMEM page and measure the allocation time
    start_time = get_time_ns();
    void* pmem_page = allocate_pmem_pages(fd, PAGE_SIZE, PAGE_NUM);
    uint64_t pmem_alloc_time = get_time_ns() - start_time;
    total_pmem_alloc_time += pmem_alloc_time;

    // Step 6: Access the PMEM page and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_pmem_access_time += access_random_page(pmem_page, PAGE_NUM, READ_WRITE);
    }

    // Cleanup PMEM page
    munmap(pmem_page, PAGE_SIZE);

    // Step 7: Allocate a PMEM range via devdax and measure the allocation time
    start_time = get_time_ns();
    void* pmem_devdax_range = allocate_pmem_pages(dax_fd, PAGE_SIZE, PAGE_NUM);
    uint64_t pmem_devdax_alloc_time = get_time_ns() - start_time;
    total_pmem_devdax_alloc_time += pmem_devdax_alloc_time;

    // Step 8: Access the PMEM range via devdax and measure the access time
    for (int i = 0; i < ITERATIONS; ++i) {
        total_pmem_devdax_access_time += access_random_page(pmem_devdax_range, PAGE_NUM, READ_WRITE);
    }

    close(fd);

    // Calculate and print the averages
    double avg_alloc_time = (double)total_alloc_time;
    double avg_local_access_time = (double)total_local_access_time / ITERATIONS;
    double avg_move_time = (double)total_move_time;
    double avg_remote_access_time = (double)total_remote_access_time / ITERATIONS;
    double avg_pmem_alloc_time = (double)total_pmem_alloc_time;
    double avg_pmem_access_time = (double)total_pmem_access_time / ITERATIONS;
    double avg_pmem_devdax_alloc_time = (double)total_pmem_devdax_alloc_time;
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
