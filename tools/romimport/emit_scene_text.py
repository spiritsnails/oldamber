
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "assetpack"))

import gen1_script as G
from gen1_rom import Gen1Rom, RomError, default_paths

OUT = os.path.join(REPO, "mod_runtime", "generatedmaps", "red",
                   "scene_text.tbl")

NAME_HINT = re.compile(r"Text|Str|Msg|Line")

MIN_LEN = 5

EXTRA_SYMBOLS = {
    "DiplomaPlayer",
    "DiplomaCongrats",
    "DiplomaGameFreak",
}

SHORT_ALLOW = {
    "_CinnabarGymSuperNerd5EndBattleText",
    "CinnabarGymSuperNerd5.EndBattleText",
}

def escape(s):
    return (s.replace("\\", "\\\\").replace("\t", " ")
             .replace("\n", "\\n").replace("\f", "\\f")

             .replace("\x0b", "\\c"))

RAM_SLOTS = {
    "wStringBuffer":   "{name}",
    "wcf4b":           "{name}",

    "wBoxMon1Nick":    "{mon}",

    "wNameBuffer":     "{badge}",
    "wGymCityName":    "{city}",
    "wGymLeaderName":  "{leader}",
    "wOaksAideRewardItemName": "{item}",
}

def ram_slot(rom, addr):
    for sym, place in RAM_SLOTS.items():
        e = rom.sym.get(sym)
        if e and e[1] == addr:
            return place
    return "{ram:%04X}" % addr

NUM_SLOTS = {
    "hOaksAideRequirement":  "{req}",
    "hOaksAideNumMonsOwned": "{owned}",
}

def num_slot(rom, addr):
    for sym, place in NUM_SLOTS.items():
        e = rom.sym.get(sym)
        if e and e[1] == addr:
            return place
    return "{num:%04X}" % addr

def decode_runs(rom, off, bank, depth=0):
    cm = G.charmap()
    out = []
    prev_was_far = False
    for _ in range(600):
        just_returned_from_far, prev_was_far = prev_was_far, False
        b = rom.data[off]
        off += 1
        if b == 0x01:
            out.append(ram_slot(rom, rom.data[off] | (rom.data[off + 1] << 8)))
            off += 2
        elif b == 0x09:
            addr = rom.data[off] | (rom.data[off + 1] << 8)
            out.append(num_slot(rom, addr))
            off += 3
        elif b == 0x00:
            continue
        elif b == G.TX_FAR:

            a = rom.data[off] | (rom.data[off + 1] << 8)
            fb = rom.data[off + 2]
            far = a if a < 0x4000 else fb * 0x4000 + (a - 0x4000)
            if depth < 4:
                out.append(decode_runs(rom, far, fb, depth + 1))
            off += 3
            prev_was_far = True
        elif b == 0x50:

            if just_returned_from_far or rom.data[off] not in (0x00, 0x01, 0x09):
                break
        elif b in (0x57, 0x58):
            break
        elif b == 0x08:

            break
        elif b in G.CONTROL:
            out.append(G.CONTROL[b])
        elif b < 0x18:
            continue
        else:
            tok = cm.get(b, "")
            out.append(G.SUBSTITUTIONS.get(tok, tok) if len(tok) > 1 else tok)
    return "".join(out).rstrip(" ")

DATA_STRING_MAX = 24

def local_label_is_code(rom, name):
    if "." not in name:
        return False
    parent = name.split(".", 1)[0]
    try:
        p_off = rom.offset(parent)
        l_off = rom.offset(name)
    except Exception:
        return False
    if p_off is None or l_off is None or l_off <= p_off:
        return False
    try:
        _, end = G.decode_text(rom, p_off)
    except Exception:
        return False
    if l_off < end:
        return False

    if rom.data[l_off] < 0x18:
        return False

    end_of_run = -1
    for k in range(l_off, min(l_off + DATA_STRING_MAX, len(rom.data))):
        b = rom.data[k]
        if b == 0x50:
            end_of_run = k
            break
        if b < 0x18 or b not in G.charmap():
            return True
    return end_of_run < 0

def build(rom):
    out = {}
    for name in rom.sym:
        if not NAME_HINT.search(name) and name not in EXTRA_SYMBOLS:
            continue
        if local_label_is_code(rom, name):
            continue
        try:
            off = rom.offset(name)
            if off is None:
                continue
            txt, _ = G.decode_text(rom, off)

            full = decode_runs(rom, off, rom.sym[name][0])
            if full and len(full) > len(txt or ""):
                txt = full
        except Exception:
            continue
        if txt and (len(txt) >= MIN_LEN or name in SHORT_ALLOW):
            out[name] = txt

    for name in list(out):
        inner = name + ".Text"
        if inner in out:
            out[name] = out[inner]

    return sorted(out.items())

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")

    ap.add_argument("--out", default=None)
    ap.add_argument("--check", action="store_true",
                    help="report coverage against mod_runtime/scenes without "
                         "writing anything")
    a = ap.parse_args()

    dr, ds = default_paths(REPO)
    try:
        rom = Gen1Rom(a.rom or dr, a.sym or ds)
    except RomError as e:
        sys.exit("error: %s" % e)

    if a.out is None:
        import build_pak as BP
        a.out = os.path.join(REPO, "mod_runtime", "generatedmaps",
                             BP.ROM_PACKAGE_ID.get(rom.sha1, "red"),
                             "scene_text.tbl")

    rows = build(rom)
    if a.check:
        return check(rows)

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# GENERATED by tools/romimport/emit_scene_text.py from the "
                 "user's own ROM.\n# <symbol>\\t<text>. Referenced by "
                 "`say rom:<symbol>` in .scene files.\n")
        for name, txt in rows:
            fh.write("%s\t%s\n" % (name, escape(txt)))
    print("wrote %d strings to %s" % (len(rows), a.out))
    return 0

SAY = re.compile(r'^\s*say(?:_auto)?\s+"((?:[^"\\]|\\.)*)"', re.M)

def norm(s):
    s = (s.replace("\\n", "\n").replace("\\f", "\n").replace("\f", "\n")
          .replace("\\c", "\n").replace("\x0b", "\n"))
    return re.sub(r"\s+", " ", s.rstrip("@")).strip()

def check(rows):
    by_norm = {}
    for name, txt in rows:
        by_norm.setdefault(norm(txt), name)

    scenes = os.path.join(REPO, "mod_runtime", "scenes")
    lits = []
    for root, _d, files in os.walk(scenes):
        for f in files:
            if not f.endswith(".scene"):
                continue
            p = os.path.join(root, f)
            with open(p, encoding="utf-8", errors="replace") as fh:
                for m in SAY.finditer(fh.read()):
                    lits.append((os.path.relpath(p, scenes), m.group(1)))

    hit = [(p, s) for p, s in lits if norm(s) in by_norm]
    miss = [(p, s) for p, s in lits if norm(s) not in by_norm]
    print("ROM strings in table : %d" % len(rows))
    print("scene say-literals   : %d" % len(lits))
    print("resolvable to a symbol: %d (%.1f%%)"
          % (len(hit), 100.0 * len(hit) / max(1, len(lits))))
    print("not resolvable        : %d" % len(miss))
    for p, s in miss[:15]:
        print("    %-40s %s" % (p[:40], s[:56]))
    return 0

if __name__ == "__main__":
    sys.exit(main())
