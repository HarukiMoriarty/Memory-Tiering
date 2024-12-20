import pandas as pd
import matplotlib.pyplot as plt

# Load data from a CSV file
file_path = "experiment_results.csv"  # Replace with your file path
data = pd.read_csv(file_path)

# Group by configuration columns and average numeric values
grouped = data.groupby(["Tier", "Pattern", "HotAccessCount", "ColdAccessInterval"]).mean(numeric_only=True)

# Reset index for easier plotting
grouped.reset_index(inplace=True)

# Get unique tiers and access patterns
tiers = grouped["Tier"].unique()
access_patterns = grouped["Pattern"].unique()

# Function to create 2x2 subplots for each tier
def create_2x2_subplots(data, tiers, patterns):
    for tier in tiers:
        # Filter data for the current tier
        tier_data = data[data["Tier"] == tier]
        
        # Create a figure for the current tier with a 2x2 grid of subplots
        fig, axes = plt.subplots(2, 2, figsize=(12, 10), sharey=False)
        for i, pattern in enumerate(patterns):
            # Determine the row and column indices for the subplot
            row, col = divmod(i, 2)

            # Filter data for the current access pattern
            pattern_data = tier_data[tier_data["Pattern"] == pattern]
            
            # Add configuration labels for clarity
            pattern_data["Configuration"] = (
                pattern_data["HotAccessCount"].astype(str) 
                + ", " 
                + pattern_data["ColdAccessInterval"].astype(str)
            )
            
            # Extract configurations
            configurations = pattern_data["Configuration"].unique()
            x = range(len(configurations))
            width = 0.3

            if tier == 2:
                # Extract DRAM and PMEM access for Tier 2
                dram_access = []
                pmem_access = []
                for config in configurations:
                    subset = pattern_data[pattern_data["Configuration"] == config]
                    dram_access.append(subset["DRAMAccess"].values[0] if not subset.empty else 0)
                    pmem_access.append(subset["PMEMAccess"].values[0] if not subset.empty else 0)

                # Plot DRAM and PMEM access for Tier 2
                axes[row, col].bar(x, dram_access, width=width, label="DRAM Access", alpha=0.7)
                axes[row, col].bar([xi + width for xi in x], pmem_access, width=width, label="PMEM Access", alpha=0.7, color="orange")

            elif tier == 3:
                # Extract NUMA local, NUMA remote, and PMEM access for Tier 3
                numa_local_access = []
                numa_remote_access = []
                pmem_access = []
                for config in configurations:
                    subset = pattern_data[pattern_data["Configuration"] == config]
                    numa_local_access.append(subset["NUMALocalAccess"].values[0] if not subset.empty else 0)
                    numa_remote_access.append(subset["NUMARemoteAccess"].values[0] if not subset.empty else 0)
                    pmem_access.append(subset["PMEMAccess"].values[0] if not subset.empty else 0)

                # Plot NUMA local, NUMA remote, and PMEM access for Tier 3
                axes[row, col].bar(x, numa_local_access, width=width, label="NUMA Local Access", alpha=0.7, color="green")
                axes[row, col].bar([xi + width for xi in x], numa_remote_access, width=width, label="NUMA Remote Access", alpha=0.7, color="red")
                axes[row, col].bar([xi + 2 * width for xi in x], pmem_access, width=width, label="PMEM Access", alpha=0.7, color="purple")

            # Set titles and labels for each subplot
            axes[row, col].set_title(f"Access Pattern: {pattern}")
            axes[row, col].set_xticks([xi + width / 2 for xi in x])
            axes[row, col].set_xticklabels(configurations, rotation=45, ha="right", fontsize=8)
            axes[row, col].set_ylabel("Page Access")
            axes[row, col].legend(fontsize=8)

        # Hide unused subplots if patterns < 4
        if len(patterns) < 4:
            for j in range(len(patterns), 4):
                row, col = divmod(j, 2)
                axes[row, col].axis("off")

        plt.tight_layout(rect=[0, 0, 1, 0.95])
        plt.show()

# Call the function to create subplots
create_2x2_subplots(grouped, tiers, access_patterns)