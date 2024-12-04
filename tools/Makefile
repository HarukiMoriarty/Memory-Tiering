# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -lrt
NUMA_LIB = -lnuma

# Targets
TARGETS = pmem numa

# Build rules
all: $(TARGETS)

pmem: pmem.c
	$(CC) $(CFLAGS) -o $@ $^

numa: numa.c
	$(CC) $(CFLAGS) -o $@ $^ $(NUMA_LIB)

# Clean rule
clean:
	rm -f $(TARGETS)
