#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
omnetpp_dir=${OMNETPP_ROOT:-$(CDPATH= cd -- "$project_dir/../.." && pwd)}
inet_dir=${INET_ROOT:-$project_dir/../inet}
adapter_dir=$project_dir/inet_adapter

cd "$adapter_dir"
PATH="$omnetpp_dir/bin:$PATH" "$omnetpp_dir/bin/opp_makemake" -f --deep --make-so \
    -o raytracing2d_inet -I../src -I"$inet_dir/src" -L"$inet_dir/src" -lINET -KCFLAGS_EXTRA=-std=c++17
PATH="$omnetpp_dir/bin:$PATH" LD_LIBRARY_PATH="$omnetpp_dir/lib:$inet_dir/src:${LD_LIBRARY_PATH:-}" make -j2
PATH="$omnetpp_dir/bin:$PATH" LD_LIBRARY_PATH="$omnetpp_dir/lib:$inet_dir/src:${LD_LIBRARY_PATH:-}" \
    "$omnetpp_dir/bin/opp_run" -u Cmdenv -n ".:$inet_dir/src" \
    -l "$inet_dir/src/INET" -l ./raytracing2d_inet \
    -f omnetpp.ini
