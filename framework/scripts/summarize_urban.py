#!/usr/bin/env python3
"""Summarize the incremental v1.1 urban grid CSV files."""
from __future__ import annotations

import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "simulations" / "results"
CONFIGS = ["Urban01_FSPL", "Urban02_Blockage", "Urban03_R1", "Urban04_R1R2", "Urban05_Full"]


def safe_float(value: str) -> float:
    try:
        return float(value)
    except ValueError:
        return math.nan


def percentile(values, q):
    if not values:
        return math.nan
    data = sorted(values)
    if len(data) == 1:
        return data[0]
    pos = (len(data) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return data[lo]
    return data[lo] * (hi - pos) + data[hi] * (pos - lo)


summary_rows = []
for config in CONFIGS:
    path = RESULTS / f"{config}-grid.csv"
    if not path.exists():
        raise SystemExit(f"Missing {path}. Run scripts/run_urban_suite.sh after building OMNeT++.")
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))

    powers = [safe_float(r["received_power_dbm"]) for r in rows]
    powers = [v for v in powers if math.isfinite(v)]
    no_path = len(rows) - len(powers)
    path_counts = [int(r["retained_paths"]) for r in rows]
    candidates = [int(r["candidate_paths"]) for r in rows]
    delay_spreads = [safe_float(r["rms_delay_spread_ns"]) for r in rows]
    delay_spreads = [v for v in delay_spreads if math.isfinite(v)]

    summary_rows.append({
        "config": config,
        "grid_samples": len(rows),
        "finite_samples": len(powers),
        "no_path_samples": no_path,
        "coverage_fraction": len(powers) / len(rows) if rows else math.nan,
        "mean_pr_dbm": sum(powers) / len(powers) if powers else math.nan,
        "median_pr_dbm": percentile(powers, 0.5),
        "p10_pr_dbm": percentile(powers, 0.1),
        "p90_pr_dbm": percentile(powers, 0.9),
        "min_pr_dbm": min(powers) if powers else math.nan,
        "max_pr_dbm": max(powers) if powers else math.nan,
        "mean_retained_paths": sum(path_counts) / len(path_counts) if path_counts else math.nan,
        "max_retained_paths": max(path_counts) if path_counts else 0,
        "total_candidate_paths": sum(candidates),
        "mean_rms_delay_ns": sum(delay_spreads) / len(delay_spreads) if delay_spreads else math.nan,
    })

output = RESULTS / "Urban_v1.1_summary.csv"
with output.open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(summary_rows[0]))
    writer.writeheader()
    writer.writerows(summary_rows)

print(output)
for row in summary_rows:
    print(f"{row['config']}: finite={row['finite_samples']}/{row['grid_samples']} "
          f"meanPr={row['mean_pr_dbm']:.2f} dBm meanPaths={row['mean_retained_paths']:.2f}")
