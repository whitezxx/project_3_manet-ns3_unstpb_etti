# Project 3 -- MANET routing in ns-3: AODV vs OLSR vs DSDV

Data Communications group project, 2026-2027.
Team: Lupu Gabriel-Irinel (simulation, repository mentainer), Filimon Alexandra-Elena (instrumentation, batch), Ermilios Ilias-Alexandros (analysis, report).

## What this repository does
Compares three ad-hoc routing protocols in ns-3.48 with identical topology, traffic and seeds,
and measures packet delivery ratio, end-to-end delay, jitter and normalized routing load.

## Requirements
- Ubuntu 24.04 (native or WSL2), ns-3.48 built with `--build-profile=optimized`
- Python 3 with pandas, numpy, matplotlib

## Reproduce the midterm result (one command)
```bash
git clone <repo-url> manet-ns3 && cd manet-ns3
NS3_DIR=~/project_dc/ns-3-dev ./run_midterm.sh
```
Output: `plots/midterm/pdr.png`, `delay.png`, `control_time.png`, `kpis.csv`.

## Layout
| Path | Content | Owner |
|---|---|---|
| `ns3/manet-compare.cc` | ns-3 scenario program (linked into `<ns-3>/scratch/`) | Lupu Gabriel-Irinel |
| `run_midterm.sh` | build + baseline runs + analysis | Filimon Alexandra-Elena |
| `scripts/analyze_midterm.py` | KPIs and plots from the CSV logs | Ermilios Ilias-Alexandros |
| `NOTES.md` | decisions, sanity checks, measured run times | all |
| `roadmap/`, `slides/`, `report/` | documents | all |

## Scenario (guideline defaults)
500 m x 500 m, 100 m range (RangePropagationLossModel), 802.11g 6 Mbps fixed,
10 CBR/UDP flows of 128 B at 2 packets/s, RandomWaypoint (pause 2 s), 200 s simulated, warm-up 20 s.

## KPI definitions
- **PDR (%)** = data packets received / data packets sent (packets sent after the warm-up).
- **Delay (ms)**: receive time - send time carried in the packet header; p50, p95.
- **Jitter (ms)**: mean |d_i - d_(i-1)| over consecutive packets of a flow, averaged over flows.
- **Normalized routing load** = routing control packets transmitted by all nodes / data packets delivered.

## Contributions
See `git log` and `NOTES.md`.