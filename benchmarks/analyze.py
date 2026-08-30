#!/usr/bin/env python3
import argparse
import csv
import os
import sys

import matplotlib.pyplot as plt

TOPOLOGY_ORDER = ["leaf-only", "chain", "hub", "random-dag"]
TOPOLOGY_COLORS = {
    "leaf-only": "#2a78d6",
    "chain": "#eb6834",
    "hub": "#1baf7a",
    "random-dag": "#eda100",
}

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
STATUS_GOOD = "#0ca30c"
STATUS_CRITICAL = "#d03b3b"


def load_rows(path):
    with open(path) as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        r["size"] = int(r["size"])
        r["num_changes"] = int(r["num_changes"])
        r["time_s"] = float(r["time_s"])
        r["dirty"] = int(r["dirty"])
        r["reused"] = int(r["reused"])
    return rows


def index_rows(rows):
    return {(r["topology"], r["size"], r["mode"], r["num_changes"]): r for r in rows}


def style_axes(ax):
    ax.set_facecolor(SURFACE)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(AXIS)
    ax.spines["bottom"].set_color(AXIS)
    ax.tick_params(colors=MUTED, labelsize=8)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def shared_legend(fig, axes):
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=len(labels), frameon=False,
               bbox_to_anchor=(0.5, 1.06), fontsize=9, labelcolor=INK)


def plot_speedup(rows, sizes, out_path):
    idx = index_rows(rows)
    fig, axes = plt.subplots(1, len(sizes), figsize=(4 * len(sizes), 4), sharey=True)
    fig.patch.set_facecolor(SURFACE)
    axes = list(axes) if len(sizes) > 1 else [axes]

    for ax, size in zip(axes, sizes):
        style_axes(ax)
        for topo in TOPOLOGY_ORDER:
            baseline = idx[(topo, size, "baseline", 0)]["time_s"]
            changes = sorted({r["num_changes"] for r in rows
                               if r["topology"] == topo and r["mode"] == "incremental_warm"
                               and r["size"] == size})
            xs, ys = [], []
            for nc in changes:
                row = idx.get((topo, size, "incremental_warm", nc))
                if row is None:
                    continue
                xs.append(nc)
                ys.append(baseline / row["time_s"])
            ax.plot(xs, ys, marker="o", markersize=6, linewidth=2,
                    color=TOPOLOGY_COLORS[topo], label=topo)
        ax.axhline(1.0, color=MUTED, linewidth=1, linestyle="--")
        ax.set_title(f"{size} funkcija", color=INK, fontsize=11)
        ax.set_xlabel("broj izmenjenih funkcija", color=MUTED, fontsize=9)

    ymin, ymax = axes[0].get_ylim()
    for ax in axes:
        ax.axhspan(1.0, ymax, color=STATUS_GOOD, alpha=0.06, zorder=0)
        ax.axhspan(ymin, 1.0, color=STATUS_CRITICAL, alpha=0.06, zorder=0)
    axes[0].set_ylim(ymin, ymax)

    axes[0].text(0.02, 0.97, "brže od baseline-a", transform=axes[0].transAxes,
                 color=STATUS_GOOD, fontsize=8, va="top", ha="left")
    axes[0].text(0.02, 0.03, "sporije od baseline-a", transform=axes[0].transAxes,
                 color=STATUS_CRITICAL, fontsize=8, va="bottom", ha="left")
    axes[0].set_ylabel("ubrzanje (baseline / inkrementalno)", color=INK, fontsize=9)
    shared_legend(fig, axes)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def plot_dirty_fraction(rows, sizes, out_path):
    idx = index_rows(rows)
    fig, axes = plt.subplots(1, len(sizes), figsize=(4 * len(sizes), 4), sharey=True)
    fig.patch.set_facecolor(SURFACE)
    axes = list(axes) if len(sizes) > 1 else [axes]

    for ax, size in zip(axes, sizes):
        style_axes(ax)
        for topo in TOPOLOGY_ORDER:
            changes = sorted({r["num_changes"] for r in rows
                               if r["topology"] == topo and r["mode"] == "incremental_warm"
                               and r["size"] == size})
            xs, ys = [], []
            for nc in changes:
                row = idx.get((topo, size, "incremental_warm", nc))
                if row is None:
                    continue
                xs.append(nc)
                ys.append(100.0 * row["dirty"] / size)
            ax.plot(xs, ys, marker="o", markersize=6, linewidth=2,
                    color=TOPOLOGY_COLORS[topo], label=topo)
        ax.set_title(f"{size} funkcija", color=INK, fontsize=11)
        ax.set_xlabel("broj izmenjenih funkcija", color=MUTED, fontsize=9)
        ax.set_ylim(-5, 105)

    axes[0].set_ylabel("% funkcija koje su dirty", color=INK, fontsize=9)
    shared_legend(fig, axes)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def plot_absolute_time(rows, sizes, out_path):
    idx = index_rows(rows)
    fig, axes = plt.subplots(len(TOPOLOGY_ORDER), len(sizes),
                              figsize=(3.6 * len(sizes), 2.6 * len(TOPOLOGY_ORDER)))

    fig.patch.set_facecolor(SURFACE)

    for row, topo in enumerate(TOPOLOGY_ORDER):
        for col, size in enumerate(sizes):
            ax = axes[row][col]
            style_axes(ax)
            baseline = idx[(topo, size, "baseline", 0)]["time_s"]
            changes = sorted({r["num_changes"] for r in rows
                               if r["topology"] == topo and r["mode"] == "incremental_warm"
                               and r["size"] == size})
            xs, ys = [], []
            for nc in changes:
                r = idx.get((topo, size, "incremental_warm", nc))
                if r is None:
                    continue
                xs.append(nc)
                ys.append(r["time_s"])
            ax.plot(xs, ys, marker="o", markersize=5, linewidth=2,
                    color=TOPOLOGY_COLORS[topo])
            ax.axhline(baseline, color=MUTED, linewidth=1.3, linestyle="--")

            ymin, ymax = min(ys + [baseline]), max(ys + [baseline])
            pad = max((ymax - ymin) * 0.25, ymax * 0.05)
            ax.set_ylim(ymin - pad, ymax + pad)

            if row == 0:
                ax.set_title(f"{size} funkcija", color=INK, fontsize=11)
            if col == 0:
                ax.set_ylabel(topo, color=TOPOLOGY_COLORS[topo], fontsize=10,
                              fontweight="bold")
            if row == len(TOPOLOGY_ORDER) - 1:
                ax.set_xlabel("broj izmenjenih funkcija", color=MUTED, fontsize=8)
            ax.tick_params(labelsize=7)

    fig.text(0.5, -0.01,
              "isprekidana siva linija = baseline (puna kompilacija) za taj panel — "
              "svaki red ima SVOJU y-osu",
              ha="center", color=MUTED, fontsize=8)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def plot_practical_case(rows, sizes, out_path, num_changes=1):
    idx = index_rows(rows)
    fig, ax = plt.subplots(figsize=(7, 4.5))
    fig.patch.set_facecolor(SURFACE)
    style_axes(ax)

    n_topo = len(TOPOLOGY_ORDER)
    width = 0.8 / n_topo
    x = list(range(len(sizes)))

    for i, topo in enumerate(TOPOLOGY_ORDER):
        ys = []
        for size in sizes:
            baseline = idx[(topo, size, "baseline", 0)]["time_s"]
            row = idx.get((topo, size, "incremental_warm", num_changes))
            ys.append(baseline / row["time_s"] if row else 0)
        offsets = [xi + (i - (n_topo - 1) / 2) * width for xi in x]
        ax.bar(offsets, ys, width=width * 0.9, color=TOPOLOGY_COLORS[topo], label=topo)

    ymin, ymax = ax.get_ylim()
    ax.axhspan(1.0, max(ymax, 1.05), color=STATUS_GOOD, alpha=0.06, zorder=0)
    ax.axhspan(0, 1.0, color=STATUS_CRITICAL, alpha=0.06, zorder=0)
    ax.set_ylim(0, max(ymax, 1.05))
    ax.axhline(1.0, color=MUTED, linewidth=1, linestyle="--")

    ax.set_xticks(x)
    ax.set_xticklabels([f"{s} funkcija" for s in sizes], color=INK, fontsize=9)
    ax.set_ylabel("ubrzanje (baseline / inkrementalno)", color=INK, fontsize=9)
    ax.set_title(f"Tipičan slučaj: {num_changes} izmenjena funkcija", color=INK, fontsize=11)
    ax.legend(frameon=False, fontsize=8, labelcolor=INK, loc="upper left")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def plot_baseline_scaling(rows, out_path):
    idx = index_rows(rows)
    sizes = sorted({r["size"] for r in rows})
    fig, ax = plt.subplots(figsize=(6, 4.5))
    fig.patch.set_facecolor(SURFACE)
    style_axes(ax)

    for topo in TOPOLOGY_ORDER:
        ys = [idx[(topo, s, "baseline", 0)]["time_s"] for s in sizes]
        ax.plot(sizes, ys, marker="o", markersize=6, linewidth=2,
                color=TOPOLOGY_COLORS[topo], label=topo)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xticks(sizes)
    ax.set_xticklabels([str(s) for s in sizes])
    ax.set_xlabel("broj funkcija u fajlu", color=MUTED, fontsize=9)
    ax.set_ylabel("vreme pune kompilacije (s)", color=INK, fontsize=9)
    ax.legend(frameon=False, fontsize=8, labelcolor=INK, loc="upper left")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", required=True)
    parser.add_argument("--outdir", default="benchmarks/results")
    args = parser.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    rows = load_rows(args.results)
    sizes = sorted({r["size"] for r in rows})

    plot_speedup(rows, sizes, os.path.join(args.outdir, "speedup.png"))
    plot_dirty_fraction(rows, sizes, os.path.join(args.outdir, "dirty_fraction.png"))
    plot_baseline_scaling(rows, os.path.join(args.outdir, "baseline_scaling.png"))
    plot_absolute_time(rows, sizes, os.path.join(args.outdir, "absolute_time.png"))
    plot_practical_case(rows, sizes, os.path.join(args.outdir, "practical_case.png"))

    print(f"Wrote graphs to {args.outdir}/", file=sys.stderr)


if __name__ == "__main__":
    main()
