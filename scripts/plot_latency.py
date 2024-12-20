import pandas as pd
import matplotlib.pyplot as plt
import numpy as np


def create_latency_plots(data):
    pattern_mapping = {
        'Skewed Pattern': 'skewed,skewed,skewed',
        'Uniform Pattern': 'uniform,uniform,uniform',
        'Hybrid Pattern': 'uniform,uniform,skewed'
    }

    hot_counts = [1, 4, 8, 32, 64, 128]
    colors = {
        'P50': '#1f77b4',
        'P99': '#ff7f0e'
    }

    for pattern_name, pattern_value in pattern_mapping.items():
        pattern_data = data[data['Pattern'] == pattern_value]

        # Calculate global min/max values for both metrics
        p50_min = pattern_data['P50Latency'].min()
        p50_max = pattern_data['P50Latency'].max()
        p99_min = pattern_data['P99Latency'].min()
        p99_max = pattern_data['P99Latency'].max()

        # Add some padding to the ranges
        p50_min = p50_min * 0.9
        p50_max = p50_max * 1.1
        p99_min = p99_min * 0.9
        p99_max = p99_max * 1.1

        # Create a 2x3 grid that can accommodate all 6 plots
        fig, axes = plt.subplots(2, 3, figsize=(20, 12))
        fig.suptitle(pattern_name, fontsize=16, y=0.95)
        axes = axes.flatten()  # Convert 2x3 array to 1D array of 6 axes

        for idx, hot_count in enumerate(hot_counts):
            ax = axes[idx]  # Now we can use all 6 positions
            ax2 = ax.twinx()

            hot_count_data = pattern_data[pattern_data['HotAccessCount'] == hot_count]
            grouped = hot_count_data.groupby(
                ['Tier', 'ColdAccessInterval']).mean()
            grouped = grouped.reset_index()

            intervals = sorted(grouped['ColdAccessInterval'].unique())

            for tier in [2, 3]:
                tier_data = grouped[grouped['Tier'] == tier]

                # P50 Latency
                p50_values = []
                for interval in intervals:
                    value = tier_data[tier_data['ColdAccessInterval'] == interval]['P50Latency'].iloc[
                        0] if not tier_data[tier_data['ColdAccessInterval'] == interval].empty else 0
                    p50_values.append(value)

                line_style = '-' if tier == 2 else '--'
                ax.plot(intervals, p50_values,
                        label=f'Tier {tier} - P50',
                        color=colors['P50'],
                        linestyle=line_style,
                        marker='o',
                        markersize=4)

                # P99 Latency
                p99_values = []
                for interval in intervals:
                    value = tier_data[tier_data['ColdAccessInterval'] == interval]['P99Latency'].iloc[
                        0] if not tier_data[tier_data['ColdAccessInterval'] == interval].empty else 0
                    p99_values.append(value)

                ax2.plot(intervals, p99_values,
                         label=f'Tier {tier} - P99',
                         color=colors['P99'],
                         linestyle=line_style,
                         marker='s',
                         markersize=4)

            # Set consistent y-axis limits for both axes
            ax.set_ylim(p50_min, p50_max)
            ax2.set_ylim(p99_min, p99_max)

            # Set log scale for both axes
            ax.set_yscale('log')
            ax2.set_yscale('log')

            # Customize subplot
            ax.set_title(f'Hot Access Count: {hot_count}')
            ax.set_xlabel('Cold Access Interval')
            ax.set_ylabel('P50 Latency (ns)', color=colors['P50'])
            ax2.set_ylabel('P99 Latency (ns)', color=colors['P99'])

            ax.tick_params(axis='y', labelcolor=colors['P50'])
            ax2.tick_params(axis='y', labelcolor=colors['P99'])

            ax.grid(True, linestyle='--', alpha=0.3)

            if idx == 0:
                lines1, labels1 = ax.get_legend_handles_labels()
                lines2, labels2 = ax2.get_legend_handles_labels()
                ax2.legend(lines1 + lines2, labels1 + labels2,
                           bbox_to_anchor=(1.05, 1), loc='upper left')

        plt.tight_layout()
        plt.subplots_adjust(top=0.9)
        plt.savefig("latency_" + pattern_name + ".pdf")


# Example usage:
data = pd.read_csv('results.csv')
create_latency_plots(data)
