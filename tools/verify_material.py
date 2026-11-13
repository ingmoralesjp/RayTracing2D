"""Read-only checks for the archived source, data and main spatial results."""
from pathlib import Path
import csv
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]

manifest = json.loads((ROOT / "material-sha256.json").read_text())
for name, expected in manifest.items():
    path = ROOT / name
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise SystemExit(f"Material hash mismatch: {name}")

for step, expected in [(50, (77, 57, 20, 41, 122)),
                       (25, (273, 192, 81, 151, 460)),
                       (10, (1581, 1106, 475, 877, 2811))]:
    path = ROOT / "experiments/data/campaign" / f"Thesis_M{step:02}_FULL_{step}m-grid.csv"
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    exterior = [row for row in rows if row["rx_domain"] == "exterior"]
    finite = [row for row in exterior if int(row["retained_paths"]) > 0]
    actual = (len(rows), len(exterior), len(rows) - len(exterior), len(finite),
              sum(int(row["retained_paths"]) for row in finite))
    if actual != expected:
        raise SystemExit(f"Unexpected {step} m grid: {actual}")
    print(f"Verified {step} m grid: {actual}")

print(f"Verified {len(manifest)} archived file hashes")
