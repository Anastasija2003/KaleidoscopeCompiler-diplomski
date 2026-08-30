#!/usr/bin/env python3
import argparse
import random
import re
import sys

FUNC_RE = re.compile(r"def\s+(f\d+)\s*\([^)]*\)\n((?:(?!\ndef ).)*)", re.DOTALL)
NUM_RE = re.compile(r"(\d+\.\d+)")
CALL_RE = re.compile(r"\bf(\d+)\s*\(")


def parse_functions(text):
    functions = {}
    for m in FUNC_RE.finditer(text):
        name = m.group(1)
        functions[name] = (m.start(), m.end(), m.group(0), m.group(2))
    return functions


def find_callees(body):
    return CALL_RE.findall(body)


def select_targets(functions, target, num_changes, rng):
    names = list(functions.keys())
    if target == "random":
        return rng.sample(names, min(num_changes, len(names)))
    if target == "leaf":
        leaves = [n for n, (_, _, _, body) in functions.items() if not find_callees(body)]
        return rng.sample(leaves, min(num_changes, len(leaves)))
    if target == "hub":
        counts = {}
        for _, (_, _, _, body) in functions.items():
            for c in find_callees(body):
                callee = f"f{c}"
                counts[callee] = counts.get(callee, 0) + 1
        ranked = sorted(counts, key=lambda n: -counts[n])
        return ranked[:num_changes]
    if target.startswith("name="):
        return [target[len("name="):]]
    raise ValueError(f"unknown target selector: {target}")


def mutate_block(block):
    matches = list(NUM_RE.finditer(block))
    if not matches:
        return block, False
    last = matches[-1]
    new_val = float(last.group(1)) + 1.0
    return block[: last.start()] + f"{new_val}" + block[last.end():], True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--in", dest="infile", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--num-changes", type=int, default=1)
    parser.add_argument("--target", default="random")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    rng = random.Random(args.seed)

    with open(args.infile) as f:
        text = f.read()

    functions = parse_functions(text)
    targets = select_targets(functions, args.target, args.num_changes, rng)

    changed = []
    for name in targets:
        if name not in functions:
            continue
        start, end, block, _ = functions[name]
        new_block, ok = mutate_block(block)
        if not ok:
            continue
        text = text[:start] + new_block + text[end:]
        changed.append(name)
        functions = parse_functions(text)

    with open(args.out, "w") as f:
        f.write(text)

    print(f"Mutated {len(changed)} function(s): {', '.join(changed)}",
          file=sys.stderr)


if __name__ == "__main__":
    main()
