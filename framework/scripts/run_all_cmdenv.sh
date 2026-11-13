#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${OMNETPP_ROOT:-}" ]; then
    PATH="$OMNETPP_ROOT/bin:$PATH"
    LD_LIBRARY_PATH="$OMNETPP_ROOT/lib:${LD_LIBRARY_PATH:-}"
    export PATH LD_LIBRARY_PATH
fi
cd "$project_dir/simulations"

for cfg in LoS SingleReflection BlockedDirect MultipleWalls SecondOrderParallelWalls FresnelConcreteTE FresnelConcreteTM KnifeEdgeBlocked OsmBuilding CoverageGrid; do
    echo "============================================================"
    echo "Running regression: $cfg"
    echo "============================================================"
    ../src/raytracing2d -u Cmdenv -n ../src:. -f omnetpp.ini -c "$cfg"
done
