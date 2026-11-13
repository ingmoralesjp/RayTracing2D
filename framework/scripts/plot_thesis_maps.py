#!/usr/bin/env python3
"""Render the three final thesis maps from ONE M10 grid CSV.

This script does not rerun physics. It reads the same receiver samples and makes:
  - coherent received power Pr(x,y)
  - retained path count Npaths(x,y)
  - RMS delay spread tau_RMS(x,y)
NaN/no-path cells are intentionally omitted rather than interpreted as weak power.
"""
from __future__ import annotations
import argparse, csv, math
from pathlib import Path


def load(path):
    rows=[]
    with open(path,newline="",encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows.append(r)
    return rows


def finite(v):
    try:
        return math.isfinite(float(v))
    except Exception:
        return False


def render(rows, column, label, title, output, tx_x, tx_y):
    import matplotlib.pyplot as plt
    xs=[]; ys=[]; values=[]
    for r in rows:
        if finite(r.get(column,"nan")):
            xs.append(float(r["x_m"])); ys.append(float(r["y_m"])); values.append(float(r[column]))
    fig, ax = plt.subplots(figsize=(7.2,7.2))
    sc = ax.scatter(xs, ys, c=values, marker="s", s=18)
    ax.scatter([tx_x], [tx_y], marker="^", s=95, label="TX")
    ax.legend(loc="best")
    ax.set_xlabel("x (m)")
    ax.set_ylabel("y (m)")
    ax.set_title(title)
    ax.set_aspect("equal", adjustable="box")
    cb = fig.colorbar(sc, ax=ax)
    cb.set_label(label)
    fig.tight_layout()
    fig.savefig(output, dpi=220)
    plt.close(fig)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("grid_csv", type=Path)
    ap.add_argument("--out-dir", type=Path, default=Path("results/thesis_maps"))
    ap.add_argument("--tx-x", type=float, default=1838.748)
    ap.add_argument("--tx-y", type=float, default=465.0)
    args=ap.parse_args()
    rows=load(args.grid_csv)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    render(rows,"received_power_dbm",r"$P_r$ (dBm)","RayTracing2D FULL - potencia recibida coherente",
           args.out_dir/"Thesis_M10_received_power.png", args.tx_x, args.tx_y)
    render(rows,"retained_paths","Caminos retenidos","RayTracing2D FULL - numero de caminos",
           args.out_dir/"Thesis_M10_path_count.png", args.tx_x, args.tx_y)
    render(rows,"rms_delay_spread_ns",r"RMS delay spread (ns)","RayTracing2D FULL - dispersion temporal",
           args.out_dir/"Thesis_M10_rms_delay.png", args.tx_x, args.tx_y)
    print(args.out_dir)

if __name__=="__main__": main()
