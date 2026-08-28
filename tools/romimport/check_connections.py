
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

DEFAULT_BLOCKS = os.path.join(ROOT, "mod_runtime", "generatedmaps", "johto",
                              "blocks")

OPPOSITE = {"north": "south", "south": "north", "west": "east", "east": "west"}

def read_blocks(blocks_dir):
    conns, sizes, have = {}, {}, set()
    for fn in sorted(os.listdir(blocks_dir)):
        if not fn.endswith(".block"):
            continue
        have.add(os.path.splitext(fn)[0])
        with open(os.path.join(blocks_dir, fn), encoding="utf-8",
                  errors="replace") as fh:
            for line in fh:
                if line.startswith("connect "):
                    _, src, d, dest, coord, adjust = line.split()
                    conns[(src, d)] = (dest, int(coord), int(adjust))
                elif line.startswith("mapsize "):
                    _, nm, w, h = line.split()
                    sizes[nm] = (int(w), int(h))
    return conns, sizes, have

def check(blocks_dir, verbose=False):
    conns, sizes, have = read_blocks(blocks_dir)
    problems = []

    pairs = 0
    for (src, d), (dest, _coord, adj) in sorted(conns.items()):
        back = conns.get((dest, OPPOSITE[d]))
        if not back or back[0] != src:
            problems.append(f"one-way: {src} {d} -> {dest}, no {OPPOSITE[d]} "
                            f"connection back (found {back})")
            continue
        pairs += 1
        if adj != -back[2]:
            problems.append(f"not reciprocal: {src} {d} {dest} adjust {adj}, "
                            f"but {dest} {OPPOSITE[d]} {src} adjust {back[2]} "
                            f"(expected {-adj})")

    for (src, d), (dest, coord, _adj) in sorted(conns.items()):
        if dest not in have:
            problems.append(f"missing target: {src} {d} -> {dest} has no "
                            f".block file")
            continue
        if dest not in sizes:
            problems.append(f"no mapsize for {dest} (target of {src} {d})")
            continue
        w, h = sizes[dest]

        span = (4 * h) if d in ("north", "south") else (4 * w)
        want = {"north": span - 1, "south": 1, "west": span - 2, "east": 0}[d]
        if coord != want:
            problems.append(f"arrival coord: {src} {d} {dest} is {coord}, "
                            f"the port's convention is {want}")

    print(f"connections: {len(conns)}   reciprocal pairs: {pairs}   "
          f"block files: {len(have)}")
    if verbose:
        for (src, d), (dest, coord, adj) in sorted(conns.items()):
            print(f"  {src} {d} {dest} {coord} {adj}")
    for p in problems:
        print(f"  !! {p}")
    print(f"\n{'PASSED' if not problems else f'FAILED -- {len(problems)} problem(s)'}")
    return not problems

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--blocks", default=DEFAULT_BLOCKS,
                    help="directory of emitted .block files")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    return 0 if check(args.blocks, args.verbose) else 1

if __name__ == "__main__":
    sys.exit(main())
