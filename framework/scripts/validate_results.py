#!/usr/bin/env python3
"""Validate reproducible regression and v1.1 urban outputs after Cmdenv runs."""
from __future__ import annotations

import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "simulations" / "results"


def rows(name):
    with (RESULTS / name).open(newline="") as stream:
        return list(csv.DictReader(stream))


def close(value, expected, tolerance=1e-8):
    if not math.isclose(float(value), expected, rel_tol=tolerance, abs_tol=tolerance):
        raise AssertionError(f"{value} != {expected}")


# ---------------------------------------------------------------------------
# v1.0 regression contracts retained in v1.1
# ---------------------------------------------------------------------------
single = rows("SingleReflection-rays.csv")
assert len(single) == 2 and "(450:400)" in single[1]["points"]

second = rows("SecondOrderParallelWalls-rays.csv")
assert len(second) == 2 and all(row["reflection_order"] == "2" for row in second)
close(second[0]["distance_m"], 943.39811320566)

te = rows("FresnelConcreteTE-rays.csv")[0]
tm = rows("FresnelConcreteTM-rays.csv")[0]
close(te["reflection_magnitude"], 0.485109590479)
close(tm["reflection_magnitude"], 0.296785667636)
assert te["material_model_in_range"] == tm["material_model_in_range"] == "1"

diffraction = rows("KnifeEdgeBlocked-rays.csv")
assert len(diffraction) == 2 and all(row["diffraction_order"] == "1" for row in diffraction)
close(diffraction[0]["diffraction_loss_db"], 41.3664800658)

grid = rows("CoverageGrid-grid.csv")
assert len(grid) == 25
blocked = next(row for row in grid if row["x_m"] == "80" and row["y_m"] == "25")
assert blocked["direct_blocked"] == "1" and blocked["retained_paths"] == "0"
assert math.isnan(float(blocked["received_power_dbm"]))

osm_scalar = (RESULTS / "OsmBuilding-0.sca").read_text()
assert "osmNodeCount 4" in osm_scalar and "osmBuildingCount 1" in osm_scalar
assert "wallCount 4" in osm_scalar

# ---------------------------------------------------------------------------
# v1.1 urban end-to-end contracts
# ---------------------------------------------------------------------------
configs = ["Urban01_FSPL", "Urban02_Blockage", "Urban03_R1", "Urban04_R1R2", "Urban05_Full"]
urban = {cfg: rows(f"{cfg}-grid.csv") for cfg in configs}
lengths = {cfg: len(data) for cfg, data in urban.items()}
assert len(set(lengths.values())) == 1 and next(iter(lengths.values())) > 0


def finite_count(data):
    return sum(math.isfinite(float(r["received_power_dbm"])) for r in data)


finite = {cfg: finite_count(data) for cfg, data in urban.items()}
assert finite["Urban01_FSPL"] == lengths["Urban01_FSPL"]
assert finite["Urban02_Blockage"] <= finite["Urban03_R1"]
assert finite["Urban03_R1"] <= finite["Urban04_R1R2"]
assert finite["Urban04_R1R2"] <= finite["Urban05_Full"]

# Candidate enumeration must reflect the mechanisms enabled by each step.
assert all(int(r["reflection1_paths"]) == 0 for r in urban["Urban02_Blockage"])
assert any(int(r["reflection1_paths"]) > 0 for r in urban["Urban03_R1"])
assert any(int(r["second_order_candidates"]) > 0 for r in urban["Urban04_R1R2"])
assert any(int(r["diffraction_candidates"]) > 0 for r in urban["Urban05_Full"])

summary = RESULTS / "Urban_v1.1_summary.csv"
assert summary.exists(), "Run scripts/summarize_urban.py after the urban suite"

# ---------------------------------------------------------------------------
# v1.1 sensitivity and completeness contracts (Urban06--Urban15)
# ---------------------------------------------------------------------------
sensitivity_names = [
    "Urban06_Full_TM", "Urban07_Full_Incoherent", "Urban08_Full_916MHz",
    "Urban09_DelaySpread", "Urban10_Full_Guarded", "Urban11_Grid20m",
    "Urban12_Grid5m", "Urban13_AllConcrete", "Urban14_AllGlass",
    "Urban15_MaxPaths16",
]
sensitivity = {name: rows(f"{name}-grid.csv") for name in sensitivity_names}
for name in sensitivity_names:
    assert sensitivity[name], f"Missing sensitivity output for {name}"

assert len(sensitivity["Urban11_Grid20m"]) == 121
assert len(sensitivity["Urban12_Grid5m"]) == 1849
for name in sensitivity_names:
    if name not in {"Urban11_Grid20m", "Urban12_Grid5m"}:
        assert len(sensitivity[name]) == 484

def finite_rows(data):
    return [row for row in data if math.isfinite(float(row["received_power_dbm"]))]

full = urban["Urban05_Full"]
tm = sensitivity["Urban06_Full_TM"]
incoherent = sensitivity["Urban07_Full_Incoherent"]
delay_view = sensitivity["Urban09_DelaySpread"]
guarded = sensitivity["Urban10_Full_Guarded"]
max16 = sensitivity["Urban15_MaxPaths16"]

assert len(finite_rows(tm)) == len(finite_rows(full)) == 340
assert any(abs(float(a["received_power_dbm"]) - float(b["received_power_dbm"])) > 1e-6
           for a, b in zip(full, tm)
           if math.isfinite(float(a["received_power_dbm"])) and math.isfinite(float(b["received_power_dbm"])))
for row in finite_rows(incoherent):
    close(row["received_power_dbm"], float(row["incoherent_power_dbm"]), tolerance=1e-9)
assert any(abs(float(a["received_power_dbm"]) - float(b["received_power_dbm"])) > 1e-3
           for a, b in zip(full, incoherent)
           if math.isfinite(float(a["received_power_dbm"])) and math.isfinite(float(b["received_power_dbm"])))

# Urban09 changes only the Qtenv visualization metric; numerical CSV is identical.
assert all(a == b for a, b in zip(full, delay_view))

# Path results remain identical. In v1.3 the distance guard also changes the
# candidate-accounting columns because conservative pruning happens pre-trace.
physics_fields = [
    "retained_paths", "los_paths", "reflection1_paths", "reflection2_paths",
    "diffraction_paths", "received_power_dbm", "incoherent_power_dbm",
    "equivalent_path_loss_db", "strongest_path_dbm", "mean_delay_ns",
    "rms_delay_spread_ns", "max_excess_delay_ns",
]
for reference, limited in zip(full, guarded):
    for field in physics_fields:
        a, b = float(reference[field]), float(limited[field])
        assert (math.isnan(a) and math.isnan(b)) or math.isclose(a, b, rel_tol=1e-10, abs_tol=1e-10)
assert all(a == b for a, b in zip(full, max16))

assert any(abs(float(a["received_power_dbm"]) - float(b["received_power_dbm"])) > 1e-6
           for a, b in zip(sensitivity["Urban13_AllConcrete"], sensitivity["Urban14_AllGlass"])
           if math.isfinite(float(a["received_power_dbm"])) and math.isfinite(float(b["received_power_dbm"])))

low_frequency = sensitivity["Urban08_Full_916MHz"]
assert sum(int(row["material_out_of_range_paths"]) for row in low_frequency) > 0
assert sum(int(row["material_out_of_range_paths"]) for row in full) == 0

all_summary = RESULTS / "AllConfigs_summary.csv"
assert all_summary.exists(), "Run scripts/summarize_all_configs.py"
with all_summary.open(newline="") as stream:
    all_rows = list(csv.DictReader(stream))
expected_configs = 32  # v1.1 set + two v1.2 sweeps + three v1.3 pruning cases.
assert len(all_rows) == expected_configs
map_check = next(row for row in all_rows if row["config"] == "UrbanStudentMapImportCheck")
assert map_check["status"] in {"not-run", "executed"}
if (RESULTS / "UrbanStudentMapImportCheck-0.sca").exists():
    assert map_check["status"] == "executed"
    map_scalar = (RESULTS / "UrbanStudentMapImportCheck-0.sca").read_text()
    assert "osmNodeCount 25214" in map_scalar
    assert "osmBuildingCount 2721" in map_scalar
for row in all_rows:
    value = float(row["explicit_rx_pr_dbm"])
    assert math.isnan(value) or abs(value) < 1e250, "Internal no-path sentinel leaked into summary"

# ---------------------------------------------------------------------------
# v1.2 frequency-response contracts
# ---------------------------------------------------------------------------
for name in ["FrequencySweepSingleReflection", "Urban16_FrequencySweep"]:
    frequency_rows = rows(f"{name}-frequency.csv")
    assert len(frequency_rows) == 201
    close(frequency_rows[0]["frequency_hz"], 2.3e9)
    close(frequency_rows[-1]["frequency_hz"], 2.5e9)
    assert all(math.isfinite(float(row["received_power_dbm"])) for row in frequency_rows)

controlled_sweep = rows("FrequencySweepSingleReflection-frequency.csv")
controlled_powers = [float(row["received_power_dbm"]) for row in controlled_sweep]
assert max(controlled_powers) - min(controlled_powers) > 3.0
urban_sweep_scalar = (RESULTS / "Urban16_FrequencySweep-0.sca").read_text()
assert "frequencySweepSampleCount 201" in urban_sweep_scalar

# ---------------------------------------------------------------------------
# v1.3 conservative pre-trace pruning contracts
# ---------------------------------------------------------------------------
pruned_grid = rows("Urban17_Pruned600m-grid.csv")
assert len(pruned_grid) == len(full)
for reference, pruned in zip(full, pruned_grid):
    for field in physics_fields:
        a, b = float(reference[field]), float(pruned[field])
        assert (math.isnan(a) and math.isnan(b)) or math.isclose(a, b, rel_tol=1e-10, abs_tol=1e-10)

r1_map = (RESULTS / "UrbanStudentMapPrunedR1-0.sca").read_text()
assert "wallCount 14182" in r1_map
assert "firstOrderTheoretical 14182" in r1_map
assert "firstOrderCandidates 4419" in r1_map
assert "firstOrderPruned 9763" in r1_map

r2_map = (RESULTS / "UrbanStudentMapPrunedR2_250m-0.sca").read_text()
assert "secondOrderTheoretical 201114942" in r2_map
assert "secondOrderCandidates 101554" in r2_map
assert "secondOrderPruned 201013388" in r2_map

print("RayTracing2D v1.3 validation matrix: PASS")
print("Validated: regression geometry/physics plus incremental urban OSM grid pipeline.")
print("Urban finite-cell counts:", finite)
print("Validated all runnable configurations plus Urban06--Urban15 sensitivities.")
