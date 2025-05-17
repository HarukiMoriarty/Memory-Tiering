import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import sys

# Read the data from file


def plot_metrics(filename, time_interval=10):
    # Read data from the CSV file
    df = pd.read_csv(filename)

    # Generate time axis based on time_interval
    time_points = np.arange(
        time_interval, time_interval * (len(df) + 1), time_interval)

    # Generate output base filename
    base_name = os.path.splitext(os.path.basename(filename))[0]

    # Create fig directory if it doesn't exist
    os.makedirs('fig', exist_ok=True)

    # Reduce the number of x-ticks
    tick_count = min(7, len(time_points))
    step = max(1, len(time_points) // tick_count)
    x_ticks = time_points[::step]
    if len(time_points) > 0 and time_points[-1] not in x_ticks:
        x_ticks = np.append(x_ticks, time_points[-1])

    # Plot 1: Latency
    plt.figure(figsize=(15, 8))
    plt.plot(time_points, df['Latency(ns)'], 'b-', linewidth=2, marker='o')
    plt.title('Average Latency Over Time', fontsize=14)
    plt.ylabel('Latency (ns)', fontsize=12)
    plt.xlabel(f'Time (seconds, interval={time_interval}s)', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(x_ticks)
    plt.tight_layout()
    output_file = f'fig/{base_name}_latency.pdf'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Latency figure saved as: {output_file}")
    plt.close()

    # Plot 2: Throughput
    plt.figure(figsize=(15, 8))
    plt.plot(time_points, df['Throughput(ops/s)'] /
             1e6, 'r-', linewidth=2, marker='o')
    plt.title('Throughput Over Time', fontsize=14)
    plt.ylabel('Throughput (M ops/s)', fontsize=12)
    plt.xlabel(f'Time (seconds, interval={time_interval}s)', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(x_ticks)
    plt.tight_layout()
    output_file = f'fig/{base_name}_throughput.pdf'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Throughput figure saved as: {output_file}")
    plt.close()

    # Plot 3: Access Types
    plt.figure(figsize=(15, 8))
    plt.plot(time_points, df['LocalAccess'], 'g-',
             linewidth=2, marker='o', label='Local Access')
    plt.plot(time_points, df['RemoteAccess'], 'b-',
             linewidth=2, marker='s', label='Remote Access')
    plt.plot(time_points, df['PmemAccess'], 'r-',
             linewidth=2, marker='^', label='Pmem Access')
    plt.plot(time_points, df['TotalAccess'], 'k-',
             linewidth=2, marker='*', label='Total Access')
    plt.title('Access Types Over Time', fontsize=14)
    plt.ylabel('Number of Accesses', fontsize=12)
    plt.xlabel(f'Time (seconds, interval={time_interval}s)', fontsize=12)
    plt.legend(loc='best')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(x_ticks)
    plt.tight_layout()
    output_file = f'fig/{base_name}_access_types.pdf'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Access Types figure saved as: {output_file}")
    plt.close()

    # Plot 4: Count Types
    plt.figure(figsize=(15, 8))
    plt.plot(time_points, df['LocalCount'], 'g-',
             linewidth=2, marker='o', label='Local Count')
    plt.plot(time_points, df['RemoteCount'], 'b-',
             linewidth=2, marker='s', label='Remote Count')
    plt.plot(time_points, df['PmemCount'], 'r-',
             linewidth=2, marker='^', label='Pmem Count')
    plt.title('Count Types Over Time', fontsize=14)
    plt.xlabel(f'Time (seconds, interval={time_interval}s)', fontsize=12)
    plt.ylabel('Count Values', fontsize=12)
    plt.legend(loc='best')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(x_ticks)
    plt.tight_layout()
    output_file = f'fig/{base_name}_count_types.pdf'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Count Types figure saved as: {output_file}")
    plt.close()

    # --- New Plot: Subplots for Transfer Counts ---
    fig, axs = plt.subplots(3, 1, figsize=(15, 18))

    # Subplot 1: local2remote vs remote2local
    axs[0].plot(time_points, df['local2remote'], 'b-',
                linewidth=2, marker='o', label='Local to Remote')
    axs[0].plot(time_points, df['remote2local'], 'r-',
                linewidth=2, marker='s', label='Remote to Local')
    axs[0].set_title('Local-Remote Transfer Over Time', fontsize=14)
    axs[0].set_ylabel('Transfer Count', fontsize=12)
    axs[0].grid(True, linestyle='--', alpha=0.7)
    axs[0].legend()
    axs[0].set_xticks(x_ticks)

    # Subplot 2: remote2pmem vs pmem2remote
    axs[1].plot(time_points, df['remote2pmem'], 'g-',
                linewidth=2, marker='^', label='Remote to Pmem')
    axs[1].plot(time_points, df['pmem2remote'], 'm-',
                linewidth=2, marker='v', label='Pmem to Remote')
    axs[1].set_title('Remote-Pmem Transfer Over Time', fontsize=14)
    axs[1].set_ylabel('Transfer Count', fontsize=12)
    axs[1].grid(True, linestyle='--', alpha=0.7)
    axs[1].legend()
    axs[1].set_xticks(x_ticks)

    # Subplot 3: local2pmem vs pmem2local
    axs[2].plot(time_points, df['local2pmem'], 'c-',
                linewidth=2, marker='>', label='Local to Pmem')
    axs[2].plot(time_points, df['pmem2local'], 'y-',
                linewidth=2, marker='<', label='Pmem to Local')
    axs[2].set_title('Local-Pmem Transfer Over Time', fontsize=14)
    axs[2].set_ylabel('Transfer Count', fontsize=12)
    axs[2].set_xlabel(
        f'Time (seconds, interval={time_interval}s)', fontsize=12)
    axs[2].grid(True, linestyle='--', alpha=0.7)
    axs[2].legend()
    axs[2].set_xticks(x_ticks)

    plt.tight_layout()
    output_file = f'fig/{base_name}_transfer_types.pdf'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Transfer Types figure saved as: {output_file}")
    plt.close()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python <csv_file_path> [time_interval]")
        sys.exit(1)

    csv_file = sys.argv[1]

    if not os.path.exists(csv_file):
        print(f"Error: File '{csv_file}' not found")
        sys.exit(1)

    time_interval = 10
    if len(sys.argv) > 2:
        try:
            time_interval = int(sys.argv[2])
        except ValueError:
            print("Error: Time interval must be an integer. Using default value of 10.")

    plot_metrics(csv_file, time_interval)
