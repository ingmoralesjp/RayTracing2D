#!/usr/bin/env python3
"""Validate the synthetic v1.4 measurement-evaluation contract."""
from __future__ import annotations

import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUMMARY = ROOT / "validation" / "results" / "synthetic_summary.csv"
PAIRS = ROOT / "validation" / "results" / "synthetic_pairs.csv"

with SUMMARY.open(newline="") as stream:
    rows = {row["set"]: row for row in csv.DictReader(stream)}
assert set(rows) == {"all", "calibration", "test"}
assert int(rows["all"]["n"]) == 10
assert math.isclose(float(rows["all"]["mae_db"]), 1.55, abs_tol=1e-10)
assert math.isclose(float(rows["all"]["rmse_db"]), math.sqrt(3.275), abs_tol=1e-10)
assert math.isclose(float(rows["all"]["bias_db"]), 0.05, abs_tol=1e-10)
assert int(rows["calibration"]["n"]) == int(rows["test"]["n"]) == 5
with PAIRS.open(newline="") as stream:
    pairs = list(csv.DictReader(stream))
assert len(pairs) == 10
assert all(float(row["match_distance_m"]) == 0.0 for row in pairs)
print("RayTracing2D v1.4 synthetic validation harness: PASS")
