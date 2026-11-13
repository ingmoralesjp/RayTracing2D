#!/bin/sh
set -eu
GROUP=${1:-help}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/src/raytracing2d"
INI="$ROOT/simulations/omnetpp.ini"

run_cfg() {
  cfg=$1
  echo "=== $cfg ==="
  (cd "$ROOT/simulations" && "$BIN" -u Cmdenv -n ../src:. -f "$INI" -c "$cfg")
}

case "$GROUP" in
  regression)
    for c in Thesis_T00_LoS Thesis_T01_LoS_R1 Thesis_T02_LoS_R1R2; do run_cfg "$c"; done ;;
  diffraction)
    run_cfg Thesis_D00_Find_NLoS_D1
    (cd "$ROOT" && python3 scripts/select_nlos_d1_candidate.py \
      simulations/results/Thesis_D00_Find_NLoS_D1-grid.csv \
      simulations/data/student/zacatecas_student.osm \
      --out-csv simulations/results/Thesis_D00_NLoS_D1_candidates.csv \
      --out-ini simulations/results/Thesis_D00_selected_candidate.ini)
    for c in Thesis_D01_NLoS_Blockage Thesis_D02_NLoS_R1 Thesis_D03_NLoS_R1R2 Thesis_D04_NLoS_FULL; do run_cfg "$c"; done ;;
  ablation)
    for c in Thesis_A10_LoS_25m Thesis_A11_LoS_R1_25m Thesis_A12_FULL_25m; do run_cfg "$c"; done ;;
  maps)
    for c in Thesis_M50_FULL_50m Thesis_M25_FULL_25m Thesis_M10_FULL_10m; do run_cfg "$c"; done
    (cd "$ROOT" && python3 scripts/plot_thesis_maps.py simulations/results/Thesis_M10_FULL_10m-grid.csv --out-dir simulations/results/thesis_maps) ;;
  sensitivity)
    for c in \
      Thesis_P01_TE_Coherent Thesis_P02_TM_Coherent Thesis_P03_TE_Incoherent \
      Thesis_F01_2400MHz Thesis_F02_916MHz \
      Thesis_G01_Angle90 Thesis_G02_Angle75 Thesis_G03_Angle60 \
      Thesis_S_R1_Original Thesis_S_R1_Concrete Thesis_S_R1_Glass \
      Thesis_S_R2_Original Thesis_S_R2_Concrete Thesis_S_R2_Glass \
      Thesis_S_D1_Original Thesis_S_D1_Concrete Thesis_S_D1_Glass; do run_cfg "$c"; done ;;
  help|*)
    echo "Usage: $0 {regression|diffraction|ablation|maps|sensitivity}"
    echo "Run groups deliberately; there is no automatic massive 'all' campaign." ;;
esac
