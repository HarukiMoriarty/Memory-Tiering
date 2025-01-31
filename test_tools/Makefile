# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -lrt
NUMA_LIB = -lnuma

ifdef DEBUG
CFLAGS += -DDEBUG
endif

# Targets
TARGETS = benchmark

# Build rules
all: $(TARGETS)

benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $^ $(NUMA_LIB)

# Clean rule
clean:
	rm -f $(TARGETS)
