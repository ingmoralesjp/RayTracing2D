#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
omnetpp_dir=${OMNETPP_ROOT:-$(CDPATH= cd -- "$project_dir/../.." && pwd)}
PATH="$omnetpp_dir/bin:$PATH"
LD_LIBRARY_PATH="$omnetpp_dir/lib:${LD_LIBRARY_PATH:-}"
export PATH LD_LIBRARY_PATH

printf '%s\n' "RayTracing2D environment check"
for tool in opp_makemake opp_run; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '  %-15s %s\n' "$tool" "OK ($(command -v "$tool"))"
    else
        printf '  %-15s %s\n' "$tool" "NOT FOUND"
    fi
done

if command -v g++ >/dev/null 2>&1; then
    printf '  %-15s %s\n' "g++" "OK ($(g++ --version | head -n 1))"
else
    printf '  %-15s %s\n' "g++" "NOT FOUND"
fi
