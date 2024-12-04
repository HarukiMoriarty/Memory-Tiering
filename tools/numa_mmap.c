#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <numaif.h>
#include <unistd.h>
#include <time.h>

#define ITERATIONS 1000000
#define ARRAY_SIZE 1024 * 1024 // 1MB

// Function to get the current timestamp in nanoseconds
uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Measure memory access latency for a given memory region
void measure_latency(const char *name, void *array, size_t size) {
    uint64_t start, end;
    uint64_t total_latency = 0;
    volatile int dummy;

    for (size_t i = 0; i < ITERATIONS; ++i) {
        size_t index = rand() % size; // Random index access
        start = get_time_ns();
        dummy = ((int *)array)[index]; // Read access
        end = get_time_ns();
        total_latency += (end - start);
    }

    double avg_latency = (double)total_latency / ITERATIONS;
    printf("Average latency for %s: %.2f ns\n", name, avg_latency);
}

void bind_memory_to_numa_node(void *addr, size_t size, int numa_node) {
    unsigned long nodemask = (1UL << numa_node); // Nodemask for the target NUMA node
    if (mbind(addr, size, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) != 0) {
        perror("mbind");
        munmap(addr, size);
        exit(EXIT_FAILURE);
    }
}

void benchmark_memory_access(void *addr, size_t size, const char *description) {
    volatile int dummy;
    struct timespec start, end;

    size_t num_elements = size / sizeof(int); // Total number of integers in the allocated block

    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        size_t random_index = rand() % num_elements; // Generate a random index
        dummy = ((int *)addr)[random_index];        // Access the random index
    }

    // End timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double avg_latency = (total_time / ITERATIONS) * 1e6; // Average latency in microseconds

    printf("Average latency for %s: %.6f µs\n", description, avg_latency);
}

int main() {
    size_t size = ARRAY_SIZE; // 1 MB
    void *addr_node0, *addr_node1;

    // Allocate memory for NUMA node 0
    addr_node0 = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr_node0 == MAP_FAILED) {
        perror("mmap (node 0)");
        exit(EXIT_FAILURE);
    }
    bind_memory_to_numa_node(addr_node0, size, 0);
    printf("Memory allocated on NUMA node 0\n");

    // Allocate memory for NUMA node 1
    addr_node1 = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr_node1 == MAP_FAILED) {
        perror("mmap (node 1)");
        munmap(addr_node0, size);
        exit(EXIT_FAILURE);
    }
    bind_memory_to_numa_node(addr_node1, size, 1);
    printf("Memory allocated on NUMA node 1\n");

    // Write to memory to ensure allocation
    memset(addr_node0, 0, size);
    memset(addr_node1, 0, size);

    // Benchmark memory access
    // measure_latency("NUMA node 0", addr_node0, size);
    // measure_latency("NUMA node 1", addr_node1, size);
    benchmark_memory_access(addr_node0, size, "NUMA node 0");
    benchmark_memory_access(addr_node1, size, "NUMA node 1");

    // Clean up
    munmap(addr_node0, size);
    munmap(addr_node1, size);

    return 0;
}
