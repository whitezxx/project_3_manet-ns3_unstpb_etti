#!/usr/bin/env python3
"""
analyze_midterm.py -- KPIs and plots for the midterm baseline (Project 3).

Usage:
    python3 analyze_midterm.py <results_dir> <plots_dir> [--nodes 50] [--speed 0] [--warmup 20]

Input : the CSV logs written by manet-compare.cc (<tag>_tx.csv, _rx.csv, _ctrl.csv, _summary.csv)
Output: <plots_dir>/kpis.csv           one row per run
        <plots_dir>/pdr.png            PDR per protocol
        <plots_dir>/delay.png          delay p50 / p95 per protocol
        <plots_dir>/control_time.png   routing control packets per second (one run per protocol)
"""
import argparse
import glob
import os
import re

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")  # write PNG files, no window needed
import matplotlib.pyplot as plt

PROTOCOLS = ["aodv", "olsr", "dsdv"]
# file names look like: aodv_n50_s0_r1_summary.csv
TAG = re.compile(r"(?P<protocol>[a-z]+)_n(?P<nodes>\d+)_s(?P<speed>[\d.]+)_r(?P<run>\d+)_summary\.csv$")


def kpis_of_one_run(base, warmup):
    """Read the logs of one run and return a dict with its KPIs."""
    tx = pd.read_csv(base + "_tx.csv")      # flow, seq, t_tx, bytes
    rx = pd.read_csv(base + "_rx.csv")      # flow, seq, t_tx, t_rx, delay_ms, bytes
    ctrl = pd.read_csv(base + "_ctrl.csv")  # t, node, port, bytes

    # Warm-up: ignore packets SENT before `warmup` seconds (same rule for every protocol).
    tx = tx[tx["t_tx"] >= warmup]
    rx = rx[rx["t_tx"] >= warmup]
    ctrl = ctrl[ctrl["t"] >= warmup]

    sent = len(tx)
    received = len(rx)

    # Jitter: per flow, mean |delay(i) - delay(i-1)| over consecutive received packets,
    # then the average over flows.
    rx = rx.sort_values(["flow", "seq"])
    jitter_per_flow = rx.groupby("flow")["delay_ms"].apply(lambda d: d.diff().abs().mean())

    return {
        "sent": sent,
        "received": received,
        "pdr": 100.0 * received / sent if sent else np.nan,
        "delay_mean": rx["delay_ms"].mean(),
        "delay_p50": rx["delay_ms"].quantile(0.50),
        "delay_p95": rx["delay_ms"].quantile(0.95),
        "jitter": jitter_per_flow.mean(),
        "ctrl_pkts": len(ctrl),
        "nrl": len(ctrl) / received if received else np.nan,  # normalized routing load
        "dead_flows": int(10 - rx["flow"].nunique()),         # flows that delivered nothing
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results")
    ap.add_argument("plots")
    ap.add_argument("--nodes", type=int, default=50)
    ap.add_argument("--speed", type=float, default=0.0)
    ap.add_argument("--warmup", type=float, default=20.0)
    args = ap.parse_args()
    os.makedirs(args.plots, exist_ok=True)

    # 1) one row of KPIs per run found in the results directory
    rows = []
    for path in sorted(glob.glob(os.path.join(args.results, "*_summary.csv"))):
        m = TAG.search(os.path.basename(path))
        if not m:
            continue
        if int(m["nodes"]) != args.nodes or float(m["speed"]) != args.speed:
            continue
        base = path[: -len("_summary.csv")]
        row = {"protocol": m["protocol"], "nodes": int(m["nodes"]),
               "speed": float(m["speed"]), "run": int(m["run"])}
        row.update(kpis_of_one_run(base, args.warmup))
        rows.append(row)
    if not rows:
        raise SystemExit(f"no runs with nodes={args.nodes} speed={args.speed} in {args.results}")

    kpis = pd.DataFrame(rows).sort_values(["protocol", "run"])
    kpis.to_csv(os.path.join(args.plots, "kpis.csv"), index=False)

    # 2) mean over runs per protocol (with 1 seed this is just the value of that run)
    mean = kpis.groupby("protocol").mean(numeric_only=True).reindex(PROTOCOLS).dropna(how="all")
    n_runs = kpis.groupby("protocol")["run"].count().reindex(mean.index)
    labels = [p.upper() for p in mean.index]
    subtitle = (f"{args.nodes} nodes, {args.speed:g} m/s, {int(n_runs.min())} seed(s), "
                f"warm-up {args.warmup:g} s, offered load 20 pkt/s (20.5 kbps)")

    # 3) PDR plot
    fig, ax = plt.subplots(figsize=(6, 4))
    bars = ax.bar(labels, mean["pdr"], color = ['#FFFF00', '#D62728', '#2CA02C'][: len(labels)])
    ax.bar_label(bars, fmt="%.1f %%")
    ax.set_ylabel("Packet delivery ratio (%)")
    ax.set_ylim(0, 105)
    ax.set_title("PDR\n" + subtitle, fontsize=9)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(args.plots, "pdr.png"), dpi=150)
    plt.close(fig)

    # 4) Delay plot: p50 and p95 side by side (linear scale; for mobile runs, where p95 can be
    #    100x larger than p50, switch to ax.set_yscale("log"))
    x = np.arange(len(labels))
    fig, ax = plt.subplots(figsize=(6, 4))
    b1 = ax.bar(x - 0.2, mean["delay_p50"], width=0.4, label="p50")
    b2 = ax.bar(x + 0.2, mean["delay_p95"], width=0.4, label="p95")
    ax.bar_label(b1, fmt="%.1f", fontsize=8)
    ax.bar_label(b2, fmt="%.1f", fontsize=8)
    ax.set_xticks(x, labels)
    ax.set_ylim(0, mean["delay_p95"].max() * 1.2)
    ax.set_ylabel("End-to-end delay (ms)")
    ax.set_title("Delay\n" + subtitle, fontsize=9)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(args.plots, "delay.png"), dpi=150)
    plt.close(fig)

    # 5) Control packets per second over time, first run of each protocol (whole run, no warm-up cut)
    fig, ax = plt.subplots(figsize=(7, 4))
    for p in mean.index:
        run = int(kpis[kpis.protocol == p]["run"].min())
        f = os.path.join(args.results, f"{p}_n{args.nodes}_s{args.speed:g}_r{run}_ctrl.csv")
        ctrl = pd.read_csv(f)
        per_second = ctrl.groupby(ctrl["t"].astype(int)).size()
        ax.plot(per_second.index, per_second.values, label=p.upper(), linewidth=1)
    ax.axvline(args.warmup, color="gray", linestyle="--", linewidth=1)
    ax.text(args.warmup + 2, ax.get_ylim()[1] * 0.9, "end of warm-up", fontsize=8, color="gray")
    ax.set_xlabel("simulated time (s)")
    ax.set_ylabel("routing control packets / s (all nodes)")
    ax.set_title("Routing control traffic over time\n" + subtitle, fontsize=9)
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(args.plots, "control_time.png"), dpi=150)
    plt.close(fig)

    # 6) print a table for the terminal / slides
    cols = ["sent", "received", "pdr", "delay_p50", "delay_p95", "jitter", "nrl", "dead_flows"]
    print(subtitle)
    print(mean[cols].round(2).to_string())
    print(f"\nwrote kpis.csv, pdr.png, delay.png, control_time.png in {args.plots}")


if __name__ == "__main__":
    main()
