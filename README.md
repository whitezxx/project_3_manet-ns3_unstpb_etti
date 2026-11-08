# Project 3 -- MANET routing in ns-3: AODV vs OLSR vs DSDV

Data Communications group project, 2026-2027.
Team: Lupu Gabriel-Irinel (simulation, C++), Filimon Alexandra-Elena (instrumentation, batch),
Ermilios Ilias-Alexandros (analysis, report).

## What this repository does

It compares three ad-hoc routing protocols in ns-3.48 (AODV, reactive; OLSR, proactive link-state;
DSDV, proactive distance-vector). All three run on the same topology, traffic and seeds. The
comparison measures packet delivery ratio, end-to-end delay, jitter, normalized routing load and
hop count, under different node densities and speeds.

**Status (midterm):** the 50-node static baseline (3 protocols, 1 seed) is reproduced with one
command. The full core matrix (75 runs) is planned. See [`roadmap/roadmap.pdf`](roadmap/roadmap.pdf).

## Requirements

* Ubuntu 26.04 (native or WSL2)
* ns-3.48 built with the optimized profile
* Python 3 with pandas, numpy and matplotlib

### Setup from scratch

```bash
# ns-3.48
sudo apt install g++ python3 cmake ninja-build git
mkdir -p ~/project_dc && cd ~/project_dc
git clone https://gitlab.com/nsnam/ns-3-dev.git && cd ns-3-dev
git checkout ns-3.48
./ns3 configure --build-profile=optimized --enable-examples
./ns3 build

# Python packages
sudo apt install python3-pandas python3-numpy python3-matplotlib
```

## Reproduce the midterm result (one command)

```bash
git clone https://github.com/whitezxx/project_3_manet-ns3_unstpb_etti.git project_3_manet-ns3_unstpb_etti
cd project_3_manet-ns3_unstpb_etti
NS3_DIR=~/project_dc/ns-3-dev ./run_midterm.sh
```

The script:

1. links `ns3/manet-compare.cc` into `<ns-3>/scratch/`;
2. builds the program;
3. runs AODV, OLSR and DSDV;
4. computes the KPIs and writes the plots.

Output: `plots/midterm/pdr.png`, `delay.png`, `control_time.png` and `kpis.csv`. Raw logs go to
`results/midterm/`.

### Expected output

50 nodes, static, seed 1, warm-up 20 s. About 20 s of wall-clock time for the three runs.

| Protocol | Sent | Received | PDR % | Delay p50 (ms) | Delay p95 (ms) | Jitter (ms) | NRL  |
|----------|-----:|---------:|------:|---------------:|---------------:|------------:|-----:|
| AODV     | 3580 |     3217 |  89.9 |           1.92 |           2.32 |        0.51 | 4.14 |
| OLSR     | 3580 |     3222 |  90.0 |           1.92 |           2.32 |        0.10 | 2.30 |
| DSDV     | 3580 |     3148 |  87.9 |           1.92 |           3.12 |        8.46 | 6.34 |

### Options

`run_midterm.sh` reads these environment variables:

| Variable | Default                  | Meaning                         |
|----------|--------------------------|---------------------------------|
| `NS3_DIR`| `~/project_dc/ns-3-dev`  | ns-3 source/build directory     |
| `SEEDS`  | `1`                      | seed list, e.g. `"1 2 3 4 5"`   |
| `NODES`  | `50`                     | number of nodes                 |
| `SPEED`  | `0`                      | RandomWaypoint speed (m/s)      |

Example: `SEEDS="1 2 3 4 5" NS3_DIR=~/project_dc/ns-3-dev ./run_midterm.sh`

To change the warm-up, run the analysis directly:

```bash
python3 scripts/analyze_midterm.py results/midterm plots/midterm --nodes 50 --speed 0 --warmup 40
```

## Layout

| Path                         | Content                                              | Owner                     |
|------------------------------|------------------------------------------------------|---------------------------|
| `ns3/manet-compare.cc`       | ns-3 scenario program (linked into `<ns-3>/scratch/`)| Lupu Gabriel-Irinel       |
| `run_midterm.sh`             | build + baseline runs + analysis                     | Filimon Alexandra-Elena   |
| `scripts/analyze_midterm.py` | KPIs and plots from the CSV logs                     | Ermilios Ilias-Alexandros |
| `NOTES.md`                   | decisions, sanity checks, measured run times         | all                       |
| `roadmap/`                   | roadmap (initial and updated for the midterm)        | all                       |
| `slides/`, `report/`         | presentations and final report (to be added)         | all                       |

## Scenario program: `manet-compare.cc`

```bash
./ns3 run "manet-compare --protocol=aodv --nodes=50 --speed=0 --run=1 --outDir=results"
```

| Parameter    | Default   | Meaning                                    |
|--------------|-----------|--------------------------------------------|
| `--protocol` | `aodv`    | `aodv`, `olsr` or `dsdv`                   |
| `--nodes`    | `50`      | number of nodes                            |
| `--speed`    | `0`       | RandomWaypoint speed in m/s (0 = static)   |
| `--pause`    | `2`       | RandomWaypoint pause time in s             |
| `--run`      | `1`       | RngRun (seed index); RngSeed is fixed to 12345 |
| `--time`     | `200`     | simulated time in s                        |
| `--flows`    | `10`      | number of CBR/UDP flows                    |
| `--pktSize`  | `128`     | UDP payload size in bytes                  |
| `--pktRate`  | `2`       | packets/s per flow                         |
| `--range`    | `100`     | radio range in m                           |
| `--outDir`   | `results` | output directory (must exist)              |

The area (500 m x 500 m) is fixed in the code, so density changes only with the number of nodes.

### Output files

Every file name starts with the tag `<protocol>_n<nodes>_s<speed>_r<run>`, e.g. `aodv_n50_s0_r1`.

| File             | Columns                                                                                         |
|------------------|-------------------------------------------------------------------------------------------------|
| `_tx.csv`        | `flow, seq, t_tx, bytes` -- one line per data packet offered by a source                         |
| `_rx.csv`        | `flow, seq, t_tx, t_rx, delay_ms, bytes` -- one line per data packet received                    |
| `_ctrl.csv`      | `t, node, port, bytes` -- one line per routing control packet transmitted by any node            |
| `_flows.csv`     | `flow, src, dst, start` -- source/sink pairs and start times of this run                         |
| `_flowmon.xml`   | raw FlowMonitor output                                                                          |
| `_flowmon.csv`   | `flowId, src, dst, dstPort, txPackets, rxPackets, lostPackets, delaySum_ms, jitterSum_ms, timesForwarded, avgHops` (data flows only) |
| `_summary.csv`   | `protocol, nodes, speed, run, sim_time, flows, app_tx, app_rx, src_drops, ctrl_pkts, ctrl_bytes, wall_s` |

## Scenario (guideline defaults)

* Area 500 m x 500 m, radio range 100 m (`RangePropagationLossModel`).
* 802.11g ad-hoc, fixed 6 Mbps (`ConstantRateWifiManager`, `ErpOfdmRate6Mbps`).
* 10 CBR/UDP flows of 128 B at 2 packets/s between random source/sink pairs (fixed per seed).
  Sources start at a random time in [1, 2] s and stop at 199 s.
* Offered load: 20 packets/s in total, 20.48 kbps at the UDP payload level.
* Mobility: static, or RandomWaypoint with pause 2 s.
* 200 s simulated, first 20 s discarded (warm-up).
* Density as neighbours per node `k = n*pi*r^2/A`: 25 nodes ~ 3, 50 ~ 6, 100 ~ 13.

## Measurement rules

* **One clock.** Every timestamp is ns-3 simulated time. The send time travels in the packet
  (`SeqTsSizeHeader`), so the delay is `t_rx - t_tx` on the same clock.
* **Same conditions for every protocol.** All random variables (Wi-Fi, positions, mobility, flow
  pairs, start times) use fixed RNG streams. At equal `--run`, AODV, OLSR and DSDV see the same
  topology and the same traffic.
* **Constant offered load.** A custom sender (`CbrSend`) generates exactly `pktRate` packets/s per
  flow, whatever the routing state. If the source has no route, the packet is lost and counted in
  `src_drops`. `OnOffApplication` is not used, because it stops generating while no route exists,
  which would make the offered load depend on the protocol.
* **Warm-up.** Data packets *sent* before the warm-up are ignored, both in `tx` and in `rx`.
  Control packets transmitted before the warm-up are ignored.
* **Control overhead.** Counted at the IPv4 `Tx` trace of every node by UDP destination port
  (AODV 654, OLSR 698, DSDV 269). This includes originated and forwarded transmissions.

## KPI definitions

* **PDR (%)** = data packets received / data packets offered by the sources (after the warm-up).
  Packets dropped at the source for lack of a route count as lost.
* **End-to-end delay (ms)** = receive time - send time from the packet header. Reported as mean,
  p50 and p95; p99 only when a run has at least 1000 samples.
* **Jitter (ms)** = mean |d_i - d_(i-1)| over consecutive received packets of a flow, then averaged
  over flows.
* **Normalized routing load (NRL)** = routing control packets transmitted by all nodes / data
  packets delivered.
* **Average hop count** -- planned: from the IP TTL at reception (`Ipv4L3Protocol/LocalDeliver`),
  after the warm-up. FlowMonitor's `avgHops` is exported but not used, because it ignores the
  warm-up and also counts forwards of packets that were lost later.

**Statistics (final matrix):** mean +/- 95 % confidence interval over 5 seeds, Student t with
4 degrees of freedom (t = 2.776). Every result is reported with the offered load, run duration and
number of samples.

## Known limitations

* **Partitions.** With 100 m range, some nodes can be isolated. In the baseline (seed 1), flow 2
  (node 23 -> node 1) delivers nothing under every protocol, which caps PDR at about 90 %. Such
  seeds are reported, not discarded.
* **DSDV convergence.** DSDV still loses packets until about 60 s, after the 20 s warm-up. The
  warm-up value is under review (see `NOTES.md`).
* **Mixed runs.** `analyze_midterm.py` uses *every* run in the results directory that matches
  `--nodes` and `--speed`. Delete old runs before changing seeds, or use a new directory.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `permission denied: ./run_midterm.sh` | `chmod +x run_midterm.sh` |
| A different copy of the program runs | `run_midterm.sh` does not replace an existing `scratch/manet-compare.cc`. Check it with `ls -l <ns-3>/scratch/manet-compare.cc` and remove it if it points elsewhere. |
| `cannot write to ... (does the directory exist?)` | Create `--outDir` first; `run_midterm.sh` does this automatically. |
| `/bin/bash^M: bad interpreter` | The file has Windows line endings: `sed -i 's/\r$//' run_midterm.sh` |
| `*:Zone.Identifier` files appear | They come from copying files from Windows into WSL; delete them and do not commit them. |

## Contributions

* **Lupu Gabriel-Irinel** -- simulation (C++). Scenario program `ns3/manet-compare.cc`:
  topology, Wi-Fi PHY/MAC, mobility, protocol selection, custom CBR sender, fixed RNG streams,
  command-line parameters; sanity tests. Planned: hop count from TTL, position log, Extension B
  (energy).
* **Filimon Alexandra-Elena** -- instrumentation and batch. Per-packet CSV traces, control-packet
  counting, CSV log format, `run_midterm.sh`. Planned: parallel resumable batch script for the
  full matrix, one-command reproduction of all results.
* **Ermilios Ilias-Alexandros** -- analysis and report. `scripts/analyze_midterm.py`: warm-up
  filtering, KPIs, plots from logs. Planned: matrix analysis with confidence intervals, warm-up
  sensitivity study, report, slides, Extension C (bursty traffic).

The commit history (`git log`) and the decision log in `NOTES.md` document who did what.
