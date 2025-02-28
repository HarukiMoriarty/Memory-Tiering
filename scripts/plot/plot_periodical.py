import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import sys

# Read the data from file
def plot_metrics(filename):
    # Read data from the CSV file
    df = pd.read_csv(filename)
    
    # Generate time axis (10s, 20s, etc.)
    time_points = np.arange(10, 10 * (len(df) + 1), 10)
    
    # Create a figure with 3 subplots stacked vertically
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(30, 15))
    
    # Plot 1: Latency
    ax1.plot(time_points, df['Latency(ns)'], 'b-', linewidth=2, marker='o')
    ax1.set_title('Average Latency Over Time', fontsize=14)
    ax1.set_ylabel('Latency (ns)', fontsize=12)
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Only show some time points on x-axis (every 50 seconds)
    x_ticks = np.arange(0, max(time_points) + 50, 50)
    x_ticks[0] = 10  # Start from 10s instead of 0
    ax1.set_xticks(x_ticks)
    
    # Plot 2: Throughput
    ax2.plot(time_points, df['Throughput(ops/s)'] / 1e6, 'r-', linewidth=2, marker='o')  # Convert to millions
    ax2.set_title('Throughput Over Time', fontsize=14)
    ax2.set_ylabel('Throughput (M ops/s)', fontsize=12)
    ax2.grid(True, linestyle='--', alpha=0.7)
    ax2.set_xticks(x_ticks)
    
    # Plot 3: Access Types
    ax3.plot(time_points, df['LocalAccess'], 'g-', linewidth=2, marker='o', label='Local Access')
    ax3.plot(time_points, df['RemoteAccess'], 'b-', linewidth=2, marker='s', label='Remote Access')
    ax3.plot(time_points, df['PmemAccess'], 'r-', linewidth=2, marker='^', label='Pmem Access')
    ax3.set_title('Access Types Over Time', fontsize=14)
    ax3.set_xlabel('Time (seconds)', fontsize=12)
    ax3.set_ylabel('Number of Accesses', fontsize=12)
    ax3.legend(loc='best')
    ax3.grid(True, linestyle='--', alpha=0.7)
    ax3.set_xticks(x_ticks)
    
    # Adjust layout
    plt.tight_layout()
    
    # Generate output filename based on input filename
    base_name = os.path.splitext(os.path.basename(filename))[0]
    output_file = f'fig/{base_name}_visualization.pdf'
    
    # Save figure
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Figure saved as: {output_file}")

if __name__ == "__main__":
    # Check if file parameter is provided
    if len(sys.argv) > 1:
        csv_file = sys.argv[1]
        if not os.path.exists(csv_file):
            print(f"Error: File '{csv_file}' not found")
            sys.exit(1)
        plot_metrics(csv_file)
    else:
        print("Usage: python script.py <csv_file_path>")
        sys.exit(1)