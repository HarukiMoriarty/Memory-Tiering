# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -lrt
NUMA_LIB = -lnuma

# Targets
TARGETS = pmem numa numa_mmap benchmark

# Build rules
all: $(TARGETS)

pmem: pmem.c
	$(CC) $(CFLAGS) -o $@ $^

numa: numa.c
	$(CC) $(CFLAGS) -o $@ $^ $(NUMA_LIB)

numa_mmap: numa_mmap.c
	$(CC) $(CFLAGS) -o $@ $^ $(NUMA_LIB)

benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $^ $(NUMA_LIB)

# Clean rule
clean:
	rm -f $(TARGETS)
