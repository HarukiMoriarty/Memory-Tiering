#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define DAX_DEVICE "/dev/dax1.0"
#define MAP_SIZE 2097152  // Adjusted to 2MB alignment

int main() {
    // Open the DAX device
    int fd = open(DAX_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }

    // Map the DAX device
    void *mapped_addr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_addr == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }

    // Access the memory
    uint32_t *data = (uint32_t *)mapped_addr;
    printf("Read value: %u\n", *data); // Example read

    // Clean up
    if (munmap(mapped_addr, MAP_SIZE) < 0) {
        perror("munmap failed");
    }
    close(fd);

    return 0;
}