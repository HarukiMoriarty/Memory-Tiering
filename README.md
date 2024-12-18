# Memory Tiering Policy

## Server Memory Size Config

numa local: 5 GiB       5 * 1024 / 4 = 1280 * 20 = 25 MB

numa remote: 20 GiB     1280 * 4 = 5120 = 100 MB

pmem: 80 GiB            5120 * 4 = 400 MB

## Client Page Partition

Server page number partition (1:4:16): 

- numa local: 1280
- numa remote: 5120
- pmem: 21480

Suppose page allocation of clients is: 1000, 2000, 1500, with total number is 4500. 

Then the client partition of numa local pages is

- client 1: 1000 * 1280 / 4500 = 284
- client 2: 2000 * 1280 / 4500 = 568
- client 3: 1500 * 1280 / 4500 = 426

The client partition of numa remote pages is

- client 1: 1000 * 5120 / 4500 = 1137 > (1000 - 284) = 716 -> finish allocation
- client 2: 2000 * 5120 / 4500 = 2274 > (2000 - 568) = 1432 -> finish allocation
- client 3: 1500 * 5120 / 4500 = 1709 > (1500 - 426) = 1074 -> finish allocation

## Evaluation

To start the evaluation, run the following command:

```bash
make clean
make

./build/main -p uniform,skewed,uniform -s 1000,2000,1500 -b 100 -m 1000 -c 1280,5120,21480
```

Where:
- `-p`, `--patterns`: Memory access patterns for each client (uniform/skewed)
- `-s`, `--sizes`: Memory space size for each client
- `-b`, `--buffer-size`: Size of ring buffer
- `-m`, `--messages`: Number of messages per client
- `-c`, `--server`: Memory configuration for server
- `-h`, `--help`: Print usage information
