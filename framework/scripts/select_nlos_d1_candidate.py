#!/usr/bin/env python3
"""Select reproducible NLoS/D1 receiver candidates from a Thesis_D00 grid CSV.

The script intentionally separates geometric discovery from thesis interpretation:
- direct path must be blocked;
- at least one D1 path must survive;
- received power must be finite;
- the receiver must lie outside imported OSM building footprints;
- clean D1-only receivers (R1=R2=0) are ranked first.

It writes a ranked CSV and an INI snippet. The project ships with RX=(1800,225) m
as the v1.6 canonical case because it is clean, outside footprints, and yields one
unique D1 path after corner deduplication.
"""
from __future__ import annotations

import argparse
import csv
import math
import re
from pathlib import Path

EARTH_RADIUS_M = 6371008.8


def project(lat_deg: float, lon_deg: float, lat0_deg: float, lon0_deg: float):
    lat0 = math.radians(lat0_deg)
    x = EARTH_RADIUS_M * math.cos(lat0) * math.radians(lon_deg - lon0_deg)
    y = EARTH_RADIUS_M * math.radians(lat_deg - lat0_deg)
    return x, y


def load_building_polygons(osm_path: Path, lat0: float, lon0: float):
    text = osm_path.read_text(encoding="utf-8", errors="replace")
    nodes = {}
    for m in re.finditer(r'<node\b[^>]*id="([^"]+)"[^>]*lat="([^"]+)"[^>]*lon="([^"]+)"[^>]*>', text):
        nodes[m.group(1)] = project(float(m.group(2)), float(m.group(3)), lat0, lon0)

    polygons = []
    for m in re.finditer(r'<way\b[^>]*id="([^"]+)"[^>]*>(.*?)</way>', text, re.S):
        way_id, body = m.group(1), m.group(2)
        if not re.search(r'<tag\b[^>]*k="building"', body):
            continue
        refs = re.findall(r'<nd\b[^>]*ref="([^"]+)"[^>]*/?>', body)
        pts = [nodes[r] for r in refs if r in nodes]
        if len(pts) >= 4 and pts[0] == pts[-1]:
            polygons.append((way_id, pts[:-1]))
    return polygons


def point_in_polygon(x: float, y: float, polygon):
    inside = False
    n = len(polygon)
    for i in range(n):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            x_cross = (x2 - x1) * (y - y1) / (y2 - y1) + x1
            if x < x_cross:
                inside = not inside
    return inside


def as_int(row, key):
    return int(float(row.get(key, "0") or 0))


def as_float(row, key):
    try:
        return float(row.get(key, "nan"))
    except ValueError:
        return float("nan")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("grid_csv", type=Path)
    ap.add_argument("osm_file", type=Path)
    ap.add_argument("--origin-lat", type=float, default=22.7569500)
    ap.add_argument("--origin-lon", type=float, default=-102.6002500)
    ap.add_argument("--tx-x", type=float, default=1838.748)
    ap.add_argument("--tx-y", type=float, default=465.0)
    ap.add_argument("--target-distance", type=float, default=240.0,
                    help="Ranking preference only; does not filter candidates.")
    ap.add_argument("--out-csv", type=Path, default=Path("results/Thesis_D00_NLoS_D1_candidates.csv"))
    ap.add_argument("--out-ini", type=Path, default=Path("results/Thesis_D00_selected_candidate.ini"))
    args = ap.parse_args()

    polygons = load_building_polygons(args.osm_file, args.origin_lat, args.origin_lon)
    candidates = []
    with args.grid_csv.open(newline="", encoding="utf-8") as f:
        grid_rows = list(csv.DictReader(f))
    xs_all = [as_float(r, "x_m") for r in grid_rows]
    ys_all = [as_float(r, "y_m") for r in grid_rows]
    min_x, max_x = min(xs_all), max(xs_all)
    min_y, max_y = min(ys_all), max(ys_all)

    for row in grid_rows:
            x = as_float(row, "x_m")
            y = as_float(row, "y_m")
            pr = as_float(row, "received_power_dbm")
            if not math.isfinite(x) or not math.isfinite(y) or not math.isfinite(pr):
                continue
            if as_int(row, "direct_blocked") != 1 or as_int(row, "diffraction_paths") < 1:
                continue
            footprint_hits = [wid for wid, poly in polygons if point_in_polygon(x, y, poly)]
            if footprint_hits:
                continue
            distance = math.hypot(x - args.tx_x, y - args.tx_y)
            r1 = as_int(row, "reflection1_paths")
            r2 = as_int(row, "reflection2_paths")
            d1 = as_int(row, "diffraction_paths")
            clean_penalty = r1 + r2
            edge_margin = min(x - min_x, max_x - x, y - min_y, max_y - y)
            boundary_penalty = 1 if edge_margin < 25.0 else 0
            score = (clean_penalty, boundary_penalty, abs(distance - args.target_distance), -edge_margin, -d1, -pr, x, y)
            candidates.append((score, row, distance))

    candidates.sort(key=lambda item: item[0])
    if not candidates:
        raise SystemExit("No outside-footprint blocked D1 candidate found.")

    args.out_csv.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "rank", "x_m", "y_m", "distance_tx_rx_m", "direct_blocked",
        "los_paths", "reflection1_paths", "reflection2_paths", "diffraction_paths",
        "retained_paths", "received_power_dbm", "rms_delay_spread_ns"
    ]
    with args.out_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for rank, (_, row, distance) in enumerate(candidates, 1):
            w.writerow({
                "rank": rank,
                "x_m": row["x_m"], "y_m": row["y_m"],
                "distance_tx_rx_m": f"{distance:.6f}",
                "direct_blocked": row["direct_blocked"],
                "los_paths": row["los_paths"],
                "reflection1_paths": row["reflection1_paths"],
                "reflection2_paths": row["reflection2_paths"],
                "diffraction_paths": row["diffraction_paths"],
                "retained_paths": row["retained_paths"],
                "received_power_dbm": row["received_power_dbm"],
                "rms_delay_spread_ns": row["rms_delay_spread_ns"],
            })

    # The ranked top candidate is a suggestion. The baked thesis case remains
    # explicitly documented so a future re-ranking does not silently move it.
    _, best, best_distance = candidates[0]
    args.out_ini.parent.mkdir(parents=True, exist_ok=True)
    args.out_ini.write_text(
        "# Generated by select_nlos_d1_candidate.py\n"
        "# Suggested candidate; review before replacing the frozen thesis case.\n"
        f"# distance_tx_rx_m = {best_distance:.6f}\n"
        f"*.tracer.rxX = {best['x_m']}m\n"
        f"*.tracer.rxY = {best['y_m']}m\n",
        encoding="utf-8",
    )

    print(f"Candidates: {len(candidates)}")
    print(f"Ranked CSV: {args.out_csv}")
    print(f"Suggested INI snippet: {args.out_ini}")
    print("Frozen v1.6 thesis D1 case: RX=(1800,225) m")
    print(f"Top ranked suggestion: RX=({best['x_m']},{best['y_m']}) m")


if __name__ == "__main__":
    main()
