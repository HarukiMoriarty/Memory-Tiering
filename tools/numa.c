#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <numa.h> // NUMA API

#define ARRAY_SIZE 1024 * 1024 // 1MB
#define ITERATIONS 1000000

// Function to get the current timestamp in nanoseconds
uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Measure memory access latency for a given memory region
void measure_latency(const char *name, volatile uint64_t *array, size_t size) {
    uint64_t start, end;
    uint64_t total_latency = 0;

    for (size_t i = 0; i < ITERATIONS; ++i) {
        size_t index = rand() % size; // Random index access
        start = get_time_ns();
        volatile uint64_t value = array[index]; // Read access
        end = get_time_ns();
        total_latency += (end - start);
    }

    double avg_latency = (double)total_latency / ITERATIONS;
    printf("Average latency for %s: %.2f ns\n", name, avg_latency);
}

// Allocate memory on a specific NUMA node
uint64_t* allocate_on_numa_node(int node) {
    if (numa_available() == -1) {
        fprintf(stderr, "NUMA is not supported on this system.\n");
        exit(1);
    }

    // Allocate memory bound to the specified NUMA node
    void *mem = numa_alloc_onnode(ARRAY_SIZE * sizeof(uint64_t), node);
    if (!mem) {
        fprintf(stderr, "Failed to allocate memory on NUMA node %d.\n", node);
        exit(1);
    }

    // Initialize memory
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        ((uint64_t *)mem)[i] = i;
    }

    return (uint64_t *)mem;
}

int main() {
    printf("NUMA Latency Benchmark\n");

    // Allocate memory on NUMA nodes
    printf("Allocating memory on NUMA node 0...\n");
    uint64_t *node0_array = allocate_on_numa_node(0);

    printf("Allocating memory on NUMA node 1...\n");
    uint64_t *node1_array = allocate_on_numa_node(1);

    // Measure latency from NUMA node 0
    printf("\nMeasuring access latency from NUMA node 0:\n");
    numa_run_on_node(0); // Run this thread on NUMA node 0
    measure_latency("Local DRAM (NUMA Node 0)", node0_array, ARRAY_SIZE);
    measure_latency("Remote DRAM (NUMA Node 1)", node1_array, ARRAY_SIZE);

    // Measure latency from NUMA node 1
    printf("\nMeasuring access latency from NUMA node 1:\n");
    numa_run_on_node(1); // Run this thread on NUMA node 1
    measure_latency("Remote DRAM (NUMA Node 0)", node0_array, ARRAY_SIZE);
    measure_latency("Local DRAM (NUMA Node 1)", node1_array, ARRAY_SIZE);

    // Cleanup
    numa_free(node0_array, ARRAY_SIZE * sizeof(uint64_t));
    numa_free(node1_array, ARRAY_SIZE * sizeof(uint64_t));

    return 0;
}
