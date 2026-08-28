
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from crystal_rom import BANK_SIZE

SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "pokecrystal-master")

CONN_NORTH, CONN_SOUTH, CONN_WEST, CONN_EAST = 0x08, 0x04, 0x02, 0x01
CONN_ORDER = (("north", CONN_NORTH), ("south", CONN_SOUTH),
              ("west", CONN_WEST), ("east", CONN_EAST))

MAP_HEADER_SIZE = 9
TILESET_ENTRY_SIZE = 15
METATILE_SIZE = 16
NUM_METATILES = 128
TILE_SIZE = 16

def group_pointers(rom):
    bank, addr = rom.addr_of("MapGroupPointers")
    end = rom.next_symbol_addr(bank, addr)
    if end is None:
        raise RuntimeError("cannot size MapGroupPointers")
    count = (end - addr) // 2
    return [(bank, rom.u16(bank, addr + i * 2)) for i in range(count)]

GSC_KANTO_PREFIX = "Gsc"

def kanto_landmark(src_root):
    path = os.path.join(src_root, "constants", "landmark_constants.asm")
    n = 0
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.split(";")[0]
        if re.match(r"\s*DEF\s+KANTO_LANDMARK\s+EQU\s+const_value\s*$", line):
            return n
        m = re.match(r"\s*const_def\s*(-?\d+)?", line)
        if m:
            n = int(m.group(1)) if m.group(1) else 0
            continue
        if re.match(r"\s*const\s+\w+", line):
            n += 1
        elif re.match(r"\s*const_skip\s*(\d+)?", line):
            m2 = re.match(r"\s*const_skip\s*(\d+)?", line)
            n += int(m2.group(1)) if m2.group(1) else 1
    raise ValueError("")

def build_map_table(rom, src_root=None):
    by_id, by_name = {}, {}
    kanto_lm = kanto_landmark(src_root or SRC)
    groups = group_pointers(rom)

    starts = sorted({a for (_b, a) in groups})
    for gi, (bank, gaddr) in enumerate(groups, start=1):
        after = [a for a in starts if a > gaddr]
        limit = after[0] if after else (rom.next_symbol_addr(bank, gaddr)
                                        or BANK_SIZE * 2)
        mi = 0
        while True:
            hdr_addr = gaddr + mi * MAP_HEADER_SIZE
            if hdr_addr + MAP_HEADER_SIZE > limit:
                break
            attr_bank = rom.u8(bank, hdr_addr)
            attr_addr = rom.u16(bank, hdr_addr + 3)
            names = [n for n in rom.names_at(attr_bank, attr_addr)
                     if n.endswith("_MapAttributes")]
            if not names:
                break
            name = sorted(names)[0][:-len("_MapAttributes")]

            if rom.u8(bank, hdr_addr + 5) >= kanto_lm:
                name = GSC_KANTO_PREFIX + name
            by_id[(gi, mi + 1)] = name
            by_name[name] = (gi, mi + 1)
            mi += 1
    return by_id, by_name

def read_map_header(rom, group, mapno):
    bank, gaddr = group_pointers(rom)[group - 1]
    a = gaddr + (mapno - 1) * MAP_HEADER_SIZE
    phone_tod = rom.u8(bank, a + 7)
    return {
        "attributes": (rom.u8(bank, a), rom.u16(bank, a + 3)),
        "tileset": rom.u8(bank, a + 1),
        "environment": rom.u8(bank, a + 2),
        "landmark": rom.u8(bank, a + 5),
        "music": rom.u8(bank, a + 6),

        "phone_service": phone_tod >> 4,
        "time_of_day": phone_tod & 0x0F,
        "fishing_group": rom.u8(bank, a + 8),
    }

def read_attributes(rom, attr_bank, attr_addr, id_to_name):
    a = attr_addr
    out = {
        "border_block": rom.u8(attr_bank, a),
        "height": rom.u8(attr_bank, a + 1),
        "width": rom.u8(attr_bank, a + 2),
        "blocks": (rom.u8(attr_bank, a + 3), rom.u16(attr_bank, a + 4)),
        "scripts": (rom.u8(attr_bank, a + 6), rom.u16(attr_bank, a + 7)),

        "events": (rom.u8(attr_bank, a + 6), rom.u16(attr_bank, a + 9)),
        "connection_mask": rom.u8(attr_bank, a + 11),
        "connections": [],
    }
    c = a + 12
    for name, bit in CONN_ORDER:
        if not (out["connection_mask"] & bit):
            continue
        g, m = rom.u8(attr_bank, c), rom.u8(attr_bank, c + 1)
        cy, cx = rom.u8(attr_bank, c + 8), rom.u8(attr_bank, c + 9)

        free = cx if name in ("north", "south") else cy
        offset = -((free - 256 if free >= 128 else free) // 2)
        out["connections"].append({
            "dir": name,
            "group": g, "map": m,
            "dest": id_to_name.get((g, m)),
            "src_blocks_ptr": rom.u16(attr_bank, c + 2),
            "dest_offset": rom.u16(attr_bank, c + 4),
            "strip_len": rom.u8(attr_bank, c + 6),
            "dest_width": rom.u8(attr_bank, c + 7),
            "y": cy,
            "x": cx,
            "offset": offset,
            "window": rom.u16(attr_bank, c + 10),
        })
        c += 12
    return out

PALMAP_BANK0_TILES = 96
PALMAP_GAP_TILES = 32

def gfx_index_to_vram(i):
    return i if i < PALMAP_BANK0_TILES else i + PALMAP_GAP_TILES

def read_tileset_palmap(rom, bank, addr, ngfx):
    out = []
    for i in range(ngfx):
        v = gfx_index_to_vram(i)
        byte = rom.u8(bank, addr + v // 2)
        nib = (byte & 0x0F) if (v % 2 == 0) else (byte >> 4)
        out.append(nib & 0x07)
    return out

def anim_bank(rom):
    return rom.sym["Tileset0Anim"][0]

def read_tileset_anim(rom, bank, addr, limit=64):
    out = []
    for _ in range(limit):
        arg = rom.u16(bank, addr)
        fn = rom.u16(bank, addr + 2)
        names = rom.names_at(bank, fn)
        name = names[0] if names else f"${fn:04X}"
        out.append((name, arg))
        addr += 4
        if name == "DoneTileAnimation":
            break
    return out

def sprite_default_palette(rom, sprite_id):
    bank, addr = rom.sym["OverworldSprites"]
    if sprite_id < 1:
        return None
    return rom.u8(bank, addr + (sprite_id - 1) * 6 + 5) & 0x07

def read_blocks(rom, blocks_bank, blocks_addr, width, height):
    return rom.read(blocks_bank, blocks_addr, width * height)

def read_events(rom, ev_bank, ev_addr):
    a = ev_addr + 2
    out = {}

    n = rom.u8(ev_bank, a); a += 1
    warps = []
    for _ in range(n):
        warps.append({
            "y": rom.u8(ev_bank, a),
            "x": rom.u8(ev_bank, a + 1),
            "dest_warp": rom.u8(ev_bank, a + 2),
            "group": rom.u8(ev_bank, a + 3),
            "map": rom.u8(ev_bank, a + 4),
        })
        a += 5
    out["warps"] = warps

    n = rom.u8(ev_bank, a); a += 1
    coords = []
    for _ in range(n):
        coords.append({
            "scene": rom.u8(ev_bank, a),
            "y": rom.u8(ev_bank, a + 1),
            "x": rom.u8(ev_bank, a + 2),
            "script": rom.u16(ev_bank, a + 4),
        })
        a += 8
    out["coord_events"] = coords

    n = rom.u8(ev_bank, a); a += 1
    bgs = []
    for _ in range(n):
        bgs.append({
            "y": rom.u8(ev_bank, a),
            "x": rom.u8(ev_bank, a + 1),
            "function": rom.u8(ev_bank, a + 2),
            "script": rom.u16(ev_bank, a + 3),
        })
        a += 5
    out["bg_events"] = bgs

    n = rom.u8(ev_bank, a); a += 1
    objs = []
    for _ in range(n):
        radius = rom.u8(ev_bank, a + 4)
        pal_type = rom.u8(ev_bank, a + 7)
        objs.append({
            "sprite": rom.u8(ev_bank, a),

            "y": rom.u8(ev_bank, a + 1) - 4,
            "x": rom.u8(ev_bank, a + 2) - 4,
            "movement": rom.u8(ev_bank, a + 3),
            "radius_y": radius >> 4,
            "radius_x": radius & 0x0F,
            "hour1": rom.u8(ev_bank, a + 5),
            "hour2": rom.u8(ev_bank, a + 6),
            "palette": pal_type >> 4,
            "type": pal_type & 0x0F,
            "sight_range": rom.u8(ev_bank, a + 8),
            "script": rom.u16(ev_bank, a + 9),
            "event_flag": rom.u16(ev_bank, a + 11),
        })
        a += 13
    out["object_events"] = objs
    out["end"] = a
    return out

def read_tileset_entry(rom, tileset_id):
    bank, addr = rom.addr_of("Tilesets")
    a = addr + tileset_id * TILESET_ENTRY_SIZE
    return {
        "gfx": rom.dba(bank, a),
        "meta": rom.dba(bank, a + 3),
        "coll": rom.dba(bank, a + 6),

        "anim": (anim_bank(rom), rom.u16(bank, a + 9)),

        "palmap": (bank, rom.u16(bank, a + 13)),

        "palmap": (bank, rom.u16(bank, a + 13)),
    }

PIT_COLL = (0x60, 0x68)
HI_NYBBLE_WARPS = 0x70
CARPET_DIR = {0x70: "down", 0x76: "left", 0x78: "up", 0x7E: "right"}
IMMEDIATE_NAME = {0x71: "door", 0x72: "ladder", 0x7A: "staircase",
                  0x7B: "cave", 0x7C: "panel"}

def warp_kind(coll_id):
    if coll_id in PIT_COLL:
        return "pit", None
    if (coll_id & 0xF0) == HI_NYBBLE_WARPS:
        if coll_id in CARPET_DIR:
            return "carpet", CARPET_DIR[coll_id]
        return "immediate", IMMEDIATE_NAME.get(coll_id, f"warp_{coll_id:02X}")
    return None, None

HI_NYBBLE_LEDGES = 0xA0
LEDGE_TABLE = (
    frozenset({"right"}),
    frozenset({"left"}),
    frozenset({"up"}),
    frozenset({"down"}),
    frozenset({"right", "down"}),
    frozenset({"down", "left"}),
    frozenset({"up", "right"}),
    frozenset({"up", "left"}),
)

LEDGE_DIR_ORDER = ("down", "up", "left", "right")

def ledge_dirs(coll_id):
    if (coll_id & 0xF0) != HI_NYBBLE_LEDGES:
        return None
    mask = LEDGE_TABLE[coll_id & 0x07]
    return tuple(d for d in LEDGE_DIR_ORDER if d in mask)

LO_NYBBLE_GRASS = 0x07
HI_NYBBLE_TALL_GRASS = 0x10
HI_NYBBLE_WATER = 0x20
COLL_LONG_GRASS = 0x14
COLL_LONG_GRASS_1C = 0x1C

def grass_encounter_set(rom):
    bank, addr = rom.sym["CheckGrassCollision.blocks"]
    out = set()
    while True:
        b = rom.u8(bank, addr)
        if b == 0xFF:
            return out
        out.add(b)
        addr += 1

def grass_rustles(coll_id):
    if coll_id in (COLL_LONG_GRASS, COLL_LONG_GRASS_1C):
        return True
    if (coll_id & 0xF0) in (HI_NYBBLE_TALL_GRASS, HI_NYBBLE_WATER):
        return (coll_id & LO_NYBBLE_GRASS) == 0
    return False

def cut_collision_set(rom):
    bank, addr = rom.sym["CheckCutCollision.blocks"]
    out = set()
    while True:
        b = rom.u8(bank, addr)
        if b == 0xFF:
            return out
        out.add(b)
        addr += 1

def cut_tree_blocks(rom):
    bank, addr = rom.addr_of("CutTreeBlockPointers")
    out = {}
    while rom.u8(bank, addr) != 0xFF:
        ts, ptr = rom.u8(bank, addr), rom.u16(bank, addr + 1)
        rows, p = {}, ptr
        while rom.u8(bank, p) != 0xFF:
            rows[rom.u8(bank, p)] = (rom.u8(bank, p + 1), rom.u8(bank, p + 2))
            p += 3
        out[ts] = rows
        addr += 3
    return out

def cell_collision(blocks, coll, width, x, y):
    mi = blocks[(y // 2) * width + (x // 2)]
    quad = {(0, 0): 0, (1, 0): 1, (0, 1): 2, (1, 1): 3}[(x % 2, y % 2)]
    return coll[mi][quad]

def const_table(path, prefix=""):
    out, n = {}, 0
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.split(";")[0]
        m = re.match(r"\s*const_def\s*(-?\d+)?", line)
        if m:
            n = int(m.group(1)) if m.group(1) else 0
            continue
        m = re.match(r"\s*const_value\s*=\s*(-?\d+)", line)
        if m:
            n = int(m.group(1))
            continue

        m = re.match(r"\s*const_next\s+\$?([0-9A-Fa-f]+)", line)
        if m:
            tok = m.group(1)
            n = int(tok, 16) if "$" in line else int(tok)
            continue
        m = re.match(r"\s*const_skip\s*(\d+)?", line)
        if m:
            n += int(m.group(1) or 1)
            continue
        m = re.match(r"\s*const\s+(\w+)", line)
        if m:
            if not prefix or m.group(1).startswith(prefix):
                out[m.group(1)] = n
            n += 1
    return out

def trainer_class_names(disasm):
    out = []
    with open(os.path.join(disasm, "constants", "trainer_constants.asm"),
              encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.match(r"\s*trainerclass\s+(\w+)", line.split(";")[0])
            if m:
                out.append(m.group(1))
    return out

def tileset_name(rom, entry):
    def stems(ptr, suffix):
        out = set()
        for n in rom.names_at(*ptr):
            if n.startswith("Tileset") and n.endswith(suffix):
                out.add(n[len("Tileset"):-len(suffix)])
        return out

    cand = (stems(entry["meta"], "Meta")
            & stems(entry["coll"], "Coll")
            & stems(entry["gfx"], "GFX"))
    cand = {c for c in cand if not c.isdigit()}
    if len(cand) == 1:
        return cand.pop()
    return None

def metatile_count(rom, entry):
    mbank, maddr = entry["meta"]
    cbank, caddr = entry["coll"]
    if cbank == mbank and caddr > maddr:
        return (caddr - maddr) // METATILE_SIZE
    end = rom.next_symbol_addr(mbank, maddr)
    if end is None:
        return NUM_METATILES
    return (end - maddr) // METATILE_SIZE

def read_metatiles(rom, meta_bank, meta_addr, count=NUM_METATILES):
    raw = rom.read(meta_bank, meta_addr, count * METATILE_SIZE)
    return [raw[i * METATILE_SIZE:(i + 1) * METATILE_SIZE] for i in range(count)]

def read_collision(rom, coll_bank, coll_addr, count=NUM_METATILES):
    raw = rom.read(coll_bank, coll_addr, count * 4)
    return [tuple(raw[i * 4:(i + 1) * 4]) for i in range(count)]

def read_tileset_gfx(rom, gfx_bank, gfx_addr, limit=0x1000):
    raw = rom.decompress(gfx_bank, gfx_addr, limit=limit)
    return [raw[i:i + TILE_SIZE] for i in range(0, len(raw), TILE_SIZE)]

def read_palette_map(rom, pal_bank, pal_addr, count=NUM_METATILES):
    raw = rom.read(pal_bank, pal_addr, count * 2)
    out = []
    for i in range(count):
        b0, b1 = raw[i * 2], raw[i * 2 + 1]
        out.append((b0 >> 4, b0 & 0x0F, b1 >> 4, b1 & 0x0F))
    return out
