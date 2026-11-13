#!/usr/bin/env python3
"""Validate the optional INET IPathLoss bridge against the core FSPL contract."""
from __future__ import annotations

import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCA = ROOT / "inet_adapter" / "results" / "General-#0.sca"
text = SCA.read_text()

def scalar(name: str) -> float:
    match = re.search(rf"^scalar\s+\S+\s+{re.escape(name)}\s+(\S+)", text, re.MULTILINE)
    if not match:
        raise AssertionError(f"Missing INET adapter scalar: {name}")
    return float(match.group(1))

loss_factor = scalar("lossFactor1km")
path_loss_db = scalar("pathLossDb1km")
c0, frequency, distance = 299792458.0, 916e6, 1000.0
expected_factor = (c0 / frequency / (4.0 * math.pi * distance)) ** 2
expected_db = -10.0 * math.log10(expected_factor)
assert math.isclose(loss_factor, expected_factor, rel_tol=1e-12)
assert math.isclose(path_loss_db, expected_db, rel_tol=1e-12)
assert math.isclose(path_loss_db, 91.68569269524, abs_tol=1e-10)
print("RayTracing2D v1.5 optional INET adapter: PASS")
print(f"FSPL(1 km, 916 MHz) = {path_loss_db:.12f} dB")
