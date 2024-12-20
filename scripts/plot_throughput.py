import pandas as pd
import matplotlib.pyplot as plt
import numpy as np


def create_throughput_plots(data):
    pattern_mapping = {
        'Skewed Pattern': 'skewed,skewed,skewed',
        'Uniform Pattern': 'uniform,uniform,uniform',
        'Hybrid Pattern': 'uniform,uniform,skewed'
    }

    hot_counts = [1, 4, 8, 32, 64, 128]

    for pattern_name, pattern_value in pattern_mapping.items():
        pattern_data = data[data['Pattern'] == pattern_value]

        # Calculate global min/max values for throughput
        throughput_min = pattern_data['Throughput'].min()
        throughput_max = pattern_data['Throughput'].max()

        # Add padding to the range
        throughput_min = throughput_min * 0.9
        throughput_max = throughput_max * 1.1

        fig, axes = plt.subplots(2, 3, figsize=(20, 12))
        fig.suptitle(pattern_name, fontsize=16, y=0.95)
        axes = axes.flatten()

        for idx, hot_count in enumerate(hot_counts):
            ax = axes[idx]

            hot_count_data = pattern_data[pattern_data['HotAccessCount'] == hot_count]
            grouped = hot_count_data.groupby(
                ['Tier', 'ColdAccessInterval']).mean()
            grouped = grouped.reset_index()

            intervals = sorted(grouped['ColdAccessInterval'].unique())

            for tier in [2, 3]:
                tier_data = grouped[grouped['Tier'] == tier]

                throughput_values = []
                for interval in intervals:
                    value = tier_data[tier_data['ColdAccessInterval'] == interval]['Throughput'].iloc[
                        0] if not tier_data[tier_data['ColdAccessInterval'] == interval].empty else 0
                    throughput_values.append(value)

                line_style = '-' if tier == 2 else '--'
                ax.plot(intervals, throughput_values,
                        label=f'Tier {tier}',
                        linestyle=line_style,
                        marker='o',
                        markersize=4)

            # Set consistent y-axis limits
            ax.set_ylim(throughput_min, throughput_max)

            # Use log scale for throughput
            ax.set_yscale('log')

            # Customize subplot
            ax.set_title(f'Hot Access Count: {hot_count}')
            ax.set_xlabel('Cold Access Interval')
            ax.set_ylabel('Throughput')

            ax.grid(True, linestyle='--', alpha=0.3)

            # Only show legend for first subplot
            if idx == 0:
                ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')

        plt.tight_layout()
        plt.subplots_adjust(top=0.9)
        plt.savefig("throught_" + pattern_name + ".pdf")


# Example usage:
data = pd.read_csv('results.csv')
create_throughput_plots(data)
