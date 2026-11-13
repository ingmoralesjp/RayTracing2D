#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${OMNETPP_ROOT:-}" ]; then
    PATH="$OMNETPP_ROOT/bin:$PATH"
    LD_LIBRARY_PATH="$OMNETPP_ROOT/lib:${LD_LIBRARY_PATH:-}"
    export PATH LD_LIBRARY_PATH
fi
cd "$project_dir/simulations"

# Incremental urban mechanisms. Same geometry/grid; enable one physical family
# at a time so differences are attributable to the added mechanism.
for cfg in Urban01_FSPL Urban02_Blockage Urban03_R1 Urban04_R1R2 Urban05_Full; do
    echo "============================================================"
    echo "Running urban v1.1 experiment: $cfg"
    echo "============================================================"
    ../src/raytracing2d -u Cmdenv -n ../src:. -f omnetpp.ini -c "$cfg"
done

python3 ../scripts/summarize_urban.py
