import pandas as pd
import matplotlib.pyplot as plt

# Load data from a CSV file
file_path = "experiment_results.csv"  # Replace with your file path
data = pd.read_csv(file_path)

# Group by configuration columns and average numeric values
grouped = data.groupby(["Tier", "Pattern", "HotAccessCount", "ColdAccessInterval"]).mean(numeric_only=True)

# Reset index for easier plotting
grouped.reset_index(inplace=True)

# Get unique access patterns
access_patterns = grouped["Pattern"].unique()

def create_graphs_by_tier_2x2(data, patterns):
    # Get unique tiers
    unique_tiers = data["Tier"].unique()

    for tier in unique_tiers:
        # Filter data for the current tier
        tier_data = data[data["Tier"] == tier]
        
        # Create a 2x2 grid of subplots for the current tier
        fig, axes = plt.subplots(2, 2, figsize=(10, 10), sharey=False)

        for i, pattern in enumerate(patterns):
            # Determine the row and column indices for the subplot
            row, col = divmod(i, 2)

            # Filter data for the current access pattern
            pattern_data = tier_data[tier_data["Pattern"] == pattern]
            
            # Add configuration labels for clarity
            pattern_data["Configuration"] = pattern_data["HotAccessCount"].astype(str) + ", " + pattern_data["ColdAccessInterval"].astype(str)
            
            # Extract configurations
            configurations = pattern_data["Configuration"].unique()
            x = range(len(configurations))
            width = 0.3
            
            # Extract latency and throughput
            latency = []
            throughput = []
            for config in configurations:
                subset = pattern_data[pattern_data["Configuration"] == config]
                latency.append(subset["P50Latency"].values[0] if not subset.empty else 0)
                throughput.append(subset["Throughput"].values[0] if not subset.empty else 0)
            
            # Plotting
            ax = axes[row, col]
            ax.bar(x, latency, width=width, label="Latency (ms)", alpha=0.7)
            ax.set_title(f"Access Pattern: {pattern}")
            ax.set_xticks(x)
            ax.set_xticklabels(configurations, rotation=45, ha="right", fontsize=6)
            ax.set_ylabel("P50 Latency (ms)")
            ax.tick_params(axis="y")
            
            ax2 = ax.twinx()
            ax2.bar([xi + width for xi in x], throughput, width=width, label="Throughput (req/s)", color="orange", alpha=0.7)
            ax2.set_ylabel("Throughput (req/s)", color="orange")
            ax2.tick_params(axis="y", labelcolor="orange")
            
            # Adjust legends
            ax.legend(loc="upper left", fontsize=8)
            ax2.legend(loc="upper right", fontsize=8)

        # Hide unused subplots if patterns < 4
        if len(patterns) < 4:
            for j in range(len(patterns), 4):
                row, col = divmod(j, 2)
                axes[row, col].axis("off")

        plt.tight_layout(rect=[0, 0, 1, 0.95])  # Adjust layout to include the title
        plt.show()

# Call the function to create graphs by tier with 2x2 subplots
create_graphs_by_tier_2x2(grouped, access_patterns)