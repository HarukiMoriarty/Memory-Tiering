#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

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
        volatile uint64_t value = array[index];         // Read access
        end = get_time_ns();
        total_latency += (end - start);        // Accumulate latency
    }

    double avg_latency = (double)total_latency / ITERATIONS;
    printf("Average latency for %s: %.2f ns\n", name, avg_latency);
}

int main() {
    // Allocate memory in DRAM
    uint64_t *dram_array = (uint64_t *)malloc(ARRAY_SIZE * sizeof(uint64_t));
    if (!dram_array) {
        perror("malloc");
        return 1;
    }

    // Fill DRAM array
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        dram_array[i] = i;
    }

    // Map a file to PMEM using mmap
    const char *pmem_file = "/mnt/pmem-aos/latency_test";
    int fd = open(pmem_file, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        free(dram_array);
        return 1;
    }

    if (ftruncate(fd, ARRAY_SIZE * sizeof(uint64_t)) == -1) {
        perror("ftruncate");
        close(fd);
        free(dram_array);
        return 1;
    }

    uint64_t *pmem_array = (uint64_t *)mmap(NULL, ARRAY_SIZE * sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (pmem_array == MAP_FAILED) {
        perror("mmap");
        close(fd);
        free(dram_array);
        return 1;
    }

    // Fill PMEM array
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        pmem_array[i] = i;
    }

    // Measure latency
    printf("Measuring memory access latency...\n");
    measure_latency("DRAM", dram_array, ARRAY_SIZE);
    measure_latency("PMEM", pmem_array, ARRAY_SIZE);

    // Cleanup
    munmap(pmem_array, ARRAY_SIZE * sizeof(uint64_t));
    close(fd);
    free(dram_array);

    return 0;
}
