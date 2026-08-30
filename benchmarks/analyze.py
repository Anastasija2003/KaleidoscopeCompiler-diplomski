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


def new_fig(figsize=(6.5, 4.5)):
    fig, ax = plt.subplots(figsize=figsize)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(AXIS)
    ax.spines["bottom"].set_color(AXIS)
    ax.tick_params(colors=MUTED, labelsize=8)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    return fig, ax


def save(fig, out_path):
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def changes_for(rows, topo, size):
    return sorted({r["num_changes"] for r in rows
                   if r["topology"] == topo and r["mode"] == "incremental_warm"
                   and r["size"] == size})


def plot_speedup_for_size(rows, idx, size, out_path):
    fig, ax = new_fig()

    for topo in TOPOLOGY_ORDER:
        baseline = idx[(topo, size, "baseline", 0)]["time_s"]
        xs, ys = [], []
        for nc in changes_for(rows, topo, size):
            row = idx.get((topo, size, "incremental_warm", nc))
            if row is None:
                continue
            xs.append(nc)
            ys.append(baseline / row["time_s"])
        ax.plot(xs, ys, marker="o", markersize=7, linewidth=2.5,
                color=TOPOLOGY_COLORS[topo], label=topo)

    ax.axhline(1.0, color=MUTED, linewidth=1, linestyle="--")
    ymin, ymax = ax.get_ylim()
    ax.axhspan(1.0, ymax, color=STATUS_GOOD, alpha=0.06, zorder=0)
    ax.axhspan(ymin, 1.0, color=STATUS_CRITICAL, alpha=0.06, zorder=0)
    ax.set_ylim(ymin, ymax)
    ax.text(0.02, 0.97, "brže od baseline-a", transform=ax.transAxes,
            color=STATUS_GOOD, fontsize=8, va="top", ha="left")
    ax.text(0.02, 0.03, "sporije od baseline-a", transform=ax.transAxes,
            color=STATUS_CRITICAL, fontsize=8, va="bottom", ha="left")

    ax.set_xlabel("broj izmenjenih funkcija", color=MUTED, fontsize=9)
    ax.set_ylabel("ubrzanje (baseline / inkrementalno)", color=INK, fontsize=9)
    ax.set_title(f"Ubrzanje pri {size} funkcija u fajlu", color=INK, fontsize=12)
    ax.legend(frameon=False, fontsize=9, labelcolor=INK, loc="center left",
              bbox_to_anchor=(1.02, 0.5))
    save(fig, out_path)


def plot_dirty_fraction_for_size(rows, idx, size, out_path):
    fig, ax = new_fig()

    for topo in TOPOLOGY_ORDER:
        xs, ys = [], []
        for nc in changes_for(rows, topo, size):
            row = idx.get((topo, size, "incremental_warm", nc))
            if row is None:
                continue
            xs.append(nc)
            ys.append(100.0 * row["dirty"] / size)
        ax.plot(xs, ys, marker="o", markersize=7, linewidth=2.5,
                color=TOPOLOGY_COLORS[topo], label=topo)

    ax.set_ylim(-5, 105)
    ax.set_xlabel("broj izmenjenih funkcija", color=MUTED, fontsize=9)
    ax.set_ylabel("% funkcija koje su dirty", color=INK, fontsize=9)
    ax.set_title(f"Koliko koda postaje dirty pri {size} funkcija", color=INK, fontsize=12)
    ax.legend(frameon=False, fontsize=9, labelcolor=INK, loc="center left",
              bbox_to_anchor=(1.02, 0.5))
    save(fig, out_path)


def plot_absolute_time_one(rows, idx, topo, size, out_path):
    fig, ax = new_fig(figsize=(5.5, 4))

    baseline = idx[(topo, size, "baseline", 0)]["time_s"]
    xs, ys = [], []
    for nc in changes_for(rows, topo, size):
        row = idx.get((topo, size, "incremental_warm", nc))
        if row is None:
            continue
        xs.append(nc)
        ys.append(row["time_s"])

    ax.plot(xs, ys, marker="o", markersize=7, linewidth=2.5,
            color=TOPOLOGY_COLORS[topo], label="inkrementalno")
    ax.axhline(baseline, color=MUTED, linewidth=1.5, linestyle="--", label="baseline")

    ymin, ymax = min(ys + [baseline]), max(ys + [baseline])
    pad = max((ymax - ymin) * 0.25, ymax * 0.05)
    ax.set_ylim(ymin - pad, ymax + pad)

    faster = baseline > max(ys)
    verdict = "brže od baseline-a na svim tačkama" if faster else \
        ("sporije od baseline-a na svim tačkama" if baseline < min(ys) else "meša se")
    verdict_color = STATUS_GOOD if faster else \
        (STATUS_CRITICAL if baseline < min(ys) else MUTED)

    ax.set_xlabel("broj izmenjenih funkcija", color=MUTED, fontsize=9)
    ax.set_ylabel("vreme (s)", color=INK, fontsize=9)
    ax.set_title(f"{topo}, {size} funkcija", color=INK, fontsize=12)
    ax.text(0.5, 1.14, verdict, transform=ax.transAxes, color=verdict_color,
            fontsize=9, ha="center", fontweight="bold")
    ax.legend(frameon=False, fontsize=8, labelcolor=INK, loc="best")
    save(fig, out_path)


def plot_practical_case(rows, idx, sizes, out_path, num_changes=1):
    fig, ax = new_fig(figsize=(7, 4.5))

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
    ax.set_title(f"Tipičan slučaj: {num_changes} izmenjena funkcija", color=INK, fontsize=12)
    ax.legend(frameon=False, fontsize=8, labelcolor=INK, loc="upper left")
    save(fig, out_path)


def plot_baseline_scaling(rows, idx, sizes, out_path):
    fig, ax = new_fig()

    for topo in TOPOLOGY_ORDER:
        ys = [idx[(topo, s, "baseline", 0)]["time_s"] for s in sizes]
        ax.plot(sizes, ys, marker="o", markersize=7, linewidth=2.5,
                color=TOPOLOGY_COLORS[topo], label=topo)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xticks(sizes)
    ax.set_xticklabels([str(s) for s in sizes])
    ax.set_xlabel("broj funkcija u fajlu", color=MUTED, fontsize=9)
    ax.set_ylabel("vreme pune kompilacije (s)", color=INK, fontsize=9)
    ax.set_title("Koliko puna kompilacija skalira sa veličinom", color=INK, fontsize=12)
    ax.legend(frameon=False, fontsize=9, labelcolor=INK, loc="upper left")
    save(fig, out_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", required=True)
    parser.add_argument("--outdir", default="benchmarks/results")
    args = parser.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    rows = load_rows(args.results)
    idx = index_rows(rows)
    sizes = sorted({r["size"] for r in rows})

    for size in sizes:
        plot_speedup_for_size(rows, idx, size,
                               os.path.join(args.outdir, f"speedup_{size}.png"))
        plot_dirty_fraction_for_size(rows, idx, size,
                                      os.path.join(args.outdir, f"dirty_fraction_{size}.png"))
        for topo in TOPOLOGY_ORDER:
            plot_absolute_time_one(
                rows, idx, topo, size,
                os.path.join(args.outdir, f"absolute_time_{topo}_{size}.png"))

    plot_practical_case(rows, idx, sizes, os.path.join(args.outdir, "practical_case.png"))
    plot_baseline_scaling(rows, idx, sizes, os.path.join(args.outdir, "baseline_scaling.png"))

    print(f"Wrote graphs to {args.outdir}/", file=sys.stderr)


if __name__ == "__main__":
    main()
