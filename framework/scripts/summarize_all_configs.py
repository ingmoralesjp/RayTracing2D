#!/usr/bin/env python3
"""Create a reproducible summary for every v1.1 configuration."""
from __future__ import annotations

import csv
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INI = ROOT / "simulations" / "omnetpp.ini"
RESULTS = ROOT / "simulations" / "results"


def percentile(values: list[float], q: float) -> float:
    if not values:
        return math.nan
    data = sorted(values)
    pos = (len(data) - 1) * q
    lo, hi = math.floor(pos), math.ceil(pos)
    return data[lo] if lo == hi else data[lo] * (hi - pos) + data[hi] * (pos - lo)


def read_scalars(path: Path) -> dict[str, float]:
    values: dict[str, float] = {}
    if not path.exists():
        return values
    pattern = re.compile(r"^scalar\s+\S+\s+(\S+)\s+([^\s]+)")
    for line in path.read_text().splitlines():
        match = pattern.match(line)
        if match:
            try:
                values[match.group(1)] = float(match.group(2))
            except ValueError:
                pass
    return values


def finite_or_nan(value: float) -> float:
    """Translate the engine's internal no-path sentinel into a CSV null value."""
    return value if math.isfinite(value) and abs(value) < 1e250 else math.nan


def category(config: str) -> str:
    if config == "UrbanStudentMapImportCheck":
        return "real-map-import-check"
    if config == "UrbanBase":
        return "urban-base"
    if re.match(r"Urban0[1-5]_", config):
        return "urban-incremental"
    if config.startswith("Urban"):
        return "urban-sensitivity"
    return "regression"


configs = re.findall(r"^\[Config ([^]]+)\]", INI.read_text(), flags=re.MULTILINE)
summary: list[dict[str, object]] = []
for config in configs:
    sca = read_scalars(RESULTS / f"{config}-0.sca")
    grid_path = RESULTS / f"{config}-grid.csv"
    grid_rows: list[dict[str, str]] = []
    if grid_path.exists():
        with grid_path.open(newline="") as stream:
            grid_rows = list(csv.DictReader(stream))

    powers = [float(r["received_power_dbm"]) for r in grid_rows]
    powers = [value for value in powers if math.isfinite(value)]
    paths = [int(r["retained_paths"]) for r in grid_rows]
    delay = [float(r["rms_delay_spread_ns"]) for r in grid_rows]
    delay = [value for value in delay if math.isfinite(value)]

    samples = len(grid_rows) or int(sca.get("gridSampleCount", 0))
    finite = len(powers) if grid_rows else int(sca.get("gridFiniteSamples", 0))
    summary.append({
        "config": config,
        "category": category(config),
        "status": "executed" if sca else "not-run",
        "grid_samples": samples,
        "finite_samples": finite,
        "coverage_fraction": finite / samples if samples else math.nan,
        "mean_pr_dbm": sum(powers) / len(powers) if powers else sca.get("gridMeanReceivedPowerDbm", math.nan),
        "median_pr_dbm": percentile(powers, 0.5),
        "p10_pr_dbm": percentile(powers, 0.1),
        "p90_pr_dbm": percentile(powers, 0.9),
        "mean_retained_paths": sum(paths) / len(paths) if paths else math.nan,
        "max_retained_paths": max(paths) if paths else math.nan,
        "total_candidate_paths": sum(int(r["candidate_paths"]) for r in grid_rows) if grid_rows else sca.get("gridTotalCandidatePaths", 0),
        "mean_rms_delay_ns": sum(delay) / len(delay) if delay else math.nan,
        "grid_seconds": sca.get("gridEvaluationSeconds", 0),
        "material_evaluated_paths": sca.get("gridMaterialEvaluatedPaths", 0),
        "material_out_of_range_paths": sca.get("gridMaterialOutOfRangePaths", 0),
        "cells_with_material_extrapolation": sca.get("gridCellsWithMaterialExtrapolation", 0),
        "frequency_sweep_samples": sca.get("frequencySweepSampleCount", 0),
        "frequency_sweep_min_pr_dbm": sca.get("frequencySweepMinReceivedPowerDbm", math.nan),
        "frequency_sweep_max_pr_dbm": sca.get("frequencySweepMaxReceivedPowerDbm", math.nan),
        "frequency_sweep_ripple_db": sca.get("frequencySweepRippleDb", math.nan),
        "frequency_sweep_out_of_range_samples": sca.get("frequencySweepMaterialOutOfRangeSamples", 0),
        "first_order_theoretical": sca.get("firstOrderTheoretical", 0),
        "first_order_evaluated": sca.get("firstOrderCandidates", 0),
        "first_order_pruned": sca.get("firstOrderPruned", 0),
        "second_order_theoretical": sca.get("secondOrderTheoretical", 0),
        "second_order_evaluated": sca.get("secondOrderCandidates", 0),
        "second_order_pruned": sca.get("secondOrderPruned", 0),
        "explicit_rx_pr_dbm": finite_or_nan(sca.get("totalReceivedPowerDbm", math.nan)),
    })

output = RESULTS / "AllConfigs_summary.csv"
with output.open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(summary[0]))
    writer.writeheader()
    writer.writerows(summary)

print(output)
for row in summary:
    print(f"{row['config']}: {row['status']}, grid={row['grid_samples']}, "
          f"finite={row['finite_samples']}, meanPr={float(row['mean_pr_dbm']):.2f} dBm")
