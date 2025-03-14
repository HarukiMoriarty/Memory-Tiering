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
    time_points = np.arange(time_interval, time_interval * (len(df) + 1), time_interval)
    
    # Create a figure with 4 subplots stacked vertically (adding the new count plot)
    fig, (ax1, ax2, ax3, ax4) = plt.subplots(4, 1, figsize=(30, 20))
    
    # Plot 1: Latency
    ax1.plot(time_points, df['Latency(ns)'], 'b-', linewidth=2, marker='o')
    ax1.set_title('Average Latency Over Time', fontsize=14)
    ax1.set_ylabel('Latency (ns)', fontsize=12)
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Only show some time points on x-axis (every 5 intervals)
    x_ticks = np.arange(0, max(time_points) + 5*time_interval, 5*time_interval)
    x_ticks[0] = time_interval  # Start from first interval instead of 0
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
    ax3.plot(time_points, df['TotalAccess'], 'k-', linewidth=2, marker='*', label='Total Access')
    ax3.set_title('Access Types Over Time', fontsize=14)
    ax3.set_ylabel('Number of Accesses', fontsize=12)
    ax3.legend(loc='best')
    ax3.grid(True, linestyle='--', alpha=0.7)
    ax3.set_xticks(x_ticks)
    
    # Plot 4: Count Types (New plot)
    ax4.plot(time_points, df['LocalCount'], 'g-', linewidth=2, marker='o', label='Local Count')
    ax4.plot(time_points, df['RemoteCount'], 'b-', linewidth=2, marker='s', label='Remote Count')
    ax4.plot(time_points, df['PmemCount'], 'r-', linewidth=2, marker='^', label='Pmem Count')
    ax4.set_title('Count Types Over Time', fontsize=14)
    ax4.set_xlabel(f'Time (seconds, interval={time_interval}s)', fontsize=12)
    ax4.set_ylabel('Count Values', fontsize=12)
    ax4.legend(loc='best')
    ax4.grid(True, linestyle='--', alpha=0.7)
    ax4.set_xticks(x_ticks)
    
    # Adjust layout
    plt.tight_layout()
    
    # Generate output filename based on input filename
    base_name = os.path.splitext(os.path.basename(filename))[0]
    output_file = f'fig/{base_name}_visualization.pdf'
    
    # Create fig directory if it doesn't exist
    os.makedirs('fig', exist_ok=True)
    
    # Save figure
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Figure saved as: {output_file}")

if __name__ == "__main__":
    # Check if parameters are provided
    if len(sys.argv) < 2:
        print("Usage: python script.py <csv_file_path> [time_interval]")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    
    # Check if file exists
    if not os.path.exists(csv_file):
        print(f"Error: File '{csv_file}' not found")
        sys.exit(1)
    
    # Get time interval if provided, otherwise use default of 10
    time_interval = 10
    if len(sys.argv) > 2:
        try:
            time_interval = int(sys.argv[2])
        except ValueError:
            print(f"Error: Time interval must be an integer. Using default value of 10.")
    
    # Call the plotting function with the specified parameters
    plot_metrics(csv_file, time_interval)