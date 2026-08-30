#!/usr/bin/env python3
import argparse
import sys


def gen_leaf_only(n, rng, density):
    return [(i, []) for i in range(n)]


def gen_chain(n, rng, density):
    return [(i, [i - 1] if i > 0 else []) for i in range(n)]


def gen_hub(n, rng, density):
    return [(i, [0] if i > 0 else []) for i in range(n)]


def gen_random_dag(n, rng, density):
    edges = []
    for i in range(n):
        if i == 0:
            edges.append((i, []))
            continue
        candidates = list(range(i))
        k = max(0, min(len(candidates), round(density * len(candidates))))
        callees = sorted(rng.sample(candidates, k)) if k > 0 else []
        edges.append((i, callees))
    return edges


TOPOLOGIES = {
    "leaf-only": gen_leaf_only,
    "chain": gen_chain,
    "hub": gen_hub,
    "random-dag": gen_random_dag,
}


def render_function(i, callees):
    if not callees:
        body = f"x + {i}.0"
    else:
        terms = " + ".join(f"f{c}(x)" for c in callees)
        body = f"{terms} + {i}.0"
    return f"def f{i}(x)\n  {body}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--num-functions", type=int, required=True)
    parser.add_argument("--topology", choices=sorted(TOPOLOGIES), required=True)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--density", type=float, default=0.15)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    import random

    rng = random.Random(args.seed)
    edges = TOPOLOGIES[args.topology](args.num_functions, rng, args.density)
    functions = [render_function(i, callees) for i, callees in edges]

    with open(args.out, "w") as f:
        f.write("\n\n".join(functions))
        f.write("\n")

    print(f"Wrote {args.num_functions} functions ({args.topology}) to {args.out}",
          file=sys.stderr)


if __name__ == "__main__":
    main()
