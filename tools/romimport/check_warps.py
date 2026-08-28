
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))

import crystal_maps as M
from crystal_rom import Rom

PC = os.path.join(ROOT, "pokecrystal-master")
BLOCKS = os.path.join(ROOT, "mod_runtime", "generatedmaps", "johto", "blocks")

def collision_constants():
    out = {}
    path = os.path.join(PC, "constants", "collision_constants.asm")
    for line in open(path, encoding="utf-8"):
        m = re.match(r"\s*DEF\s+(\w+)\s+EQU\s+\$([0-9A-Fa-f]+)", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out

def warp_rules():
    consts = collision_constants()
    src = open(os.path.join(PC, "engine", "overworld", "tile_events.asm"),
               encoding="utf-8").read()

    def body(label):
        m = re.search(rf"^{label}::?\s*\n(.*?)(?=^\w+::?\s*$)", src, re.M | re.S)
        return m.group(1) if m else ""

    def ids(text, pattern):
        out = {}
        for name in re.findall(pattern, text):
            if name in consts:
                out[consts[name]] = name
        return out

    coll_body = body("CheckWarpCollision")
    dir_body = body("CheckDirectionalWarp")
    face_body = body("CheckWarpFacingDown")
    return {

        "pits": ids(coll_body, r"cp\s+(COLL_\w+)"),
        "directional": ids(dir_body, r"cp\s+(COLL_\w+)"),
        "facing_down": ids(face_body, r"db\s+(COLL_\w+)"),
        "hi_nybble": consts["HI_NYBBLE_WARPS"],
        "consts": consts,
    }

CARPET_DIR = {"COLL_WARP_CARPET_DOWN": "down", "COLL_WARP_CARPET_UP": "up",
              "COLL_WARP_CARPET_LEFT": "left", "COLL_WARP_CARPET_RIGHT": "right"}

def required_directive(coll_name, rules):
    if coll_name in CARPET_DIR:
        return ("warp_walk_into", CARPET_DIR[coll_name])
    return ("warp_stair", None)

MANIFEST = {
    "header": {
        "attributes": "emitted: drives everything below",
        "tileset": "emitted: art/ + subtile bank",
        "environment": "emitted: `indoor`",
        "landmark": "DEFERRED: town-map region names, no port equivalent yet",
        "music": "DEFERRED: user's call -- ROM-derived audio is its own project",
        "phone_service": "DEFERRED: no phone system in the port",
        "time_of_day": "DEFERRED: no time-of-day system in the port",
        "fishing_group": "DEFERRED: fishing encounter tables not imported yet",
    },
    "attributes": {
        "border_block": "emitted: `border`",
        "height": "emitted: `mapsize`",
        "width": "emitted: `mapsize`",
        "blocks": "emitted: the cell grid",
        "scripts": "emitted: script bank for dialogue (crystal_script.py)",
        "events": "emitted: warps / objects / bg / coord events",
        "connection_mask": "emitted: implied by the `connect` lines",
        "connections": "emitted: `connect` (see check_connections.py)",
    },
    "connection": {
        "dir": "emitted: `connect`", "group": "emitted: resolves `dest`",
        "map": "emitted: resolves `dest`", "dest": "emitted: `connect`",
        "offset": "emitted: `connect` adjust",
        "y": "emitted: `connect` player_coord", "x": "emitted: `connect` player_coord",
        "src_blocks_ptr": "DEFERRED: source-side strip pointer, port recomputes",
        "dest_offset": "DEFERRED: destination buffer offset, port recomputes",
        "strip_len": "DEFERRED: port peeks against the neighbour's real bounds",
        "dest_width": "DEFERRED: port reads the neighbour's own width",
        "window": "DEFERRED: ROM tilemap window pointer, meaningless here",
    },
    "warp": {
        "y": "emitted: `warpspot` + warp quad",
        "x": "emitted: `warpspot` + warp quad",
        "dest_warp": "emitted: the quad's `warp <dest> <idx>`",
        "group": "emitted: resolves the destination name",
        "map": "emitted: resolves the destination name",
    },
    "object": {
        "sprite": "emitted: npc/trainer sprite",
        "y": "emitted", "x": "emitted",
        "movement": "emitted: npc movement vocabulary",
        "type": "emitted: chooses npc / item_ball / johto_trainer",
        "sight_range": "emitted: trainer sight distance",
        "script": "emitted: dialogue (crystal_script.py)",
        "radius_y": "DEFERRED: wander radius, DSL has no bounded-wander verb",
        "radius_x": "DEFERRED: wander radius, DSL has no bounded-wander verb",
        "hour1": "DEFERRED: time-of-day visibility, no port equivalent",
        "hour2": "DEFERRED: time-of-day visibility, no port equivalent",
        "palette": "DEFERRED: per-object GBC palette not wired for vmap NPCs",
        "event_flag": "DEFERRED: indexes CRYSTAL's flag space, deliberately "
                      "not passed through (see emit_map's trainer comment)",
    },
    "coord_event": {
        "scene": "DEFERRED: whole coord_event system unimported",
        "y": "DEFERRED", "x": "DEFERRED",
        "script": "DEFERRED: needs the scene language",
    },
    "bg_event": {
        "y": "DEFERRED: emitted only as a `; sign` comment",
        "x": "DEFERRED: emitted only as a `; sign` comment",
        "function": "DEFERRED: BGEVENT_* kind not mapped to a port directive",
        "script": "DEFERRED: sign text needs the same reader as NPC dialogue",
    },
}

def check_fields(rom, table, names):
    seen = collections.defaultdict(set)
    for name in names[:40]:
        g, m = table[1][name]
        hdr = M.read_map_header(rom, g, m)
        seen["header"].update(hdr)
        attr = M.read_attributes(rom, *hdr["attributes"], table[0])
        seen["attributes"].update(attr)
        for c in attr["connections"]:
            seen["connection"].update(c)
        ev = M.read_events(rom, *attr["events"])
        for w in ev["warps"]:
            seen["warp"].update(w)
        for o in ev["object_events"]:
            seen["object"].update(o)
        for c in ev["coord_events"]:
            seen["coord_event"].update(c)
        for b in ev["bg_events"]:
            seen["bg_event"].update(b)
    problems = []
    deferred = 0
    for group, keys in sorted(seen.items()):
        known = MANIFEST.get(group, {})
        for k in sorted(keys):
            if k in ("end",):
                continue
            if k not in known:
                problems.append(f"{group}.{k} is read from the ROM but is not "
                                f"classified in check_warps.MANIFEST")
            elif known[k].startswith("DEFERRED"):
                deferred += 1
    return problems, deferred, seen

WALK_INTO = re.compile(r"^warp_walk_into \S+ (\d+) (\d+)(?: (\w+))?\s*$", re.M)
STAIR = re.compile(r"^warp_stair \S+ (\d+) (\d+)\s*$", re.M)

def check(list_all=False):
    rom = Rom(os.path.join(PC, "pokecrystal.gbc"),
              os.path.join(PC, "pokecrystal.sym"))
    table = M.build_map_table(rom)
    rules = warp_rules()
    names = sorted({os.path.splitext(f)[0] for f in os.listdir(BLOCKS)
                    if f.endswith(".block")} & set(table[1]))

    print("")
    print(f"  directional (must press their own direction): "
          f"{', '.join(sorted(rules['directional'].values()))}")
    print(f"  spawn facing down on arrival: "
          f"{', '.join(sorted(rules['facing_down'].values()))}")
    print(f"  pits: {', '.join(sorted(rules['pits'].values()))}")

    by_kind = collections.Counter()
    missing = collections.Counter()
    offenders = []
    for name in names:
        g, m = table[1][name]
        hdr = M.read_map_header(rom, g, m)
        attr = M.read_attributes(rom, *hdr["attributes"], table[0])
        ts = M.read_tileset_entry(rom, hdr["tileset"])
        nmeta = M.metatile_count(rom, ts)
        coll = M.read_collision(rom, *ts["coll"], count=nmeta)
        blocks = M.read_blocks(rom, *attr["blocks"], attr["width"], attr["height"])
        ev = M.read_events(rom, *attr["events"])
        W, H = attr["width"] * 2, attr["height"] * 2

        path = os.path.join(BLOCKS, name + ".block")
        txt = open(path, encoding="utf-8", errors="replace").read()
        have_walk = {(int(a), int(b)): c for a, b, c in WALK_INTO.findall(txt)}
        have_stair = {(int(a), int(b)) for a, b in STAIR.findall(txt)}

        for i, w in enumerate(ev["warps"]):
            x, y = w["x"], w["y"]
            if not (0 <= x < W and 0 <= y < H):
                continue
            cid = M.cell_collision(blocks, coll, attr["width"], x, y)
            cname = rules["consts"] and next(
                (n for n, v in rules["consts"].items()
                 if v == cid and n.startswith("COLL_")), None)
            is_warp = cid in rules["pits"] or (cid & 0xF0) == rules["hi_nybble"]
            if not is_warp:

                by_kind["arrival-only (not a warp tile)"] += 1
                continue
            directive, arg = required_directive(cname, rules)
            by_kind[cname or f"${cid:02X}"] += 1
            ok = ((x, y) in have_walk and (arg is None or have_walk[(x, y)] == arg)
                  if directive == "warp_walk_into" else (x, y) in have_stair)
            if not ok:
                missing[f"{directive}" + (f" {arg}" if arg else "")] += 1
                offenders.append((name, i, x, y, cname, directive, arg))

    total = sum(by_kind.values())
    print(f"\nwarp events across {len(names)} maps: {total}")
    for k, v in by_kind.most_common():
        print(f"  {v:5d}  {k}")
    print(f"\nwarps whose ROM trigger class the emitted map does NOT state: "
          f"{sum(missing.values())}")
    for k, v in missing.most_common():
        print(f"  {v:5d}  needs `{k}`")
    if list_all:
        for name, i, x, y, cname, d, a in offenders:
            print(f"    {name} warp {i} at ({x},{y}) {cname} -> {d} {a or ''}")

    problems, deferred, seen = check_fields(rom, table, names)
    print(f"\nROM fields read: {sum(len(v) for v in seen.values())}, "
          f"of which {deferred} are classified DEFERRED")
    for p in problems:
        print(f"  !! {p}")

    ok = not problems and not missing
    print(f"\n{'PASSED' if ok else 'FAILED'}")
    return ok

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true",
                    help="print every warp whose class is not stated")
    args = ap.parse_args()
    return 0 if check(args.list) else 1

if __name__ == "__main__":
    sys.exit(main())
