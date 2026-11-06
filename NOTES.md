# NOTES -- decisions, sanity checks, measured times

Working log of the project. Every decision that affects the results is recorded here with its
reason. Raw logs are not versioned (`results/` is in `.gitignore`); every number below can be
regenerated with the scripts.

## Environment

| Item           | Value                                     |
|----------------|-------------------------------------------|
| ns-3           | 3.48, `--build-profile=optimized`         |
| OS             | Ubuntu 26.04 on WSL2                      |
| Python         | 3.14.4                                    |
| Libraries      | pandas 2.3.3, numpy 2.3.5, matplotlib 3.10.7+dfsg1| 
| Batch machine  | AMD Ryzen AI 9 HX 370 (12 cores), 32 GB RAM |

## Decisions

| Date       | Who                       | Decision | Why |
|------------|---------------------------|----------|-----|
| 2026-11-05 | Lupu Gabriel-Irinel       | Custom CBR sender (`CbrSend`) instead of `OnOffApplication` | With OnOff, OLSR offered 3431 packets vs 3954 for AODV/DSDV: OnOff stops generating packets while the source has no route, so the offered load depended on the protocol. CbrSend offers exactly 2 packets/s per flow for every protocol; packets without a route at the source are counted in `src_drops`. |
| 2026-11-05 | Lupu Gabriel-Irinel       | Fixed RNG streams (`AssignStreams`, `SetStream`) | Same positions, Wi-Fi randomness and source/sink pairs for all protocols at the same `--run`. Streams: Wi-Fi 100000, positions 200000, mobility 300000, flow pairs 400000, start times 400001. Verified: `diff` of `_flows.csv` is empty. |
| 2026-11-05 | Lupu Gabriel-Irinel       | Send time carried in the packet (`SeqTsSizeHeader`) | Delay is measured on one clock (simulated time) at the application layer, per packet, with sequence numbers for jitter and loss analysis. |
| 2026-11-05 | Lupu Gabriel-Irinel       | `RangePropagationLossModel`, MaxRange = 100 m | Exact 100 m range as in the guideline defaults: two nodes are neighbours if and only if they are within 100 m, so connectivity can be checked from the positions alone. |
| 2026-11-05 | Filimon Alexandra-Elena   | Control packets counted at IPv4 `Tx` of every node, by UDP port (654/698/269) | The same method for all three protocols, independent of their internal traces. Counts originated and forwarded transmissions, i.e. the real load on the channel. |
| 2026-11-06 | Ermilios Ilias-Alexandros | Warm-up filter by *send* time | A packet sent before the warm-up is excluded from both `tx` and `rx`, even if it arrives later; otherwise PDR could exceed the share of packets offered after the warm-up. |
| 2026-11-06 | Ermilios Ilias-Alexandros | Warm-up 20 s (guideline: 10-20 % of the run = 20-40 s) | To be confirmed: DSDV still loses packets until ~60 s. Compare `--warmup 20` and `--warmup 40` on 5 seeds and decide. |
| 2026-11-06 | all                       | Hop count from IP TTL at reception (planned) instead of FlowMonitor `avgHops` | FlowMonitor ignores the warm-up and `timesForwarded` also counts forwards of packets that were lost later. |

## Sanity checks (seed 1)

| Test | Expected | Result |
|------|----------|--------|
| 2 nodes, 1 flow, range 1000 m, 30 s | PDR 100 %, delay ~2 ms | tx=56 rx=56, PDR 100 % |
| Control ports | only the port of the selected protocol | aodv: 654, olsr: 698, dsdv: 269 |
| Same flows for all protocols | `diff` of `_flows.csv` empty | identical |
| rx <= tx per flow | always | yes |
| Offered load after warm-up | 10 flows x 2 pkt/s x 179 s (20 s to 199 s) = 3580 | 3580 for all three protocols |

## Midterm baseline (50 nodes, static, seed 1, warm-up 20 s)

Offered load 20 packets/s (20.48 kbps), 200 s simulated, 180 s measured.

| Protocol | Sent | Received | PDR % | p50 ms | p95 ms | Jitter ms | NRL  |
|----------|-----:|---------:|------:|-------:|-------:|----------:|-----:|
| AODV     | 3580 |     3217 |  89.9 |   1.92 |   2.32 |      0.51 | 4.14 |
| OLSR     | 3580 |     3222 |  90.0 |   1.92 |   2.32 |      0.10 | 2.30 |
| DSDV     | 3580 |     3148 |  87.9 |   1.92 |   3.12 |      8.46 | 6.34 |

Whole-run totals from `*_summary.csv` (0-200 s, **no warm-up cut**, so they differ from the table
above; `ctrl_bytes` = IP packet size, IP + UDP headers included):

| Protocol | app_tx | app_rx | PDR % (whole run) | src_drops | ctrl_pkts | ctrl_bytes | ctrl pkt/s | ctrl kbps | B / ctrl pkt | NRL (whole run) | wall_s |
|----------|-------:|-------:|------:|----------:|----------:|-----------:|------:|-----:|------:|-----:|-----:|
| AODV     | 3954 | 3553 | 89.9 | 0   | 15113 |   740712 |  75.6 | 29.6 |  49.0 | 4.25 | 3.72 |
| OLSR     | 3954 | 3430 | 86.7 | 524 |  8080 |  1172060 |  40.4 | 46.9 | 145.1 | 2.36 | 4.30 |
| DSDV     | 3954 | 3303 | 83.5 | 0   | 22284 |  1536312 | 111.4 | 61.5 |  68.9 | 6.75 | 4.16 |

Derived columns: ctrl pkt/s = ctrl_pkts / 200 s; ctrl kbps = ctrl_bytes x 8 / 200 s;
B / ctrl pkt = ctrl_bytes / ctrl_pkts; NRL = ctrl_pkts / app_rx.

* Offered over the whole run: 3954 packets = 10 flows x ~395 (start in [1, 2] s, stop at 199 s).
* **Control traffic is larger than the data traffic.** All three protocols send more control bytes
  than the data offered at the same (IP) level: 20 pkt/s x 156 B (128 B + 28 B IP/UDP) = 24.96 kbps.
  AODV 1.2x, OLSR 1.9x, DSDV 2.5x. (Control counts every transmission, forwarded ones included;
  data is counted once, at the source.)
* **Fewest packets is not the same as fewest bytes.** OLSR sends the fewest control packets but the
  largest ones (145 B: HELLO with neighbour lists + TC). AODV packets are small (49 B), so AODV has
  the lowest control byte rate. -> Report the NRL in bytes as well as in packets.
* **Only OLSR has source drops (524).** AODV and DSDV report 0, probably because their ns-3
  implementations queue packets at the source while no route exists, whereas OLSR rejects them.
  Flow 2 (unreachable destination) alone accounts for ~395 packets, so ~130 OLSR drops would
  come from convergence. *Hypothesis -- to verify from `_tx.csv` / `_rx.csv` per flow.*
* `wall_s` (3.7-4.3 s) is the simulation alone; the 5-6 s below includes the `./ns3 run` launcher.

Observations:

* Flow 2 (node 23 -> node 1) delivers 0 packets for all three protocols: node 1 is unreachable
  (partition). Without flow 2: AODV 99.9 %, OLSR 96.4 % (whole run), DSDV 92.8 %.
* Losses over time (whole run, without flow 2): OLSR loses only in 0-20 s (convergence);
  DSDV loses 181 in 0-20 s, 57 in 20-40 s, 10 in 40-60 s -> converges more slowly than the warm-up.
* Control traffic: DSDV has peaks every ~15 s (periodic full table dumps); OLSR is flat
  (~50 packets/s: HELLO + TC); AODV ~50-100 packets/s (HELLO every second from each node + RREQ).
* With one seed, the AODV/OLSR difference in PDR (0.1 pp) is not significant.

## Measured run times (WSL2, ns-3.48 optimized, one run each)

| Run | Wall time | RAM |
|-----|-----------|-----|
| 50 nodes static | 5-6 s (`./ns3 run`); 3.7-4.3 s simulation only (`wall_s`) | ~43 MB |
| 50 nodes, 10 m/s | ~20 s | |
| 100 nodes static (OLSR) | ~33 s | |
| 25 nodes static | `<to measure>` | |
| 50 nodes, 1 m/s | `<to measure>` | |
| 100 nodes static (AODV, DSDV) | `<to measure>` | |

The wall time of every run is also written to `_summary.csv` (`wall_s`).

## Open issues

| Issue | Owner | Target |
|-------|-------|--------|
| Decide warm-up (20 s vs 40 s) on 5 seeds | Ermilios | week 8-9 |
| Hop count from TTL (`LocalDeliver` trace) | Lupu | week 8 |
| Position log + partition classification (reachable vs partitioned flows) | Lupu, Ermilios | week 8-10 |
| Parallel resumable batch script for the 75-run matrix; flow-table check for every run | Filimon | week 8 |
| Time AODV/DSDV at 100 nodes, 25 nodes, 50 nodes at 1 m/s | Filimon | week 8 |
| `analyze_midterm.py`: `dead_flows` assumes 10 flows; read the number from `_summary.csv` | Ermilios | week 8 |
| Verify the source-drop hypothesis (OLSR 524, AODV/DSDV 0) per flow; add NRL in bytes to the analysis | Ermilios | week 8 |
| Explain the delay floor p50 = 1.92 ms (identical for all protocols) from 802.11g timing and hop count | all | before final |

## Progress log

| Week | What was done |
|------|---------------|
| 3-4  | Initial roadmap; ns-3.48 built; reference example `manet-routing-compare` run and analysed (P0). |
| 7    | Scenario program, per-packet logging, control counting, midterm script and analysis (P1, P2); baseline results; updated roadmap. |
