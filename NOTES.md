# NOTES -- decisions, sanity checks, measured times

Write here every decision (what, why, who, date). This file becomes the "methodology"
section of the report and is the best preparation for the Q&A.

## Decisions
| Date | Who | Decision | Why |
|---|---|---|---|
| 2026-11-05 | A | Custom CBR sender (`CbrSend`) instead of `OnOffApplication` | With OnOff, OLSR offered 3431 packets vs 3954 for AODV/DSDV: OnOff stops generating packets while the source has no route, so the offered load depended on the protocol. CbrSend offers exactly 2 packets/s per flow for every protocol; packets without a route at the source are counted in `src_drops`. |
| 2026-11-05 | A | Fixed RNG streams (`AssignStreams`, `SetStream`) | Same positions, Wi-Fi randomness and source/sink pairs for all protocols at the same `--run`. Verified: `diff` of `_flows.csv` is empty. |
| | C | Warm-up 20 s (guideline: 10-20 % of the run = 20-40 s) | To be confirmed: DSDV still loses packets until ~60 s. Compare `--warmup 20` and `--warmup 40` and decide. |

## Sanity checks (run 1)
| Test | Expected | Result |
|---|---|---|
| 2 nodes, 1 flow, range 1000 m, 30 s | PDR 100 %, delay ~2 ms | tx=56 rx=56, PDR 100 % |
| Control ports | only the port of the selected protocol | aodv: 654, olsr: 698, dsdv: 269 |
| Same flows for all protocols | `diff` empty | identical |
| rx <= tx per flow | always | yes |

## Midterm baseline (50 nodes, static, run 1, warm-up 20 s)
| Protocol | Sent | Received | PDR % | p50 ms | p95 ms | Jitter ms | NRL |
|---|---|---|---|---|---|---|---|
| AODV | 3580 | 3217 | 89.9 | 1.92 | 2.32 | 0.51 | 4.14 |
| OLSR | 3580 | 3222 | 90.0 | 1.92 | 2.32 | 0.10 | 2.30 |
| DSDV | 3580 | 3148 | 87.9 | 1.92 | 3.12 | 8.46 | 6.34 |

Observations:
- Flow 2 (node 23 -> node 1) delivers 0 packets for all three protocols: node 1 is unreachable
  (partition). Without flow 2: AODV 99.9 %, OLSR 96.4 % (whole run), DSDV 92.8 %.
- Losses over time (whole run, without flow 2): OLSR loses only in 0-20 s (convergence);
  DSDV loses 181 in 0-20 s, 57 in 20-40 s, 10 in 40-60 s -> converges more slowly than the warm-up.
- Control traffic: DSDV has peaks every ~15 s (periodic full table dumps); OLSR is flat
  (~50 packets/s: HELLO + TC); AODV ~50-100 packets/s (HELLO every second from each node + RREQ).

## Measured run times (WSL2, ns-3.48 optimized, one run each)
| Run | Wall time | RAM |
|---|---|---|
| 50 nodes static | 5-6 s | ~43 MB |
| 50 nodes, 10 m/s | ~20 s | |
| 100 nodes static (OLSR) | ~33 s | |
