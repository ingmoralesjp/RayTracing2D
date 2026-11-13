#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${OMNETPP_ROOT:-}" ]; then
    PATH="$OMNETPP_ROOT/bin:$PATH"
    LD_LIBRARY_PATH="$OMNETPP_ROOT/lib:${LD_LIBRARY_PATH:-}"
    export PATH LD_LIBRARY_PATH
fi

cd "$project_dir/simulations"
configs=$(sed -n 's/^\[Config \([^]]*\)\]/\1/p' omnetpp.ini)
for cfg in $configs; do
    if [ "$cfg" = "UrbanStudentMapImportCheck" ]; then
        if [ ! -f data/student/zacatecas_student.osm ]; then
            echo "SKIP $cfg: missing local data/student/zacatecas_student.osm"
            continue
        fi
    fi
    echo "============================================================"
    echo "Running configuration: $cfg"
    echo "============================================================"
    ../src/raytracing2d -u Cmdenv -n ../src:. -f omnetpp.ini -c "$cfg"
done

python3 ../scripts/summarize_urban.py
python3 ../scripts/summarize_all_configs.py
