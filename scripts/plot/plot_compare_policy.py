
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from collections import defaultdict

# === Setup ===
data_dir = "./scripts/result/zipfian"
output_dir = os.path.join(data_dir, "fig")
os.makedirs(output_dir, exist_ok=True)

# Group by base prefix (zipfian or uniform workload types)
grouped_files = defaultdict(list)
pattern = re.compile(
    r"^(periodic_(uniform|zipfian).*?)_(lru|frequency|hybrid)_.*\.csv$")

for filename in os.listdir(data_dir):
    if filename.endswith(".csv") and filename.startswith("periodic_"):
        match = pattern.match(filename)
        if match:
            prefix = match.group(1)
            grouped_files[prefix].append(filename)

# === Plotting ===
for prefix, files in grouped_files.items():
    colors = cm.get_cmap("tab20", len(files))

    # === 1. Count Group ===
    fig1, axes1 = plt.subplots(3, 1, figsize=(14, 16), sharex=True)
    plt.subplots_adjust(hspace=0.4)
    for idx, filename in enumerate(sorted(files)):
        df = pd.read_csv(os.path.join(data_dir, filename))
        if {'LocalCount', 'RemoteCount', 'PmemCount'}.issubset(df.columns):
            time = [i * 5 for i in range(len(df))]
            policy = filename.split(prefix + "_")[1].replace(".csv", "")
            color = colors(idx)
            axes1[0].plot(time, df["LocalCount"], label=policy, color=color)
            axes1[1].plot(time, df["RemoteCount"], label=policy, color=color)
            axes1[2].plot(time, df["PmemCount"], label=policy, color=color)

    for ax, title in zip(axes1, ["Local Count", "Remote Count", "Pmem Count"]):
        ax.set_title(title)
        ax.set_ylabel("Count")
        ax.grid(True)
    axes1[2].set_xlabel("Time (s)")
    axes1[0].legend(bbox_to_anchor=(1.05, 1),
                    loc='upper left', fontsize='small', ncol=2)
    plt.suptitle(f"Memory Counts - Group: {prefix}", fontsize=16)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(os.path.join(
        output_dir, f"{prefix.replace('/', '_')}_count_group.png"))
    plt.close()

    # === 2. Access Group ===
    fig2, axes2 = plt.subplots(3, 1, figsize=(14, 16), sharex=True)
    plt.subplots_adjust(hspace=0.4)
    for idx, filename in enumerate(sorted(files)):
        df = pd.read_csv(os.path.join(data_dir, filename))
        if {'LocalAccess', 'RemoteAccess', 'PmemAccess'}.issubset(df.columns):
            time = [i * 5 for i in range(len(df))]
            policy = filename.split(prefix + "_")[1].replace(".csv", "")
            color = colors(idx)
            axes2[0].plot(time, df["LocalAccess"], label=policy, color=color)
            axes2[1].plot(time, df["RemoteAccess"], label=policy, color=color)
            axes2[2].plot(time, df["PmemAccess"], label=policy, color=color)

    for ax, title in zip(axes2, ["Local Access", "Remote Access", "Pmem Access"]):
        ax.set_title(title)
        ax.set_ylabel("Accesses")
        ax.grid(True)
    axes2[2].set_xlabel("Time (s)")
    axes2[0].legend(bbox_to_anchor=(1.05, 1),
                    loc='upper left', fontsize='small', ncol=2)
    plt.suptitle(f"Access Counts - Group: {prefix}", fontsize=16)
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(os.path.join(
        output_dir, f"{prefix.replace('/', '_')}_access_group.png"))
    plt.close()

    # === 3. Latency Group ===
    fig3, ax3 = plt.subplots(figsize=(14, 6))
    for idx, filename in enumerate(sorted(files)):
        df = pd.read_csv(os.path.join(data_dir, filename))
        if "Latency(ns)" in df.columns:
            time = [i * 5 for i in range(len(df))]
            policy = filename.split(prefix + "_")[1].replace(".csv", "")
            color = colors(idx)
            ax3.plot(time, df["Latency(ns)"], label=policy, color=color)
    ax3.set_title(f"Latency - Group: {prefix}")
    ax3.set_xlabel("Time (s)")
    ax3.set_ylabel("Latency (ns)")
    ax3.grid(True)
    ax3.legend(bbox_to_anchor=(1.05, 1),
               loc='upper left', fontsize='small', ncol=2)
    plt.tight_layout()
    plt.savefig(os.path.join(
        output_dir, f"{prefix.replace('/', '_')}_latency_group.png"))
    plt.close()

    # === 4. Throughput Group ===
    fig4, ax4 = plt.subplots(figsize=(14, 6))
    for idx, filename in enumerate(sorted(files)):
        df = pd.read_csv(os.path.join(data_dir, filename))
        if "Throughput(ops/s)" in df.columns:
            time = [i * 5 for i in range(len(df))]
            policy = filename.split(prefix + "_")[1].replace(".csv", "")
            color = colors(idx)
            ax4.plot(time, df["Throughput(ops/s)"], label=policy, color=color)
    ax4.set_title(f"Throughput - Group: {prefix}")
    ax4.set_xlabel("Time (s)")
    ax4.set_ylabel("Throughput (ops/s)")
    ax4.grid(True)
    ax4.legend(bbox_to_anchor=(1.05, 1),
               loc='upper left', fontsize='small', ncol=2)
    plt.tight_layout()
    plt.savefig(os.path.join(
        output_dir, f"{prefix.replace('/', '_')}_throughput_group.png"))
    plt.close()
