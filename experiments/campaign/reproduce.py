"""Replay archived configurations without overwriting the supplied evidence."""
from pathlib import Path
import argparse
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", choices=["native", "common", "subareas", "scalability"])
    parser.add_argument("--omnetpp-root", default=os.environ.get("OMNETPP_ROOT"))
    parser.add_argument("--compiler", default=os.environ.get("CXX", "clang++"))
    args = parser.parse_args()
    output = ROOT / "reproduced" / args.campaign
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    if args.omnetpp_root:
        omnet = Path(args.omnetpp_root).resolve()
        env["PATH"] = str(omnet / "bin") + os.pathsep + env["PATH"]
        env["LD_LIBRARY_PATH"] = str(omnet / "lib") + os.pathsep + env.get("LD_LIBRARY_PATH", "")
        env["OMNETPP_ROOT"] = str(omnet)
    source = ROOT / "source"
    if args.campaign == "native":
        subprocess.run(["make", "-j2"], cwd=source, env=env, check=True)
        for config in json.loads((ROOT / "native_configurations.json").read_text()):
            command = [str(source / "src/raytracing2d"), "-u", "Cmdenv", "-n", "../src:.",
                       "-f", str(ROOT / "reviewer.ini"), "-c", config, "-r", "0",
                       "--result-dir=" + str(output)]
            with (output / (config + ".log")).open("w") as log:
                subprocess.run(command, cwd=source / "simulations", env=env,
                               stdout=log, stderr=subprocess.STDOUT, check=True)
        return
    probe = output / "rtprobe"
    command = [args.compiler, "-std=c++17", "-O3", "-DNDEBUG=1", "-I" + str(source / "src"),
               str(ROOT / "scripts/rtprobe.cpp")]
    command.extend(str(source / "src" / name) for name in
                   ["Geometry2D.cc", "RayTracer2D.cc", "Propagation2D.cc", "Materials2D.cc", "OsmImporter2D.cc"])
    subprocess.run(command + ["-o", str(probe)], check=True)
    walls = ROOT / "09_reproducibility/walls_exact.csv"
    cases = []
    if args.campaign == "common":
        cases = [("coarse_grid", walls, ROOT / "09_reproducibility/m50_receivers.csv", 1, 15),
                 ("external_reflections", walls, ROOT / "04_E5_Sionna/common_receivers.csv", 1, 7)]
    elif args.campaign == "subareas":
        cases = [(rx.stem, walls, rx, 1, 15) for rx in sorted((ROOT / "06_E7_scenarios").glob("S*_receivers.csv"))]
    else:
        bench = ROOT / "07_E8_scalability"
        for scene in sorted((bench / "scenes").glob("*.csv")):
            cases.append((scene.stem, scene, bench / (scene.stem.split("_")[0] + "_receivers.csv"), 6, 15))
        for count in [1, 16, 64, 256]:
            cases.append(("receivers_" + str(count), walls, bench / ("receivers_" + str(count) + ".csv"), 4, 15))
    for name, scene, receivers, repeats, mechanisms in cases:
        subprocess.run([str(probe), "eval", str(scene), str(receivers), str(output / (name + ".csv")),
                        str(repeats), str(mechanisms), "500"], check=True)


if __name__ == "__main__":
    main()
