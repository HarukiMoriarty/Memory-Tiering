import subprocess
import re
import statistics

# Define the command to run the benchmark
command = ["sudo", "numactl", "--membind=0", "--cpubind=0", "test_tools/benchmark"]

# Number of iterations for the batch
num_iterations = 50

# Define regex patterns to extract the relevant metrics
patterns = {
    "allocate_dram_page": re.compile(r"Average time to allocate a DRAM page:\s+([\d.]+) ns"),
    "access_dram_local": re.compile(r"Average time to access a DRAM page locally:\s+([\d.]+) ns"),
    "move_dram_page": re.compile(r"Average time to move a DRAM page to another NUMA node:\s+([\d.]+) ns"),
    "access_dram_remote": re.compile(r"Average time to access a DRAM page on a remote NUMA node:\s+([\d.]+) ns"),
    "allocate_pmem_page": re.compile(r"Average time to allocate a PMEM page:\s+([\d.]+) ns"),
    "access_pmem_page": re.compile(r"Average time to access a PMEM page:\s+([\d.]+) ns"),
    "allocate_pmem_devdax_page": re.compile(r"Average time to allocate a PMEM range via devdax:\s+([\d.]+) ns"),
    "access_pmem_devdax_page": re.compile(r"Average time to access a PMEM range via devdax:\s+([\d.]+) ns"),
}

# Store the results for each metric
results = {key: [] for key in patterns.keys()}

# Run the benchmark in batches and collect the data
for i in range(num_iterations):
    try:
        # print(f"Running iteration {i + 1}...")
        # Run the benchmark
        output = subprocess.check_output(command, text=True)
        
        # Parse the output and collect the results
        for key, pattern in patterns.items():
            match = pattern.search(output)
            if match:
                results[key].append(float(match.group(1)))
    except subprocess.CalledProcessError as e:
        print(f"Error during iteration {i + 1}: {e}")
    except Exception as ex:
        print(f"Unexpected error: {ex}")

# Compute and display the average results
print("\n--- Average Results ---")
for key, values in results.items():
    if values:
        avg = statistics.mean(values)
        print(f"{key.replace('_', ' ').title()}: {avg:.2f} ns")
    else:
        print(f"{key.replace('_', ' ').title()}: No data collected")
