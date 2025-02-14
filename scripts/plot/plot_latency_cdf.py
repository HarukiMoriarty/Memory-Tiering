import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import argparse

def plot_latency_distribution(data_path, figsize=(10, 6)):
    """
    Create a percentile distribution plot of latency data.
    
    Parameters:
    -----------
    data_path : str
        Path to the CSV file containing latency data with percentile and latency_ns columns
    figsize : tuple, optional
        Size of the figure (width, height), default is (10, 6)
    """
    try:
        # Read the data
        df = pd.read_csv(data_path)
        
        # Convert percentile strings to numerical values where possible
        numeric_data = df[~df['percentile'].isin(['Min', 'Max', 'Mean'])].copy()
        numeric_data['percentile'] = numeric_data['percentile'].astype(float) * 100
        
        # Get min, max and mean values
        min_value = df[df['percentile'] == 'Min']['latency_ns'].values[0]
        max_value = df[df['percentile'] == 'Max']['latency_ns'].values[0]
        mean_value = df[df['percentile'] == 'Mean']['latency_ns'].values[0]
        
        plt.figure(figsize=figsize)
        
        # Create dataset including Min (0%) but excluding Max
        full_percentiles = pd.concat([
            pd.DataFrame({'percentile': [0], 'latency_ns': [min_value]}),
            numeric_data
        ]).sort_values('percentile')
        
        # Plot the main distribution line
        plt.plot(full_percentiles['percentile'], full_percentiles['latency_ns'], 
                marker='o', linestyle='-', linewidth=2, label='Latency Distribution')
        
        # Add mean line
        plt.axhline(y=mean_value, color='red', linestyle='--', 
                   label=f'Mean: {mean_value:.2f}ns')
        
        # Add min annotation
        plt.annotate(f'Min: {min_value}ns', 
                    xy=(0, min_value),
                    xytext=(-2, min_value + 10))
        
        # Add max annotation pointing to the 90th percentile point
        p90_value = numeric_data[numeric_data['percentile'] == 90]['latency_ns'].values[0]
        plt.annotate(f'Max: {max_value}ns', 
                    xy=(90, p90_value),  # Point to 90th percentile
                    xytext=(100, p90_value))  # Position text above
        
        # Customize the plot
        plt.title('Access Latency Distribution')
        plt.xlabel('Percentile (%)')
        plt.ylabel('Latency (ns)')
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        # Show the plot
        plt.tight_layout()
        plt.savefig("fig/latency_hist.png")
        
    except FileNotFoundError:
        print(f"Error: File '{data_path}' not found")
    except Exception as e:
        print(f"Error: {str(e)}")

def main():
    parser = argparse.ArgumentParser(description='Plot latency distribution from CSV file.')
    parser.add_argument('data_path', type=str, help='Path to the CSV file containing latency data')
    parser.add_argument('--figsize', type=float, nargs=2, default=[10, 6], 
                        help='Figure size as width height (default: 10 6)')
    
    args = parser.parse_args()
    plot_latency_distribution(args.data_path, figsize=tuple(args.figsize))

if __name__ == "__main__":
    main()