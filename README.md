# Memory Tiering Policy

## Server Memory Tiers Size Config

numa local: 5 GiB       5 * 1024 / 4  = 1280 Pages      1280 * 20 Byte = 25 MB Entry Table

numa remote: 20 GiB     20 * 1024 / 4  = 5120 Pages     5120 * 20 Byte = 100 MB Entry Table

pmem: 80 GiB            20 * 1024 / 4 = 20480 Pages     20480 * 20 Bytes = 400 MB Entry Table

All entry tables locate at the numa local memory

## Client Page Partition

Server page number partition (1:4:16): following memory distribution 

- numa local: 1280
- numa remote: 5120
- pmem: 21480

Suppose page allocation of clients is: 1000, 2000, 1500, with total number is 4500. 

Then the client partition of numa local pages is

- client 1: 1000 * 1280 / 4500 = 284
- client 2: 2000 * 1280 / 4500 = 568
- client 3: 1500 * 1280 / 4500 = 426

The client partition of numa remote pages is

- client 1: 1000 * 5120 / 4500 = 1137 > (1000 - 284) = 716 
- client 2: 2000 * 5120 / 4500 = 2274 > (2000 - 568) = 1432 
- client 3: 1500 * 5120 / 4500 = 1709 > (1500 - 426) = 1074 

## Evaluation

To start the evaluation, run the following command:

```bash
$ make
$ LOG_LEVEL=info ./build/main -p uniform,skewed,uniform -c 1000,2000,1500 -b 100 -m 1000 -t 3 -s 1280,5120,21480 --hot-access-cnt 10 --cold-access-interval 1000
```

Where:
- `-p`, `--patterns`: Memory access patterns for each client (uniform/skewed)
- `-c`, `--client-addr-space-sizes`: Address space size for each client
- `-b`, `--buffer-size`: Size of ring buffer
- `-m`, `--messages`: Number of messages per client
- `-t`, `--num-tiers`: Number of memory tiers
- `-s`, `--mem-sizes`: Memory configuration for each tiering
- `-h`, `--help`: Print usage information
- `--hot-access-cnt`: Hot access cnt for promote a page
- `--cold-access-interval`: Cold access interval for demote a page
