#!/usr/bin/env python3
"""Run eight bounded OMNeT++ checks against the archived paper artifacts."""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import subprocess
import time


CONFIGS = [
    "Thesis_T00_LoS", "Thesis_T01_LoS_R1", "Thesis_T02_LoS_R1R2",
    "Thesis_D01_NLoS_Blockage", "Thesis_D02_NLoS_R1",
    "Thesis_D03_NLoS_R1R2", "Thesis_D04_NLoS_FULL",
    "Thesis_M50_FULL_50m",
]
PATH_CONFIGS = ["Thesis_T02_LoS_R1R2", "Thesis_D04_NLoS_FULL"]
COUNTS = {
    "directBlocked", "candidatePaths", "numPaths", "firstOrderTheoretical",
    "firstOrderCandidates", "firstOrderPruned", "firstOrderValid",
    "secondOrderTheoretical", "secondOrderCandidates", "secondOrderPruned",
    "secondOrderValid", "diffractionCandidates", "diffractionValid",
    "gridSampleCount", "gridFiniteSamples", "gridNoPathSamples",
    "gridTotalCandidatePaths", "gridTotalRetainedPaths",
    "gridMaterialEvaluatedPaths", "gridMaterialOutOfRangePaths",
    "gridCellsWithMaterialExtrapolation", "reflection_order",
    "diffraction_order", "material_model_in_range",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def tolerance(name):
    if name in COUNTS:
        return 0.0
    if name.endswith("S") or name == "delay_s":
        return 1e-13
    if name == "distance_m":
        return 1e-8
    if "phase" in name or name in {"diffraction_v", "reflection_magnitude"}:
        return 1e-9
    return 1e-8


def equal_number(name, actual, reference):
    a, b = float(actual), float(reference)
    if math.isnan(a) or math.isnan(b):
        return math.isnan(a) and math.isnan(b)
    if not math.isfinite(a) or not math.isfinite(b):
        return a == b
    if name in COUNTS:
        return a == b
    return math.isclose(a, b, rel_tol=1e-10, abs_tol=tolerance(name))


def read_scalars(path):
    scalars = {}
    for line in path.read_text().splitlines():
        match = re.match(r"^scalar\s+\S+\s+(\S+)\s+(\S+)", line)
        if match:
            scalars[match[1]] = match[2]
    return scalars


def main():
    project = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--paper", type=Path, default=project.parent / "paper")
    parser.add_argument("--out", type=Path, required=True,
                        help="New evidence directory; existing output is never overwritten")
    parser.add_argument("--omnetpp", type=Path,
                        default=Path("/home/juan/omnetpp-6.0pre11"))
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    results = out / "results"
    results.mkdir()
    supplement = args.paper.resolve() / "supplementary"
    report = {"scope": "Eight configurations, including a 77-receiver grid; not the full campaign",
              "tolerances": {"integer_counts": "exact", "power_and_loss_db": 1e-8,
                             "delay_s": 1e-13, "distance_m": 1e-8,
                             "phase_rad": 1e-9, "relative": 1e-10},
              "input_checks": [], "runs": [], "failures": []}
    failures = report["failures"]

    for local, archived in [
        (project / "simulations/omnetpp.ini", supplement / "omnetpp_modelo.ini"),
        (project / "simulations/data/student/zacatecas_student.osm",
         supplement / "zacatecas_osm_utilizado.osm"),
    ]:
        same = local.read_bytes() == archived.read_bytes()
        report["input_checks"].append({"file": str(local.relative_to(project)),
                                       "sha256": sha(local.read_bytes()), "identical": same})
        if not same:
            failures.append("Input differs from paper: " + str(local))

    for line in (supplement / "SHA256SUMS.txt").read_text().splitlines():
        match = re.match(r"^([0-9a-fA-F]{64})\s+\*?(.+)$", line.strip())
        if match:
            archived = supplement / match[2]
            if sha(archived.read_bytes()) != match[1].lower():
                failures.append("Archived checksum mismatch: " + match[2])
    if failures:
        (out / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
        raise SystemExit("Input verification failed")

    references = {row["config"]: row for row in csv.DictReader(
        io.StringIO((supplement / "Thesis_v1.6_summary.csv").read_text()))}
    env = os.environ.copy()
    env["PATH"] = str(args.omnetpp / "bin") + os.pathsep + env["PATH"]
    env["LD_LIBRARY_PATH"] = str(args.omnetpp / "lib") + os.pathsep + env.get("LD_LIBRARY_PATH", "")

    for config in CONFIGS:
        command = [str(project / "src/raytracing2d"), "-u", "Cmdenv", "-n", "../src:.",
                   "-f", "omnetpp.ini", "-c", config, "-r", "0",
                   "--result-dir=" + str(results)]
        started = time.monotonic()
        with (out / (config + ".log")).open("w") as log:
            process = subprocess.run(command, cwd=project / "simulations", env=env,
                                     stdout=log, stderr=subprocess.STDOUT, timeout=90)
        run = {"config": config, "command": command, "returncode": process.returncode,
               "elapsed_seconds": time.monotonic() - started, "compared_fields": 0,
               "failures": [], "scalars": {}}
        report["runs"].append(run)
        scalar_path = results / (config + "-0.sca")
        if process.returncode or not scalar_path.is_file():
            run["failures"].append("Simulation failed or scalar output missing")
        else:
            actual = read_scalars(scalar_path)
            run["scalars"] = actual
            for name, reference in references[config].items():
                if name in {"config", "gridEvaluationSeconds"} or not reference:
                    continue
                run["compared_fields"] += 1
                if name not in actual or not equal_number(name, actual[name], reference):
                    run["failures"].append({"metric": name, "archived": reference,
                                             "fresh": actual.get(name)})

        if config in PATH_CONFIGS and not process.returncode:
            name = config + "-rays.csv"
            archived_bytes, fresh_bytes = (supplement / name).read_bytes(), (results / name).read_bytes()
            reference_rows = list(csv.DictReader(io.StringIO(archived_bytes.decode())))
            actual_rows = list(csv.DictReader(io.StringIO(fresh_bytes.decode())))
            run["paths_csv_identical_bytes"] = archived_bytes == fresh_bytes
            run["paths_csv_sha256"] = sha(fresh_bytes)
            run["paths_csv_rows"] = len(actual_rows)
            by_id = {row["path_id"]: row for row in actual_rows}
            if len(by_id) != len(actual_rows) or set(by_id) != {row["path_id"] for row in reference_rows}:
                run["failures"].append("Retained path identities differ")
            else:
                for reference in reference_rows:
                    actual = by_id[reference["path_id"]]
                    if actual.keys() != reference.keys():
                        run["failures"].append("Path CSV columns differ")
                        continue
                    for key, value in reference.items():
                        try:
                            matches = equal_number(key, actual[key], value)
                        except ValueError:
                            matches = actual[key] == value
                        if not matches:
                            run["failures"].append({"path": reference["path_id"], "field": key,
                                                    "archived": value, "fresh": actual[key]})

        failures.extend({"config": config, "detail": issue} for issue in run["failures"])
        run["passed"] = not run["failures"]
        print(config, "PASS" if run["passed"] else "FAIL",
              "fields=" + str(run["compared_fields"]), flush=True)

    report["passed"] = not failures
    (out / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    lines = ["# Quick comparison with archived paper outputs", "",
             "Status: " + ("PASS" if report["passed"] else "FAIL"), "",
             report["scope"] + ". Runtime is excluded from physical equivalence.", "",
             "| Configuration | Scalar fields | Result |", "| --- | ---: | --- |"]
    for run in report["runs"]:
        lines.append(f"| {run['config']} | {run['compared_fields']} | {'PASS' if run['passed'] else 'FAIL'} |")
    lines += ["", "T02 and D04 also compare every retained path CSV field and path identity.",
              "The M50 grid is checked against its archived scalar summary; its historical raw grid CSV is absent.",
              "This does not rerun the complete 31-configuration campaign or validate the model against field measurements."]
    (out / "COMPARISON.md").write_text("\n".join(lines) + "\n")
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
