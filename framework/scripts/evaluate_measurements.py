#!/usr/bin/env python3
"""Match measured receiver points to a RayTracing2D grid and compute errors.

Residual convention: prediction - measurement. Positive bias means that the
model over-predicts received power. The script uses only the Python standard
library so it remains reproducible with the OMNeT++ project environment.
"""
from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path


def percentile(values: list[float], q: float) -> float:
    data = sorted(values)
    position = (len(data) - 1) * q
    low, high = math.floor(position), math.ceil(position)
    if low == high:
        return data[low]
    return data[low] * (high - position) + data[high] * (position - low)


def metrics(label: str, rows: list[dict[str, object]]) -> dict[str, object]:
    residuals = [float(row["residual_db"]) for row in rows]
    uncertainties = [float(row["uncertainty_db"]) for row in rows
                     if math.isfinite(float(row["uncertainty_db"]))]
    bias = sum(residuals) / len(residuals)
    return {
        "set": label,
        "n": len(rows),
        "mae_db": sum(abs(value) for value in residuals) / len(residuals),
        "rmse_db": math.sqrt(sum(value * value for value in residuals) / len(residuals)),
        "bias_db": bias,
        "residual_std_db": math.sqrt(sum((value - bias) ** 2 for value in residuals) /
                                     len(residuals)),
        "p10_residual_db": percentile(residuals, 0.1),
        "p50_residual_db": percentile(residuals, 0.5),
        "p90_residual_db": percentile(residuals, 0.9),
        "mean_uncertainty_db": (sum(uncertainties) / len(uncertainties)
                                if uncertainties else math.nan),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("predictions", type=Path, help="RayTracing2D grid CSV")
    parser.add_argument("measurements", type=Path,
                        help="CSV: id,x_m,y_m,measured_pr_dbm,uncertainty_db,set")
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--pairs", type=Path, required=True)
    parser.add_argument("--max-match-distance-m", type=float, default=0.01)
    args = parser.parse_args()
    if args.max_match_distance_m < 0.0:
        raise SystemExit("--max-match-distance-m must be non-negative")

    with args.predictions.open(newline="") as stream:
        predictions = list(csv.DictReader(stream))
    finite_predictions = []
    for row in predictions:
        power = float(row["received_power_dbm"])
        if math.isfinite(power):
            finite_predictions.append((float(row["x_m"]), float(row["y_m"]), power))
    if not finite_predictions:
        raise SystemExit("Prediction grid contains no finite received-power values")

    with args.measurements.open(newline="") as stream:
        measurements = list(csv.DictReader(stream))
    required = {"id", "x_m", "y_m", "measured_pr_dbm", "uncertainty_db", "set"}
    if not measurements or not required.issubset(measurements[0]):
        raise SystemExit("Measurement CSV is empty or missing required columns")
    identifiers = [row["id"] for row in measurements]
    if len(identifiers) != len(set(identifiers)):
        raise SystemExit("Measurement IDs must be unique")

    matched: list[dict[str, object]] = []
    for measurement in measurements:
        x, y = float(measurement["x_m"]), float(measurement["y_m"])
        measured = float(measurement["measured_pr_dbm"])
        uncertainty = float(measurement["uncertainty_db"])
        if not all(math.isfinite(value) for value in (x, y, measured, uncertainty)):
            raise SystemExit(f"Non-finite measurement field for {measurement['id']}")
        if uncertainty < 0.0:
            raise SystemExit(f"Negative uncertainty for {measurement['id']}")
        px, py, predicted = min(
            finite_predictions,
            key=lambda item: (item[0] - x) ** 2 + (item[1] - y) ** 2,
        )
        match_distance = math.hypot(px - x, py - y)
        if match_distance > args.max_match_distance_m:
            raise SystemExit(
                f"No prediction within {args.max_match_distance_m} m for {measurement['id']}; "
                f"nearest is {match_distance:.6g} m"
            )
        matched.append({
            "id": measurement["id"], "set": measurement["set"],
            "measurement_x_m": x, "measurement_y_m": y,
            "prediction_x_m": px, "prediction_y_m": py,
            "match_distance_m": match_distance,
            "measured_pr_dbm": measured, "predicted_pr_dbm": predicted,
            "residual_db": predicted - measured,
            "uncertainty_db": uncertainty,
        })

    args.pairs.parent.mkdir(parents=True, exist_ok=True)
    with args.pairs.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(matched[0]))
        writer.writeheader()
        writer.writerows(matched)

    labels = sorted({str(row["set"]) for row in matched})
    summaries = [metrics("all", matched)]
    summaries.extend(metrics(label, [row for row in matched if row["set"] == label])
                     for label in labels)
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    with args.summary.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summaries[0]))
        writer.writeheader()
        writer.writerows(summaries)
    for row in summaries:
        print(f"{row['set']}: n={row['n']} MAE={row['mae_db']:.4f} dB "
              f"RMSE={row['rmse_db']:.4f} dB bias={row['bias_db']:.4f} dB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
