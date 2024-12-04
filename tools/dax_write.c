#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <emmintrin.h>  // For clflush
#include <unistd.h>

#define DAX_DEVICE "/dev/dax1.0"  // Adjust to your device
#define MAP_SIZE 2097152          // Must be aligned to the device's alignment

void flush_cache_line(void *addr) {
    // Flush cache line containing 'addr' to persistent memory
    _mm_clflush(addr);
}

int main() {
    // Open the DAX device
    int fd = open(DAX_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }

    // Map the device into memory
    void *mapped_addr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_addr == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }

    // Write data to the memory-mapped region
    uint32_t *data = (uint32_t *)mapped_addr;
    *data = 12345;  // Example write
    printf("Written value: %u\n", *data);

    // Flush the cache line to ensure persistence
    flush_cache_line(data);

    // Clean up
    if (munmap(mapped_addr, MAP_SIZE) < 0) {
        perror("munmap failed");
    }
    close(fd);

    return 0;
}