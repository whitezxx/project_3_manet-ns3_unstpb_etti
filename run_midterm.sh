#!/usr/bin/env bash
# run_midterm.sh -- one command for the midterm: build, run the baseline, analyze, plot.
#
#   NS3_DIR=~/project_dc/ns-3-dev ./run_midterm.sh
#
# Baseline (guidelines, P2): 50 nodes, static, AODV/OLSR/DSDV, 1 seed.
# Change SEEDS, NODES or SPEED from the command line, e.g.  SEEDS="1 2 3" ./run_midterm.sh
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/project_dc/ns-3-dev}"
SEEDS="${SEEDS:-1}"
NODES="${NODES:-50}"
SPEED="${SPEED:-0}"
RESULTS="$REPO/results/midterm"
PLOTS="$REPO/plots/midterm"

# 1) make sure ns-3 sees our program: scratch/manet-compare.cc -> repo file (symbolic link)
if [ ! -e "$NS3_DIR/scratch/manet-compare.cc" ]; then
  ln -s "$REPO/ns3/manet-compare.cc" "$NS3_DIR/scratch/manet-compare.cc"
  echo "[midterm] linked scratch/manet-compare.cc -> $REPO/ns3/manet-compare.cc"
fi

# 2) build (only recompiles what changed)
cd "$NS3_DIR"
./ns3 build

# 3) run every protocol for every seed
mkdir -p "$RESULTS"
for run in $SEEDS; do
  for p in aodv olsr dsdv; do
    ./ns3 run --no-build "manet-compare --protocol=$p --nodes=$NODES --speed=$SPEED --run=$run --outDir=$RESULTS"
  done
done

# 4) KPIs + plots from the CSV logs
python3 "$REPO/scripts/analyze_midterm.py" "$RESULTS" "$PLOTS" --nodes "$NODES" --speed "$SPEED"
