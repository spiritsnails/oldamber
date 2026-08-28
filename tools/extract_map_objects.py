
import argparse
import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

HEADER_FIXED = 10
CONNECTION_SIZE = 11

TRAINER_FLAG = 1 << 6
ITEM_FLAG = 1 << 7

TX_ASM = 0x08
TX_ASM_MARKER = "<TX_ASM>"

def load_symbols(path):
    syms = {}
    pat = re.compile(r"^([0-9A-Fa-f]{2,}):([0-9A-Fa-f]{4})\s+(\S+)")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = pat.match(line.split(";", 1)[0].strip())
        if m:
            syms.setdefault(m.group(3), (int(m.group(1), 16), int(m.group(2), 16)))
    return syms

def flat(bank, addr):
    return addr if bank == 0 else bank * 0x4000 + (addr - 0x4000)

def real_map_headers(syms):
    return {v for k, v in syms.items() if k.endswith("_h")}

def map_object_addr(rom, syms, map_id):
    hp = flat(*syms["MapHeaderPointers"])
    hb = flat(*syms["MapHeaderBanks"])
    bank = rom[hb + map_id]
    addr = rom[hp + map_id * 2] | (rom[hp + map_id * 2 + 1] << 8)
    if not addr:
        return None
    h = flat(bank, addr)
    conn = rom[h + HEADER_FIXED - 1]
    n_conn = bin(conn & 0x0F).count("1")
    p = h + HEADER_FIXED + CONNECTION_SIZE * n_conn
    obj = rom[p] | (rom[p + 1] << 8)
    return bank, flat(bank, obj)

def map_text_pointers(rom, syms, map_id):
    hp = flat(*syms["MapHeaderPointers"])
    hb = flat(*syms["MapHeaderBanks"])
    bank = rom[hb + map_id]
    addr = rom[hp + map_id * 2] | (rom[hp + map_id * 2 + 1] << 8)
    if not addr:
        return None
    h = flat(bank, addr)
    tp = rom[h + 5] | (rom[h + 6] << 8)
    return bank, flat(bank, tp)

def object_text(rom, syms, map_id, text_id, decode):
    if not text_id:
        return None
    got = map_text_pointers(rom, syms, map_id)
    if not got:
        return None
    bank, tbl = got
    p = tbl + (text_id - 1) * 2
    if p + 1 >= len(rom):
        return None
    addr = rom[p] | (rom[p + 1] << 8)
    if not addr:
        return None
    f = flat(bank, addr)

    if rom[f] == TX_ASM:
        return TX_ASM_MARKER
    return decode(f)

def read_objects(rom, bank, addr):
    p = addr
    border = rom[p]; p += 1
    n = rom[p]; p += 1
    warps = []
    for _ in range(n):
        y, x, dw, dm = rom[p:p + 4]
        warps.append({"x": x, "y": y, "dest_warp": dw, "dest_map": dm})
        p += 4
    n = rom[p]; p += 1
    signs = []
    for _ in range(n):
        y, x, t = rom[p:p + 3]
        signs.append({"x": x, "y": y, "text_id": t})
        p += 3
    n_obj = rom[p]; p += 1
    objects = []
    for _ in range(n_obj):
        sprite, ry, rx, movement, rng = rom[p:p + 5]
        tid = rom[p + 5]

        o = {"sprite": sprite, "x": rx - 4, "y": ry - 4,
             "movement": movement, "range": rng,
             "text_id": tid & 0x3F, "kind": "npc"}
        if tid & TRAINER_FLAG:
            o["kind"] = "trainer"
            o["trainer_class"] = rom[p + 6]
            o["trainer_no"] = rom[p + 7]
            p += 8
        elif tid & ITEM_FLAG:
            o["kind"] = "item"
            o["item_id"] = rom[p + 6]
            p += 7
        else:
            p += 6
        objects.append(o)
    return {"border": border, "warps": warps, "signs": signs,
            "n_objects": n_obj, "objects": objects, "end": p}

def committed_warps():
    src = REPO / "src" / "data" / "event_data.c"
    if not src.exists():
        return {}
    txt = re.sub(r"/\*.*?\*/", "", src.read_text(encoding="utf-8", errors="replace"),
                 flags=re.S)
    out = {}
    for m in re.finditer(r"kWarps_(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};", txt, re.S):
        rows = re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)\s*\}",
                          m.group(2))
        out[m.group(1)] = [(int(a), int(b), c, int(d)) for a, b, c, d in rows]
    return out

def committed_map_names():
    src = REPO / "src" / "data" / "event_data.c"
    if not src.exists():
        return {}
    txt = re.sub(r"/\*.*?\*/", "", src.read_text(encoding="utf-8", errors="replace"),
                 flags=re.S)
    out = {}
    for m in re.finditer(r"\[\s*(0x[0-9A-Fa-f]+|\d+)\s*\]\s*=\s*\{\s*kWarps_(\w+)",
                         txt):
        out[int(m.group(1), 0)] = m.group(2)
    return out

EXPECTED_GATE_COLLAPSE = {
    "ViridianForestNorthGate",
    "Route2Gate",
    "ViridianForestSouthGate",
}

def collapse_top_door(warps):
    drop = set()
    for i, w in enumerate(warps):
        for j in range(i + 1, len(warps)):
            v = warps[j]
            if (w["y"] == 0 and v["y"] == 0 and abs(w["x"] - v["x"]) == 1
                    and w["dest_map"] == v["dest_map"]):
                drop.add(i if w["x"] < v["x"] else j)
    return [w for i, w in enumerate(warps) if i not in drop]

def committed_npcs():
    src = REPO / "src" / "data" / "event_data.c"
    if not src.exists():
        return {}
    txt = re.sub(r"/\*.*?\*/", "", src.read_text(encoding="utf-8", errors="replace"),
                 flags=re.S)
    out = {}
    for m in re.finditer(r"kNpcs_(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\};", txt, re.S):

        num = r"(0[xX][0-9a-fA-F]+|\d+)"
        rows = []
        for e in re.finditer(r"\{\s*" + r"\s*,\s*".join([num] * 4) + r"\s*,",
                             m.group(2)):
            rows.append(tuple(int(g, 0) for g in e.groups()))
        if rows:
            out[m.group(1)] = rows
    return out

def committed_signs():
    src = REPO / "src" / "data" / "event_data.c"
    if not src.exists():
        return {}
    txt = re.sub(r"/\*.*?\*/", "", src.read_text(encoding="utf-8", errors="replace"),
                 flags=re.S)
    out = {}
    for m in re.finditer(r"kSigns_(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\};", txt, re.S):
        rows = [(int(a, 0), int(b, 0)) for a, b in re.findall(
            r"\{\s*(0[xX][0-9a-fA-F]+|\d+)\s*,\s*(0[xX][0-9a-fA-F]+|\d+)\s*,",
            m.group(2))]
        if rows:
            out[m.group(1)] = rows
    return out

def verify_signs(rom, syms):
    names = committed_map_names()
    signs = committed_signs()
    if not names or not signs:
        print("SKIPPED: src/data/event_data.c is gone -- this verifier's oracle\n"
              "  is the file it replaces. Re-run against an earlier revision.")
        return 0
    real = real_map_headers(syms)
    hp = flat(*syms["MapHeaderPointers"])
    hb = flat(*syms["MapHeaderBanks"])

    seen, counts, diffs = set(), {"match": 0, "differ": 0, "skipped": 0}, []
    for mid, name in sorted(names.items()):
        if name in seen:
            continue
        hdr = (rom[hb + mid], rom[hp + mid * 2] | (rom[hp + mid * 2 + 1] << 8))
        if hdr not in real or name not in signs:
            counts["skipped"] += 1
            continue
        seen.add(name)
        got = map_object_addr(rom, syms, mid)
        if not got or got[1] >= len(rom):
            counts["skipped"] += 1
            continue
        d = read_objects(rom, got[0], got[1])
        mine = [(s["x"], s["y"]) for s in d["signs"]]
        want = signs[name]
        if mine == want:
            counts["match"] += 1
        else:
            counts["differ"] += 1
            if len(diffs) < 10:
                diffs.append((name, mine, want))
    for k in ("match", "differ", "skipped"):
        print(f"  {k:9} {counts[k]}")
    for n, a, b in diffs:
        print(f"  {n:24} rom={a[:5]}")
        print(f"  {'':24} com={b[:5]}")
    return 1 if counts["differ"] else 0

def port_movement(movement, rng):
    if movement == 0xFE:
        return {0x01: 3, 0x02: 2}.get(rng, 1)
    return 0

def verify_objects(rom, syms):
    names = committed_map_names()
    npcs = committed_npcs()
    if not names or not npcs:
        print("SKIPPED: src/data/event_data.c is gone -- this verifier's oracle\n"
              "  is the file it replaces. Re-run against an earlier revision.")
        return 0
    real = real_map_headers(syms)
    hp = flat(*syms["MapHeaderPointers"])
    hb = flat(*syms["MapHeaderBanks"])

    seen_name = set()
    counts = {"match": 0, "differ": 0, "count-differ": 0, "skipped": 0}
    diffs = []
    for mid, name in sorted(names.items()):
        if name in seen_name:
            continue
        hdr = (rom[hb + mid], rom[hp + mid * 2] | (rom[hp + mid * 2 + 1] << 8))
        if hdr not in real or name not in npcs:
            counts["skipped"] += 1
            continue
        seen_name.add(name)
        got = map_object_addr(rom, syms, mid)
        if not got or got[1] >= len(rom):
            counts["skipped"] += 1
            continue
        d = read_objects(rom, got[0], got[1])
        mine = [(o["x"], o["y"], o["sprite"],
                 port_movement(o["movement"], o["range"])) for o in d["objects"]]
        want = npcs[name]
        if len(mine) != len(want):
            counts["count-differ"] += 1
            diffs.append((name, f"rom {len(mine)} objects, committed {len(want)}"))
            continue
        bad = [i for i, (a, b) in enumerate(zip(mine, want)) if a != b]
        if bad:
            counts["differ"] += 1
            if len(diffs) < 10:
                i = bad[0]
                diffs.append((name, f"[{i}] rom={mine[i]} committed={want[i]} "
                                    f"({len(bad)} of {len(mine)} differ)"))
        else:
            counts["match"] += 1

    for k in ("match", "differ", "count-differ", "skipped"):
        print(f"  {k:13} {counts[k]}")
    for n, msg in diffs[:10]:
        print(f"  {n:26} {msg}")
    return 1 if counts["differ"] or counts["count-differ"] else 0

def verify_warps(rom, syms):
    names = committed_map_names()
    warps = committed_warps()
    if not names or not warps:
        print("SKIPPED: src/data/event_data.c is gone -- this verifier's oracle\n"
              "  is the file it replaces. Re-run against an earlier revision.")
        return 0

    shared = {n for n in set(names.values())
              if sum(1 for v in names.values() if v == n) > 1}

    real = real_map_headers(syms)
    hp = flat(*syms["MapHeaderPointers"])
    hb = flat(*syms["MapHeaderBanks"])

    counts = {"match": 0, "differ": 0, "expected": 0,
              "unused-slot": 0, "unreadable": 0}
    diffs, shared_rows = [], {}
    for mid, name in sorted(names.items()):
        want = warps.get(name)

        hdr = (rom[hb + mid], rom[hp + mid * 2] | (rom[hp + mid * 2 + 1] << 8))
        if hdr not in real:
            counts["unused-slot"] += 1
            continue
        got = map_object_addr(rom, syms, mid)
        if want is None or not got or got[1] >= len(rom):
            counts["unreadable"] += 1
            continue
        d = read_objects(rom, got[0], got[1])
        if len(d["warps"]) > 32:
            counts["unreadable"] += 1
            continue

        mine = [(w["x"], w["y"], w["dest_map"], w["dest_warp"])
                for w in d["warps"]]
        theirs = [(x, y, int(dm, 0) if dm.startswith("0x") else None, dw)
                  for x, y, dm, dw in want]

        eq = len(mine) == len(theirs) and all(
            a[0] == b[0] and a[1] == b[1] and a[3] == b[3]
            and (b[2] is None or a[2] == b[2])
            for a, b in zip(mine, theirs))
        if name in shared:
            shared_rows.setdefault(name, []).append((mid, eq))
        elif eq:
            counts["match"] += 1
        elif name in EXPECTED_GATE_COLLAPSE:
            counts["expected"] += 1
        else:
            counts["differ"] += 1
            diffs.append((mid, name, mine, theirs))

    print("uniquely-assigned ids")
    for k in ("match", "expected", "differ", "unused-slot", "unreadable"):
        print(f"  {k:11} {counts[k]}")
    amb = [n for n, r in shared_rows.items() if sum(1 for _, e in r if e) != 1]
    print(f"shared arrays {len(shared_rows)}, "
          f"resolved {len(shared_rows) - len(amb)}, ambiguous {len(amb)}")
    for mid, name, mine, theirs in diffs[:12]:
        print(f"\n  [{mid:#04x}] {name}  rom {len(mine)}, committed {len(theirs)}")
        for i, (a, b) in enumerate(zip(mine, theirs)):
            if a[0] != b[0] or a[1] != b[1] or a[3] != b[3] \
               or (b[2] is not None and a[2] != b[2]):
                print(f"      [{i}] rom={a} committed={b}")
    return 1 if counts["differ"] else 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", type=Path,
                    default=REPO / "pokered-master" / "pokered.gbc")
    ap.add_argument("--sym", type=Path, default=None)
    ap.add_argument("--verify-warps", action="store_true")
    ap.add_argument("--verify-objects", action="store_true")
    ap.add_argument("--verify-signs", action="store_true")
    ap.add_argument("--map-id", type=int)
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    syms = load_symbols(args.sym or args.rom.with_suffix(".sym"))

    if args.verify_warps:
        raise SystemExit(verify_warps(rom, syms))
    if args.verify_objects:
        raise SystemExit(verify_objects(rom, syms))
    if args.verify_signs:
        raise SystemExit(verify_signs(rom, syms))

    ids = [args.map_id] if args.map_id is not None else range(248)
    total_w = total_s = total_o = maps = 0
    for mid in ids:
        try:
            got = map_object_addr(rom, syms, mid)
        except (KeyError, IndexError):
            continue
        if not got:
            continue
        bank, addr = got
        if addr >= len(rom):
            continue
        d = read_objects(rom, bank, addr)
        if len(d["warps"]) > 32 or len(d["signs"]) > 32 or d["n_objects"] > 32:
            continue
        maps += 1
        total_w += len(d["warps"])
        total_s += len(d["signs"])
        total_o += d["n_objects"]
        if args.map_id is not None:
            print(f"map {mid}: border={d['border']} "
                  f"warps={len(d['warps'])} signs={len(d['signs'])} "
                  f"objects={d['n_objects']}")
            for w in d["warps"]:
                print(f"   warp x={w['x']:3d} y={w['y']:3d} "
                      f"-> map {w['dest_map']:3d} warp {w['dest_warp']}")
    print(f"\n{maps} maps decoded: {total_w} warps, {total_s} signs, "
          f"{total_o} objects")

if __name__ == "__main__":
    main()
