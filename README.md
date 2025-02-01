# Memory Tiering System

A configurable memory tiering system that manages data placement across different memory tiers (Local NUMA, Remote NUMA, and PMEM) based on access patterns.

## Environment Setup

### Prerequisites
- Linux system with NUMA support
- C++17 compatible compiler
- NUMA development libraries

```bash
# Install required packages on Ubuntu/Debian
sudo apt-get update
sudo apt-get install libnuma-dev cmake build-essential
```

### Building
```bash
# Create build directory
mkdir build && cd build

# Build
make -j$(nproc)
```

### Running
```bash
# Run the program
LOG_LEVEL=info ./memory_tiering [options]
```

## Configuration Parameters

| Parameter | Flag | Description | Example | Default |
|-----------|------|-------------|---------|---------|
| Buffer Size | `-b, --buffer-size` | Size of the ring buffer for message passing | `-b 100` | 10 |
| Messages | `-m, --messages` | Number of messages per client | `-m 100000` | 100 |
| Access Patterns | `-p, --patterns` | Memory access pattern per client (uniform/skewed) | `-p uniform,uniform` | Required |
| Client Tier Sizes | `-c, --client-tier-sizes` | Memory pages per tier for each client | `-c "100 50 25,200 100 50"` | Required |
| Number of Tiers | `-t, --num-tiers` | Number of memory tiers (2 or 3) | `-t 3` | 3 |
| Memory Sizes | `-s, --mem-sizes` | Total memory pages per tier | `-s 1000,500,200` | Required |
| Hot Access Count | `--hot-access-cnt` | Threshold for hot page detection | `--hot-access-cnt 10` | 10 |
| Cold Access Interval | `--cold-access-interval` | Interval (ms) for cold page detection | `--cold-access-interval 1000` | 1000 |

## System Architecture

### Memory Tier Types

| Tier Type | Description | Relative Speed | Usage |
|-----------|-------------|----------------|-------|
| Local NUMA | Local memory with direct CPU access | Fastest | Hot data, frequently accessed |
| Remote NUMA | Memory from other NUMA nodes | Medium | Warm data, moderately accessed |
| PMEM | Persistent memory | Slowest | Cold data, rarely accessed |

### Access Pattern Types

| Pattern | Description | Use Case |
|---------|-------------|----------|
| Uniform | Equal probability of accessing any page | General purpose workloads |
| Skewed | 70-20-10 distribution | Cache-like workloads with hot/warm/cold data |

## Example Usage

### Basic 2-Tier Setup
Single client with uniform access:
```bash
./memory_tiering \
  -t 2 \                    # 2-tier system
  -p uniform \              # One client with uniform access
  -c "100,50" \             # Client needs 100 pages in local, 50 in PMEM
  -s 500,200 \              # Total system has 500 local pages, 200 PMEM pages
  -b 100 \                  # Ring buffer size of 100
  -m 10000                  # 10000 messages
```

### Advanced 3-Tier Setup
Multiple clients with different patterns:
```bash
./memory_tiering \
  -t 3 \                            # 3-tier system
  -p uniform,skewed \               # Two clients: uniform and skewed access
  -c "100 50 25,200 100 50" \       # Memory sizes for each client
  -s 1000,500,200 \                 # Total memory sizes per tier
  -b 100 \                          # Ring buffer size
  -m 100000 \                       # Messages per client
  --hot-access-cnt 10 \             # Hot page threshold
  --cold-access-interval 1000       # Cold page check interval
```