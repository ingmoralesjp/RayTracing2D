#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DRJIT_LIBLLVM_PATH=${DRJIT_LIBLLVM_PATH:-/tmp/raytracing-llvm20-20261112/extracted/lib/libLLVM.so.20.1}
DRJIT_CACHE_DIR="$ROOT/runtime/drjit-cache"
MPLCONFIGDIR="$ROOT/runtime/matplotlib-sionna"
SIONNA_PYTHON=${SIONNA_PYTHON:-/tmp/raytracing-sionna-20261112/bin/python}
export DRJIT_LIBLLVM_PATH DRJIT_CACHE_DIR MPLCONFIGDIR
# This host has GCC 9; use the isolated conda-forge C++/LLVM runtime in this process.
if [ -d /tmp/raytracing-llvm20-20261112/extracted/lib ]; then
    LD_LIBRARY_PATH=/tmp/raytracing-llvm20-20261112/extracted/lib:${LD_LIBRARY_PATH:-}
    LD_PRELOAD=/tmp/raytracing-llvm20-20261112/extracted/lib/libstdc++.so.6:/tmp/raytracing-llvm20-20261112/extracted/lib/libgcc_s.so.1
    export LD_LIBRARY_PATH LD_PRELOAD
fi
exec "$SIONNA_PYTHON" "$ROOT/scripts/run_sionna_diagnostic.py" "$@"
