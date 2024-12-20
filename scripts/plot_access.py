import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as patches


def create_detailed_comparison_plots(data):
    pattern_mapping = {
        'Skewed Pattern': 'skewed,skewed,skewed',
        'Uniform Pattern': 'uniform,uniform,uniform',
        'Hybrid Pattern': 'uniform,uniform,skewed'
    }

    hot_counts = [1, 4, 8, 32, 64, 128]

    colors = {
        'DRAMAccess': '#1f77b4',
        'PMEMAccess': '#ff7f0e',
        'NUMALocalAccess': '#2ca02c',
        'NUMARemoteAccess': '#d62728'
    }

    for pattern_name, pattern_value in pattern_mapping.items():
        fig, axes = plt.subplots(2, 3, figsize=(20, 12))
        fig.suptitle(pattern_name, fontsize=16, y=0.95)
        axes = axes.flatten()

        pattern_data = data[data['Pattern'] == pattern_value]

        for idx, hot_count in enumerate(hot_counts):
            ax = axes[idx]

            hot_count_data = pattern_data[pattern_data['HotAccessCount'] == hot_count]
            grouped = hot_count_data.groupby(
                ['Tier', 'ColdAccessInterval']).mean()
            grouped = grouped.reset_index()

            intervals = sorted(grouped['ColdAccessInterval'].unique())
            x = range(len(intervals))
            width = 0.35

            # Plot bars for each tier
            for tier_idx, tier in enumerate([2, 3]):
                tier_data = grouped[grouped['Tier'] == tier]
                x_positions = [xi + (tier_idx * width) for xi in x]

                bottom = None
                for access_type in ['DRAMAccess', 'PMEMAccess'] if tier == 2 else ['NUMALocalAccess', 'NUMARemoteAccess', 'PMEMAccess']:
                    values = []
                    for interval in intervals:
                        value = tier_data[tier_data['ColdAccessInterval'] == interval][access_type].iloc[
                            0] if not tier_data[tier_data['ColdAccessInterval'] == interval].empty else 0
                        values.append(value)

                    ax.bar(x_positions, values, width, bottom=bottom,
                           label=f'Tier {tier} - {access_type}',
                           color=colors[access_type],
                           alpha=0.7)
                    bottom = values if bottom is None else [
                        sum(x) for x in zip(bottom, values)]

            # Add vertical separators between tier pairs
            for xi in x:
                ax.axvline(x=xi + width / 2, color='black',
                           linestyle='--', alpha=0.3)

            # Customize subplot
            ax.set_title(f'Hot Access Count: {hot_count}')
            ax.set_xticks([xi + width/2 for xi in x])
            ax.set_xticklabels(intervals, rotation=0)
            ax.set_xlabel('Cold Access Interval')
            ax.set_ylabel('Page Access')

            # Only show legend for first subplot
            if idx == 0:
                ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')

        # Adjust layout
        plt.tight_layout()
        plt.subplots_adjust(top=0.9)
        plt.savefig("access_" + pattern_name + ".pdf")


# Example usage:
data = pd.read_csv('results.csv')
create_detailed_comparison_plots(data)
