#!/bin/bash

# Set the number of repetitions for each configuration
ITERATIONS=5
# Initialize a counter for the unique ID
ID=1

# Set the default values for the other parameters
LOG_LEVEL="info"
EXECUTABLE="./build/main"
BUFFER_SIZE=100
MESSAGE_COUNT=10000
MEMORY_SIZES_TIER_2="6400,21480"
MEMORY_SIZES_TIER_3="1280,5120,21480"

# Define the variations for the parameters
PATTERNS=("uniform,uniform,uniform" "uniform,uniform,skewed" "skewed,uniform,skewed" "skewed,skewed,skewed")
HOT_ACCESS_COUNTS=(1 5 10 30 50)
COLD_ACCESS_INTERVALS=(10 100 200 500 1000)
TIERS=(2 3)

# CSV file to store results
CSV_FILE="experiment_results.csv"

# Write the CSV header
echo "ExperimentID,Tier,Pattern,HotAccessCount,ColdAccessInterval,NUMALocalAccess,NUMARemoteAccess,DRAMAccess,PMEMAccess,MinLatency,P50Latency,P99Latency,MaxLatency,MeanLatency,MigrationMinLatency,MigrationP50Latency,MigrationP99Latency,MigrationMaxLatency,MigrationMeanLatency,LocalToRemote,RemoteToLocal,PMEMToRemote,RemoteToPMEM,LocalToPMEM,PMEMToLocal,DRAMToPMEM,PMEMToDRAM,Throughput" > $CSV_FILE

# Function to parse output and extract metrics
parse_output() {
    local log_content="$1"
    declare -A data

    # Initialize relevant section flag
    relevant_section=false

    # Parse the log content
    while IFS= read -r line; do
        # Enable relevant section parsing
        if [[ $line =~ "Memory Access Metrics" ]]; then
            relevant_section=true
            continue
        fi

        # Parse only relevant section
        if [[ $relevant_section == true ]]; then
            if [[ $line =~ "NUMA Local:" ]]; then
                data["NUMA_Local"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "NUMA Remote:" ]]; then
                data["NUMA_Remote"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "DRAM:" && ! $line =~ "->" ]]; then
                data["DRAM"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "PMEM:" && ! $line =~ "->" ]]; then
                data["PMEM"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "Min:" ]]; then
                if [[ ! ${data["Latency_Min"]} ]]; then
                    data["Latency_Min"]=$(echo "$line" | awk '{print $NF}')
                else
                    data["Migration_Min"]=$(echo "$line" | awk '{print $NF}')
                fi
            elif [[ $line =~ "P50:" ]]; then
                if [[ ! ${data["Latency_P50"]} ]]; then
                    data["Latency_P50"]=$(echo "$line" | awk '{print $NF}')
                else
                    data["Migration_P50"]=$(echo "$line" | awk '{print $NF}')
                fi
            elif [[ $line =~ "P99:" ]]; then
                if [[ ! ${data["Latency_P99"]} ]]; then
                    data["Latency_P99"]=$(echo "$line" | awk '{print $NF}')
                else
                    data["Migration_P99"]=$(echo "$line" | awk '{print $NF}')
                fi
            elif [[ $line =~ "Max:" ]]; then
                if [[ ! ${data["Latency_Max"]} ]]; then
                    data["Latency_Max"]=$(echo "$line" | awk '{print $NF}')
                else
                    data["Migration_Max"]=$(echo "$line" | awk '{print $NF}')
                fi
            elif [[ $line =~ "Mean:" ]]; then
                if [[ ! ${data["Latency_Mean"]} ]]; then
                    data["Latency_Mean"]=$(echo "$line" | awk '{print $NF}')
                else
                    data["Migration_Mean"]=$(echo "$line" | awk '{print $NF}')
                fi
            elif [[ $line =~ "Local -> Remote:" ]]; then
                data["Local_Remote"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "Remote -> Local:" ]]; then
                data["Remote_Local"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "PMEM -> Remote:" ]]; then
                data["PMEM_Remote"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "Remote -> PMEM:" ]]; then
                data["Remote_PMEM"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "Local -> PMEM:" ]]; then
                data["Local_PMEM"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "PMEM -> Local:" ]]; then
                data["PMEM_Local"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "DRAM -> PMEM:" ]]; then
                data["DRAM_PMEM"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "PMEM -> DRAM:" ]]; then
                data["PMEM_DRAM"]=$(echo "$line" | awk '{print $NF}')
            elif [[ $line =~ "Throughput:" ]]; then
                data["Throughput"]=$(echo "$line" | awk '{print $(NF-1)}')
            fi
        fi
    done <<< "$log_content"

    # Output data as a single CSV row
    echo "${data["NUMA_Local"]},${data["NUMA_Remote"]},${data["DRAM"]},${data["PMEM"]},${data["Latency_Min"]},${data["Latency_P50"]},${data["Latency_P99"]},${data["Latency_Max"]},${data["Latency_Mean"]},${data["Migration_Min"]},${data["Migration_P50"]},${data["Migration_P99"]},${data["Migration_Max"]},${data["Migration_Mean"]},${data["Local_Remote"]},${data["Remote_Local"]},${data["PMEM_Remote"]},${data["Remote_PMEM"]},${data["Local_PMEM"]},${data["PMEM_Local"]},${data["DRAM_PMEM"]},${data["PMEM_DRAM"]},${data["Throughput"]}"
}

# Loop through all combinations of parameters
for tier in "${TIERS[@]}"; do
    # Adjust memory sizes based on the number of tiers
    if [ "$tier" -eq 2 ]; then
        MEMORY_SIZES=$MEMORY_SIZES_TIER_2
    else
        MEMORY_SIZES=$MEMORY_SIZES_TIER_3
    fi

    for pattern in "${PATTERNS[@]}"; do
        for hot_access_cnt in "${HOT_ACCESS_COUNTS[@]}"; do
            for cold_access_interval in "${COLD_ACCESS_INTERVALS[@]}"; do
                for ((iteration=1; iteration<=ITERATIONS; iteration++)); do
                    # Construct the command
                    CMD="LOG_LEVEL=$LOG_LEVEL $EXECUTABLE -p $pattern -c 1000,2000,1500 -b $BUFFER_SIZE -m $MESSAGE_COUNT -t $tier -s $MEMORY_SIZES --hot-access-cnt $hot_access_cnt --cold-access-interval $cold_access_interval"

                    # Run the command and capture output
                    OUTPUT=$(eval $CMD 2>&1)

                    # Parse the output
                    METRICS=$(parse_output "$OUTPUT")

                    # Append the configuration and metrics to the CSV file
                    echo "$ID,$tier,\"$pattern\",$hot_access_cnt,$cold_access_interval,$METRICS"  >> $CSV_FILE
                    ((ID++))
                    # Check the exit status
                    if [ $? -ne 0 ]; then
                        echo "Command failed: $CMD"
                    else
                        echo "Command succeeded: $CMD"
                    fi
                done
            done
        done
    done
done

echo "All experiments completed. Results are in $CSV_FILE."