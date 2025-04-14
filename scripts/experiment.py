import argparse
import itertools
import logging
import subprocess
import yaml
from pathlib import Path
from typing import Dict, List, Any

BASE_PATH = Path(__file__).absolute().parent
LOG = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format="%(message)s")


def load_experiment(exp_name: str) -> Dict[str, Any]:
    exp_path = BASE_PATH / "experiment" / f"{exp_name}.yaml"
    if not exp_path.exists():
        LOG.error(f"Experiment file {exp_path} not found.")
        return {}
    with open(exp_path) as f:
        return yaml.safe_load(f)


def generate_combinations(config: Dict[str, Any]):
    keys = config.keys()
    values = (config[k] if isinstance(config[k], list)
              else [config[k]] for k in keys)

    for combo in itertools.product(*values):
        param_dict = dict(zip(keys, combo))

        # Skip invalid combinations where cold_threshold <= hot_threshold
        if param_dict["policy_type"] in ["lru", "hybrid"]:
            if param_dict["cold_threshold"] < param_dict["hot_threshold"]:
                continue
        if param_dict["policy_type"] in ["frequency", "hybrid"]:
            if param_dict["cold_count"] > param_dict["hot_count"]:
                continue

        yield param_dict


def run_experiment(params: Dict[str, Any], exp_name: str):
    # Create unique identifier based on key config values
    identifier_parts = [
        params["pattern"],
        params["client_tier_sizes"].replace(" ", "-"),
        params["server_mem_sizes"].replace(",", "-"),
        f"buf{params['buffer_size']}",
        f"scan{params['scan_interval']}",
        f"sample{params['sample_rate']}",
        params["policy_type"],
    ]

    # Add policy-specific identifiers
    if params["policy_type"] == "lru":
        identifier_parts += [f"h{params['hot_threshold']}",
                             f"c{params['cold_threshold']}"]
    elif params["policy_type"] == "frequency":
        identifier_parts += [f"hc{params['hot_count']}",
                             f"cc{params['cold_count']}"]
    elif params["policy_type"] == "hybrid":
        identifier_parts += [
            f"h{params['hot_threshold']}", f"c{params['cold_threshold']}",
            f"hc{params['hot_count']}", f"cc{params['cold_count']}",
            f"wr{params['recency_weight']}", f"wf{params['frequency_weight']}"
        ]

    identifier = "_".join(identifier_parts)

    # Set output paths based on identifier
    result_dir = BASE_PATH / "result" / exp_name
    result_dir.mkdir(parents=True, exist_ok=True)
    cdf_output_path = result_dir / f"cdf_{identifier}.csv"
    periodic_output_path = result_dir / f"periodic_{identifier}.csv"

    params["cdf_output"] = str(cdf_output_path)
    params["periodic_output"] = str(periodic_output_path)

    # Build CLI command
    cmd = [
        "./build/main",
        "-p", params["pattern"],
        "-c", params["client_tier_sizes"],
        "-b", str(params["buffer_size"]),
        "--running-time", str(params["running_time"]),
        "-s", params["server_mem_sizes"],
        "--policy-type", params["policy_type"],
        "-t", str(params["num_tiers"]),
        "--scan-interval", str(params["scan_interval"]),
        "--sample-rate", str(params["sample_rate"]),
        "--output", params["cdf_output"],
        "--periodic-output", params["periodic_output"]
    ]

    # Policy-specific flags
    if params["policy_type"] == "lru":
        cmd += ["--hot-threshold", str(params["hot_threshold"]),
                "--cold-threshold", str(params["cold_threshold"])]
    elif params["policy_type"] == "frequency":
        cmd += ["--hot-count", str(params["hot_count"]),
                "--cold-count", str(params["cold_count"])]
    elif params["policy_type"] == "hybrid":
        cmd += [
            "--hot-threshold", str(params["hot_threshold"]),
            "--cold-threshold", str(params["cold_threshold"]),
            "--hot-count", str(params["hot_count"]),
            "--cold-count", str(params["cold_count"]),
            "--recency-weight", str(params["recency_weight"]),
            "--frequency-weight", str(params["frequency_weight"])
        ]

    LOG.info(f"Running command: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def main():
    parser = argparse.ArgumentParser(
        description="Run memory tiering experiments.")
    parser.add_argument(
        "experiment", help="Experiment config name (without .yaml)")
    args = parser.parse_args()

    config = load_experiment(args.experiment)
    if not config:
        return

    for combo in generate_combinations(config):
        try:
            run_experiment(combo, args.experiment)
        except subprocess.CalledProcessError as e:
            LOG.error(f"Experiment failed: {e}")


if __name__ == "__main__":
    main()
