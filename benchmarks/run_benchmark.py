#!/usr/bin/env python3
import argparse
import csv
import re
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DIRTY_RE = re.compile(r"Incremental: (\d+) dirty, (\d+) reused")


def run_timed(cmd):
    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.perf_counter() - start
    if result.returncode != 0:
        print(f"command failed: {' '.join(cmd)}", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)
    return elapsed, result.stdout + result.stderr


def run_python(script, *cmd_args):
    subprocess.run([sys.executable, str(SCRIPT_DIR / script), *cmd_args],
                    check=True, capture_output=True)


def median_timed(cmd, repeats):
    times = []
    output = ""
    for _ in range(repeats):
        elapsed, output = run_timed(cmd)
        times.append(elapsed)
    return statistics.median(times), output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kcc", required=True)
    parser.add_argument("--sizes", default="20,100,500")
    parser.add_argument("--topologies", default="leaf-only,chain,hub,random-dag")
    parser.add_argument("--num-changes", default="1,5,10")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--out", default="results.csv")
    parser.add_argument("--workdir", default="bench_work")
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()

    sizes = [int(s) for s in args.sizes.split(",")]
    topologies = args.topologies.split(",")
    num_changes_list = [int(c) for c in args.num_changes.split(",")]

    workdir = Path(args.workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    rows = []

    for topology in topologies:
        for size in sizes:
            base_ks = workdir / f"{topology}_{size}.ks"
            run_python("generate.py",
                        "--num-functions", str(size),
                        "--topology", topology,
                        "--seed", str(args.seed),
                        "--out", str(base_ks))

            baseline_o = workdir / f"{topology}_{size}_baseline.o"
            elapsed, _ = median_timed([args.kcc, str(base_ks), str(baseline_o)], args.repeats)
            rows.append({"topology": topology, "size": size, "mode": "baseline",
                         "num_changes": 0, "time_s": elapsed, "dirty": size, "reused": 0})

            cache_dir = workdir / f"{topology}_{size}_cache"

            cold_times = []
            cold_output = ""
            for _ in range(args.repeats):
                if cache_dir.exists():
                    shutil.rmtree(cache_dir)
                t, cold_output = run_timed(
                    [args.kcc, "--incremental", "--cache-dir", str(cache_dir), str(base_ks)])
                cold_times.append(t)
            elapsed = statistics.median(cold_times)
            m = DIRTY_RE.search(cold_output)
            dirty, reused = (int(m.group(1)), int(m.group(2))) if m else (size, 0)
            rows.append({"topology": topology, "size": size, "mode": "incremental_cold",
                         "num_changes": 0, "time_s": elapsed, "dirty": dirty, "reused": reused})

            for num_changes in num_changes_list:
                if num_changes > size:
                    continue
                mutated_ks = workdir / f"{topology}_{size}_mut{num_changes}.ks"
                run_python("mutate.py",
                            "--in", str(base_ks),
                            "--out", str(mutated_ks),
                            "--num-changes", str(num_changes),
                            "--target", "random",
                            "--seed", str(args.seed))

                warm_times = []
                warm_output = ""
                for _ in range(args.repeats):
                    t, warm_output = run_timed(
                        [args.kcc, "--incremental", "--cache-dir", str(cache_dir), str(mutated_ks)])
                    warm_times.append(t)
                    run_timed(
                        [args.kcc, "--incremental", "--cache-dir", str(cache_dir), str(base_ks)])
                elapsed = statistics.median(warm_times)
                m = DIRTY_RE.search(warm_output)
                dirty, reused = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
                rows.append({"topology": topology, "size": size, "mode": "incremental_warm",
                             "num_changes": num_changes, "time_s": elapsed,
                             "dirty": dirty, "reused": reused})

            print(f"done: {topology} x {size}", file=sys.stderr)

    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(
            f, fieldnames=["topology", "size", "mode", "num_changes", "time_s", "dirty", "reused"])
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} rows to {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
