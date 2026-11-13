#!/usr/bin/env python3
"""OSM preflight for RayTracing2D v1.5.1.

The script distinguishes three geographic extents that are easy to confuse in an
OSM extract:
  1) the declared <bounds> extraction box,
  2) all <node> elements stored in the XML (which may include dependency nodes
     well outside the extraction box), and
  3) nodes actually referenced by supported closed building ways.

For RayTracing2D's building-only geometry, the recommended projection origin is
the south-west corner of the declared <bounds> when present; otherwise it falls
back to the south-west corner of supported building geometry. The script does
not modify the input file.
"""
from __future__ import annotations

import argparse
import collections
import math
import xml.etree.ElementTree as ET
from pathlib import Path

EARTH_RADIUS_M = 6371008.8


def project(lat_deg: float, lon_deg: float, origin_lat_deg: float, origin_lon_deg: float):
    lat0 = math.radians(origin_lat_deg)
    x = EARTH_RADIUS_M * math.cos(lat0) * math.radians(lon_deg - origin_lon_deg)
    y = EARTH_RADIUS_M * math.radians(lat_deg - origin_lat_deg)
    return x, y


def fmt_range(values):
    return min(values), max(values)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("osm_file", type=Path)
    args = ap.parse_args()

    root = ET.parse(args.osm_file).getroot()
    nodes = {}
    for node in root.findall("node"):
        nodes[node.attrib["id"]] = (float(node.attrib["lat"]), float(node.attrib["lon"]))
    if not nodes:
        raise SystemExit("No OSM nodes found")

    all_lats = [v[0] for v in nodes.values()]
    all_lons = [v[1] for v in nodes.values()]
    materials = collections.Counter()
    building_ways = 0
    closed_building_ways = 0
    multipolygon_relations = 0
    building_lats = []
    building_lons = []

    for way in root.findall("way"):
        tags = {t.attrib.get("k", ""): t.attrib.get("v", "") for t in way.findall("tag")}
        if tags.get("building") and tags.get("building") != "no":
            building_ways += 1
            refs = [n.attrib.get("ref", "") for n in way.findall("nd")]
            if len(refs) >= 4 and refs[0] == refs[-1]:
                closed_building_ways += 1
                for ref in refs:
                    if ref in nodes:
                        lat, lon = nodes[ref]
                        building_lats.append(lat)
                        building_lons.append(lon)
            materials[tags.get("building:material", "<sin etiqueta>")] += 1

    for rel in root.findall("relation"):
        tags = {t.attrib.get("k", ""): t.attrib.get("v", "") for t in rel.findall("tag")}
        if tags.get("type") == "multipolygon" and tags.get("building"):
            multipolygon_relations += 1

    all_min_lat, all_max_lat = fmt_range(all_lats)
    all_min_lon, all_max_lon = fmt_range(all_lons)
    b_min_lat, b_max_lat = fmt_range(building_lats) if building_lats else (all_min_lat, all_max_lat)
    b_min_lon, b_max_lon = fmt_range(building_lons) if building_lons else (all_min_lon, all_max_lon)

    bounds = root.find("bounds")
    if bounds is not None:
        declared = (
            float(bounds.attrib["minlat"]), float(bounds.attrib["minlon"]),
            float(bounds.attrib["maxlat"]), float(bounds.attrib["maxlon"]),
        )
        origin_lat, origin_lon = declared[0], declared[1]
        origin_source = "declared <bounds> south-west corner"
    else:
        declared = None
        origin_lat, origin_lon = b_min_lat, b_min_lon
        origin_source = "supported-building south-west corner (no <bounds> present)"

    # Report the local extent of the geometry that RayTracing2D actually imports.
    projected_building_points = [project(lat, lon, origin_lat, origin_lon)
                                 for lat, lon in zip(building_lats, building_lons)]
    if projected_building_points:
        xs = [p[0] for p in projected_building_points]
        ys = [p[1] for p in projected_building_points]
        x_min, x_max = min(xs), max(xs)
        y_min, y_max = min(ys), max(ys)
    else:
        x_min = x_max = y_min = y_max = 0.0

    print("RayTracing2D v1.5.1 - OSM preflight")
    print(f"file: {args.osm_file}")
    print(f"nodes stored in XML: {len(nodes)}")
    if declared:
        print("declared <bounds>:")
        print(f"  latitude:  {declared[0]:.9f} .. {declared[2]:.9f}")
        print(f"  longitude: {declared[1]:.9f} .. {declared[3]:.9f}")
    print("all-node extent (may include dependency nodes outside the map):")
    print(f"  latitude:  {all_min_lat:.9f} .. {all_max_lat:.9f}")
    print(f"  longitude: {all_min_lon:.9f} .. {all_max_lon:.9f}")
    print("supported-building extent:")
    print(f"  latitude:  {b_min_lat:.9f} .. {b_max_lat:.9f}")
    print(f"  longitude: {b_min_lon:.9f} .. {b_max_lon:.9f}")
    print(f"recommended origin source: {origin_source}")
    print(f"osmOriginLatDeg = {origin_lat:.9f}")
    print(f"osmOriginLonDeg = {origin_lon:.9f}")
    print(f"imported-building local extent: X={x_min:.3f} .. {x_max:.3f} m, Y={y_min:.3f} .. {y_max:.3f} m")
    print("local axis contract: +X=east, +Y=north; Qtenv rendering is transformed separately")
    print(f"building ways: {building_ways}")
    print(f"closed building ways supported: {closed_building_ways}")
    print(f"building multipolygon relations NOT supported: {multipolygon_relations}")
    print("materials:")
    for material, count in materials.most_common():
        print(f"  {material}: {count}")

    if closed_building_ways != building_ways:
        print("WARNING: some building ways are not closed and will be rejected by the current importer.")
    if multipolygon_relations:
        print("WARNING: multipolygon building relations require a future importer extension.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
