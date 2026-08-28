
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SCENES = os.path.join(REPO, "mod_runtime", "scenes")
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "assetpack"))

import emit_kanto as E
from gen1_rom import Gen1Rom, RomError, default_paths

SPECIES_VERBS = ("give_monster", "give_pokemon", "cry_on_print", "cry", "name",
                 "show_dex", "prize_list", "has_pokemon", "swap_pokemon",

                 "wildbattle", "if", "elif", "let", "while", "until")

VERB_RENAMES = {
    "give_pokemon":       "give_monster",
    "bills_pokemon_list": "bills_monster_list",
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    a = ap.parse_args()

    dr, ds = default_paths(REPO)
    try:
        rom = Gen1Rom(a.rom or dr, a.sym or ds)
    except RomError as e:
        sys.exit("error: %s" % e)

    internal = E.species_names(rom)
    dex_of = {}
    for dex in range(1, 152):
        pass

    off = rom.offset("PokedexOrder") if "PokedexOrder" in rom.sym else None
    if off is None:
        off = rom.offset("PokedexOrder") or rom.offset("MonsterPokedexOrder")
    if off is None:
        sys.exit("cannot find the ROM's dex-order table")
    for internal_idx in range(1, 191):
        d = rom.data[off + internal_idx - 1]
        if 1 <= d <= 151 and internal_idx in internal:
            dex_of[internal[internal_idx]] = d

    changed_files = 0
    n = 0
    per = {}
    for root, _d, files in os.walk(SCENES):
        if os.path.basename(root) == "tests":
            continue
        for fn in sorted(files):
            if not fn.endswith(".scene"):
                continue
            path = os.path.join(root, fn)
            src = open(path, encoding="utf-8", errors="replace").read()
            out_lines, hit = [], 0
            for ln in src.split("\n"):
                stripped = ln.strip()
                verb = stripped.split()[0] if stripped.split() else ""
                if verb in VERB_RENAMES:
                    ln = ln.replace(verb, VERB_RENAMES[verb], 1)
                    hit += 1
                    per[verb] = per.get(verb, 0) + 1
                if verb in SPECIES_VERBS and '"' not in ln:
                    def sub(m):
                        nonlocal hit
                        w = m.group(0)
                        if w in dex_of:
                            hit += 1
                            per[w] = per.get(w, 0) + 1
                            return "MONSTER%03d" % dex_of[w]
                        return w
                    ln = re.sub(r"[A-Z][A-Z0-9_.]{2,}", sub, ln)
                out_lines.append(ln)
            if hit:
                changed_files += 1
                n += hit
                if a.apply:
                    open(path, "w", encoding="utf-8",
                         newline=chr(10)).write("\n".join(out_lines))

    print("%s %d species name(s) in %d file(s)"
          % ("scrubbed" if a.apply else "would scrub", n, changed_files))
    for k, v in sorted(per.items(), key=lambda kv: -kv[1])[:12]:
        if k in dex_of:
            print("   %-20s x%-3d -> MONSTER%03d" % (k, v, dex_of[k]))
        else:
            print("   %-20s x%-3d -> %s" % (k, v, VERB_RENAMES.get(k, "?")))
    if not a.apply:
        print("\n(dry run -- pass --apply to write)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
