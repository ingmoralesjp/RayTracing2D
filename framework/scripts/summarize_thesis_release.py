#!/usr/bin/env python3
"""Collect key OMNeT++ scalars for the v1.6 thesis-release campaign."""
from __future__ import annotations
import argparse, csv, re
from pathlib import Path

SCALAR_RE = re.compile(r'^scalar\s+\S+\s+(\S+)\s+([^\s]+)')


def parse_sca(path):
    out={}
    with open(path,encoding="utf-8",errors="replace") as f:
        for line in f:
            m=SCALAR_RE.match(line.strip())
            if not m: continue
            try: out[m.group(1)] = float(m.group(2))
            except ValueError: pass
    return out


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--results",type=Path,default=Path("simulations/results"))
    ap.add_argument("--out",type=Path,default=Path("simulations/results/Thesis_v1.6_summary.csv"))
    args=ap.parse_args()
    patterns=[
      "Thesis_T*0.sca","Thesis_D*0.sca","Thesis_A*0.sca","Thesis_M*0.sca",
      "Thesis_P*0.sca","Thesis_F*0.sca","Thesis_G*0.sca","Thesis_S*0.sca"
    ]
    files=[]
    for pat in patterns: files.extend(args.results.glob(pat))
    files=sorted(set(files))
    fields=["config","directBlocked","numPaths","firstOrderValid","secondOrderValid","diffractionValid",
            "totalReceivedPowerDbm","totalIncoherentPowerDbm","equivalentPathLossDb","rmsDelaySpreadS",
            "gridSampleCount","gridFiniteSamples","gridNoPathSamples","gridTotalRetainedPaths","gridEvaluationSeconds",
            "gridMaterialOutOfRangePaths"]
    args.out.parent.mkdir(parents=True,exist_ok=True)
    with args.out.open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for p in files:
            s=parse_sca(p)
            name=p.name.rsplit("-",1)[0]
            row={"config":name}
            for k in fields[1:]: row[k]=s.get(k,"")
            w.writerow(row)
    print(f"Wrote {len(files)} configs to {args.out}")

if __name__=="__main__": main()
