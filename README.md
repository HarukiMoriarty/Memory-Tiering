# Memory Tiering Policy

## Server Memory Size Config

numa local: 5 GiB       5 * 1024 / 4 = 1280 * 20 = 25 MB

numa remote: 20 GiB     1280 * 4 = 5120 = 100 MB

pmem: 80 GiB            5120 * 4 = 400 MB

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
