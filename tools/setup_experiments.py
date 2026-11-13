"""Populate the archived scripts' local source paths from the shared framework."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]

for name in ["campaign", "diagnostic"]:
    destination = ROOT / "experiments" / name / "source"
    if destination.exists():
        raise SystemExit(f"Existing source folder preserved: {destination}. Remove it explicitly before refreshing.")
    shutil.copytree(
        ROOT / "framework", destination,
        ignore=shutil.ignore_patterns("build-tests", "out", "results", "__pycache__", "raytracing2d"),
    )
    print(f"Prepared {destination.relative_to(ROOT)}")
