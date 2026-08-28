
import argparse
import collections
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

REPO = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, "tools", "assetpack"))
sys.path.insert(0, HERE)

from gen1_rom import Gen1Rom, RomError
import gen1_script as G
import port_overrides as PO
import build_pak as BP

OUT = os.path.join(REPO, "generated", "romimport", "kanto")

NAME_MAX = 31

BIT_TRAINER = 0x40
BIT_ITEM = 0x80

MOVEMENT = {0xFE: "walk_random", 0xFF: "stay"}
RANGE_MOVEMENT = {0x00: "walk_random", 0x01: "walk_up_down", 0x02: "walk_left_right"}
FACING = {0xD0: "down", 0xD1: "up", 0xD2: "left", 0xD3: "right"}

_SPRITE_NAMES = None

def sprite_names():
    global _SPRITE_NAMES
    if _SPRITE_NAMES is None:
        path = os.path.join(REPO, "src", "data", "sprite_names_gen.h")
        txt = open(path, encoding="utf-8", errors="replace").read()

        _SPRITE_NAMES = {int(i, 0): n for n, i in
                         re.findall(r'\{\s*"([^"]+)"\s*,\s*(0[xX][0-9a-fA-F]+|\d+)\s*\}',
                                    txt)}
    return _SPRITE_NAMES

def map_header(rom, map_id):
    ptrs = rom.offset("MapHeaderPointers")
    banks = rom.offset("MapHeaderBanks")
    bank = rom.data[banks + map_id]
    addr = rom.data[ptrs + map_id * 2] | (rom.data[ptrs + map_id * 2 + 1] << 8)
    off = addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)
    return bank, off

def object_offset(rom, bank, hdr):
    n = bin(rom.data[hdr + 9]).count("1")
    p = hdr + 10 + 11 * n
    a = rom.data[p] | (rom.data[p + 1] << 8)
    return a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)

def text_at(rom, bank, hdr, text_id):
    if text_id <= 0:
        return None
    a = rom.data[hdr + 5] | (rom.data[hdr + 6] << 8)
    tbl = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
    p = tbl + (text_id - 1) * 2
    ta = rom.data[p] | (rom.data[p + 1] << 8)
    if ta == 0:
        return None
    off = ta if ta < 0x4000 else bank * 0x4000 + (ta - 0x4000)
    if is_service_marker(rom.data[off]):
        return None
    try:
        txt, _ = G.decode_text(rom, off)
    except (IndexError, RecursionError):
        return None
    return txt

def is_service_marker(b):
    return 0xF0 <= b <= 0xFF

SERVICE_BY_SPRITE = {"NURSE": "service:heal"}

CLERK_MART_SERVICE = {
    "ViridianMart": "service:viridian_mart",
    "PewterMart": "service:pewter_mart",
    "CeruleanMart": "service:cerulean_mart",
    "VermilionMart": "service:vermilion_mart",
    "LavenderMart": "service:lavender_mart",
    "FuchsiaMart": "service:fuchsia_mart",
    "CinnabarMart": "service:cinnabar_mart",
    "SaffronMart": "service:saffron_mart",
    "IndigoPlateauLobby": "service:indigo_mart",
}

CELADON_CLERK_SERVICE = {
    ("CeladonMart2F", 0): "service:celadon_mart_2f_1",
    ("CeladonMart2F", 1): "service:celadon_mart_2f_2",
    ("CeladonMart4F", 0): "service:celadon_mart_4f",
    ("CeladonMart5F", 0): "service:celadon_mart_5f_1",
    ("CeladonMart5F", 1): "service:celadon_mart_5f_2",
}

SCRIPT_NPC_SERVICE = {
    ("NameRatersHouse", "SILPH_PRESIDENT", 5, 3): "service:name_rater",
    ("Daycare", "GENTLEMAN", 2, 3): "service:daycare_man",
}

def clerk_mart_service(map_name, clerk_index):
    svc = CLERK_MART_SERVICE.get(map_name)
    if svc is not None:
        return svc
    return CELADON_CLERK_SERVICE.get((map_name, clerk_index))

def service_text(rom, bank, hdr, text_id, sprite_name):
    if text_id <= 0:
        return None
    a = rom.data[hdr + 5] | (rom.data[hdr + 6] << 8)
    tbl = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
    p = tbl + (text_id - 1) * 2
    ta = rom.data[p] | (rom.data[p + 1] << 8)
    if not ta:
        return None
    off = ta if ta < 0x4000 else bank * 0x4000 + (ta - 0x4000)
    if not is_service_marker(rom.data[off]):
        return None
    return SERVICE_BY_SPRITE.get(sprite_name)

def script_plain_text(rom, bank, hdr, text_id):
    off = entry_offset(rom, bank, hdr, text_id)
    return script_words_at(rom, bank, off, text_id)

def script_words_at(rom, bank, off, text_id):
    if off is None or rom.data[off] != 0x08:
        return None, None
    off, facing, pending = resolve_static_gates(rom, bank, off + 1, text_id)

    op = rom.data[off]
    if op == 0x21:
        load = rom.data[off + 1] | (rom.data[off + 2] << 8)
    elif (op == 0x3E and "PrintPredefTextID" in rom.sym
            and rom.data[off + 2] in (0xCD, 0xC3)
            and rom.data[off + 3] == (rom.sym["PrintPredefTextID"][1] & 0xFF)
            and rom.data[off + 4] == ((rom.sym["PrintPredefTextID"][1] >> 8) & 0xFF)):
        return predef_text(rom, rom.data[off + 1], bank), facing
    elif op in (0xCD, 0xC3) and pending:
        load = pending
    else:
        return None, facing
    if not load:
        return None, facing
    o = load if load < 0x4000 else bank * 0x4000 + (load - 0x4000)
    try:
        txt, _ = G.decode_text(rom, o)
    except (IndexError, RecursionError, TypeError):
        return None, facing
    return txt, facing

def entry_offset(rom, bank, hdr, text_id):
    if text_id <= 0:
        return None
    a = rom.data[hdr + 5] | (rom.data[hdr + 6] << 8)
    tbl = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
    p = tbl + (text_id - 1) * 2
    ta = rom.data[p] | (rom.data[p + 1] << 8)
    if not ta:
        return None
    return ta if ta < 0x4000 else bank * 0x4000 + (ta - 0x4000)

FACING_WORD = {0x00: "down", 0x04: "up", 0x08: "left", 0x0C: "right"}

def resolve_static_gates(rom, bank, off, text_id):
    facing = pending = None
    fa = rom.sym["wSpriteStateData1"][1] + 9 if "wSpriteStateData1" in rom.sym else None
    for _ in range(8):
        i = off

        if rom.data[i] == 0x21:
            pending = rom.data[i + 1] | (rom.data[i + 2] << 8)
            i += 3
        op = rom.data[i]
        if op == 0xFA and fa is not None:
            src = rom.data[i + 1] | (rom.data[i + 2] << 8)
            if src != fa:
                break
            want, i = None, i + 3
        elif op == 0xF0:
            src, i = 0xFF00 | rom.data[i + 1], i + 2
            if "hSpriteIndexOrTextID" in rom.sym and \
                    src != rom.sym["hSpriteIndexOrTextID"][1]:
                break
            want = text_id
        else:
            break
        if rom.data[i] != 0xFE:
            break
        comparand = rom.data[i + 1]
        i += 2
        if rom.data[i] == 0x21:
            pending = rom.data[i + 1] | (rom.data[i + 2] << 8)
            i += 3
        cc = rom.data[i]
        if cc in (0x20, 0x28):
            d = rom.data[i + 1]
            tgt, after = i + 2 + (d - 256 if d > 127 else d), i + 2
        elif cc in (0xC2, 0xCA):
            a = rom.data[i + 1] | (rom.data[i + 2] << 8)
            tgt = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
            after = i + 3
        else:
            break
        if want is None:
            facing = FACING_WORD.get(comparand, facing)
            equal = True
        else:
            equal = (want == comparand)
        taken = (not equal) if cc in (0x20, 0xC2) else equal
        off = tgt if taken else after
    return off, facing, pending

def script_conditional_text(rom, bank, hdr, text_id):
    off = entry_offset(rom, bank, hdr, text_id)
    return script_cond_at(rom, bank, off, text_id)

_BADGE_BIT_EVENTS = [
    "EVENT_BEAT_BROCK",
    "EVENT_BEAT_MISTY",
    "EVENT_BEAT_LT_SURGE",
    "EVENT_BEAT_ERIKA",
    "EVENT_BEAT_KOGA",
    "EVENT_BEAT_SABRINA",
    "EVENT_BEAT_BLAINE",
    "EVENT_BEAT_VIRIDIAN_GYM_GIOVANNI",
]

_BADGE_EVENT_IDS = None

def badge_bit_to_event(rom, addr, bit):
    global _BADGE_EVENT_IDS
    sym = rom.sym.get("wBeatGymFlags")
    if not sym or addr != sym[1] or not (0 <= bit < 8):
        return None
    if _BADGE_EVENT_IDS is None:
        rev = {v: k for k, v in event_flag_names().items()}
        _BADGE_EVENT_IDS = [rev.get(n) for n in _BADGE_BIT_EVENTS]
    return _BADGE_EVENT_IDS[bit]

def script_cond_at(rom, bank, off, text_id):
    if off is None or rom.data[off] != 0x08:
        return None

    start, _facing, pre0 = resolve_static_gates(rom, bank, off + 1, text_id)

    base = rom.sym["wEventFlags"][1]

    def scan(start, stop):
        load = branch = None
        i = start
        while i < stop:
            op = rom.data[i]
            if op == 0x21:
                if load is None:
                    load = rom.data[i + 1] | (rom.data[i + 2] << 8)
                i += 3
            elif op == 0xFA:
                addr = rom.data[i + 1] | (rom.data[i + 2] << 8)
                i += 3
                if rom.data[i] == 0xCB and 0x40 <= rom.data[i + 1] < 0x80:
                    bit = (rom.data[i + 1] - 0x40) >> 3
                    i += 2

                    if rom.data[i] == 0x21:
                        if load is None:
                            load = rom.data[i + 1] | (rom.data[i + 2] << 8)
                        i += 3
                    if rom.data[i] in (0x20, 0x28):
                        if branch is not None:
                            return load, False
                        branch = (addr, bit, rom.data[i], i)
                        i += 2
            elif op in (0xCD, 0xC3, 0xC9):
                break
            else:
                i += 1
        return load, branch

    pre, branch = scan(start, start + 48)
    pre = pre if pre is not None else pre0
    if not branch:
        return None
    addr, bit, cc, at = branch
    flag = badge_bit_to_event(rom, addr, bit)
    if flag is None:
        flag = (addr - base) * 8 + bit
        if flag < 0:
            return None

    disp = rom.data[at + 1]
    tgt = at + 2 + (disp - 256 if disp > 127 else disp)
    fall, _ = scan(at + 2, min(tgt, at + 2 + 48) if tgt > at else at + 2 + 48)
    taken, _ = scan(tgt, tgt + 48)

    fall, taken = fall or pre, taken or pre
    if fall is None or taken is None or fall == taken:
        return None

    variant, default = (taken, fall) if cc == 0x20 else (fall, taken)

    def dec(x):
        o = x if x < 0x4000 else bank * 0x4000 + (x - 0x4000)
        try:
            t, _ = G.decode_text(rom, o)
        except (IndexError, RecursionError, TypeError):
            return None
        return t

    variant, default = dec(variant), dec(default)
    if not variant or not default:
        return None
    return default, flag, variant

_INV_SYMS = None

def script_symbol(rom, bank, hdr, text_id):
    global _INV_SYMS
    if _INV_SYMS is None:
        _INV_SYMS = {}
        for n, (b, a) in rom.sym.items():
            off = a if a < 0x4000 else b * 0x4000 + (a - 0x4000)
            _INV_SYMS.setdefault(off, n)
    if text_id <= 0:
        return None
    a = rom.data[hdr + 5] | (rom.data[hdr + 6] << 8)
    tbl = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
    p = tbl + (text_id - 1) * 2
    ta = rom.data[p] | (rom.data[p + 1] << 8)
    if ta == 0:
        return None
    off = ta if ta < 0x4000 else bank * 0x4000 + (ta - 0x4000)
    return _INV_SYMS.get(off)

def parse_objects(rom, map_id):
    bank, hdr = map_header(rom, map_id)
    o = object_offset(rom, bank, hdr)
    d = rom.data
    out = {"border": d[o], "warps": [], "signs": [],
           "npcs": [], "items": [], "trainers": []}
    map_name = BP.MAP_NAMES[map_id] if map_id < len(BP.MAP_NAMES) else ""
    clerk_count = 0

    p = o + 1
    n = d[p]; p += 1
    for _ in range(n):
        y, x, dw, dm = d[p], d[p + 1], d[p + 2], d[p + 3]
        out["warps"].append((x, y, dm, dw))
        p += 4

    n = d[p]; p += 1
    for _ in range(n):
        y, x, tid = d[p], d[p + 1], d[p + 2]

        txt = text_at(rom, bank, hdr, tid)
        cond = facing = None
        if txt is None:
            txt, facing = script_plain_text(rom, bank, hdr, tid)
        if txt is None:
            cond = script_conditional_text(rom, bank, hdr, tid)
            if cond:
                txt = cond[0]
        if not txt:

            txt = SERVICE_SCRIPTS.get(script_symbol(rom, bank, hdr, tid))
        out["signs"].append((x, y, txt, cond, facing))
        p += 3

    n = d[p]; p += 1
    for obj_i in range(n):
        sprite, y, x, mv, rng = d[p], d[p + 1], d[p + 2], d[p + 3], d[p + 4]
        tid = d[p + 5]
        p += 6

        obj_id = obj_i + 1

        x, y = x - 4, y - 4
        kind = "npc"
        extra = ()
        if tid & BIT_TRAINER:
            kind = "trainer"
            extra = (d[p], d[p + 1])
            p += 2
        elif tid & BIT_ITEM:
            kind = "item"
            extra = (d[p],)
            p += 1
            if not extra[0]:

                kind = "npc"
        real_tid = tid & 0x3F
        text = script = cond = None
        if kind == "npc":
            sprite_name = sprite_names().get(sprite)
            if sprite_name == "CLERK":
                text = clerk_mart_service(map_name, clerk_count)
                clerk_count += 1
            if text is None:
                text = service_text(rom, bank, hdr, real_tid, sprite_name)
            if text is None:
                text = text_at(rom, bank, hdr, real_tid)
            if text is None:

                text, _facing = script_plain_text(rom, bank, hdr, real_tid)
            if text is None:

                cond = script_conditional_text(rom, bank, hdr, real_tid)
                if cond:
                    text = cond[0]
            if text is None:

                script = script_symbol(rom, bank, hdr, real_tid)
        rec = {"sprite": sprite, "x": x, "y": y, "mv": mv, "range": rng,
               "text": text, "script": script, "extra": extra,
               "cond": cond, "tid": real_tid, "obj_id": obj_id}
        out[{"npc": "npcs", "item": "items", "trainer": "trainers"}[kind]].append(rec)
    return out

CONN_ORDER = ("north", "south", "west", "east")
CONN_BIT = {"north": 3, "south": 2, "west": 1, "east": 0}
OPPOSITE = {"north": "south", "south": "north", "west": "east", "east": "west"}

def connections(rom, map_id):
    bank, hdr = map_header(rom, map_id)
    mask = rom.data[hdr + 9]
    p = hdr + 10
    out = []
    for d in CONN_ORDER:
        if not (mask >> CONN_BIT[d]) & 1:
            continue
        blk = rom.data[p:p + 11]
        dest = blk[0]
        y, x = blk[7], blk[8]
        raw = x if d in ("north", "south") else y
        adjust = raw - 256 if raw >= 0x80 else raw

        adjust *= 2
        dw, dh = map_dims(rom, dest)

        if d == "north":
            coord = dh * 4 - 1
        elif d == "south":
            coord = 1
        elif d == "west":
            coord = dw * 4 - 2
        else:
            coord = 0
        out.append((d, dest, coord, adjust))
        p += 11
    return out

def map_dims(rom, map_id):
    if map_id in BP.MAP_FILLER:
        w, h, _t, _s = BP.MAP_FILLER[map_id]
        return w, h
    _bank, hdr = map_header(rom, map_id)
    return rom.data[hdr + 2], rom.data[hdr + 1]

_SONG_BY_ID = None

def song_names(rom):
    global _SONG_BY_ID
    if _SONG_BY_ID is not None:
        return _SONG_BY_ID
    bases = {}
    for n, (b, a) in rom.sym.items():
        if re.fullmatch(r"SFX_Headers_\d", n):
            bases[b] = a
    out = {}
    for n, (b, a) in rom.sym.items():
        if not n.startswith("Music_") or "." in n:
            continue
        base = bases.get(b)
        if base is None or a < base or (a - base) % 3:
            continue
        out[(b, (a - base) // 3)] = camel_to_snake(n[len("Music_"):])
    _SONG_BY_ID = out
    return out

_PORT_TRACKS = None

def port_tracks():
    global _PORT_TRACKS
    if _PORT_TRACKS is None:
        path = os.path.join(REPO, "src", "game", "music.c")
        txt = open(path, encoding="utf-8", errors="replace").read()
        _PORT_TRACKS = set(re.findall(r'\{\s*"([a-z0-9_]+)"\s*,\s*MUSIC_', txt))
    return _PORT_TRACKS

def camel_to_snake(s):
    out = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", s)
    out = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", out).lower()
    known = port_tracks()
    if out in known:
        return out

    flat = out.replace("_", "")
    return flat if flat in known else out

_LASTMAP_SRC = None

WARP_LAST_OVERRIDE = {
    ("CeladonMansion1F", 4, 0): "CeladonCity 4",
    ("CeladonMansion1F", 4, 11): "CeladonCity 2",
    ("CeladonMansion1F", 5, 11): "CeladonCity 2",
    ("CeladonMart1F", 2, 7): "CeladonCity 0",
    ("CeladonMart1F", 3, 7): "CeladonCity 0",
    ("CeladonMart1F", 16, 7): "CeladonCity 1",
    ("CeladonMart1F", 17, 7): "CeladonCity 1",
    ("CeruleanCave1F", 24, 17): "CeruleanCity 6",
    ("CeruleanCave1F", 25, 17): "CeruleanCity 6",
    ("CinnabarLab", 2, 7): "CinnabarIsland 2",
    ("CinnabarLab", 3, 7): "CinnabarIsland 2",
    ("CopycatsHouse1F", 2, 7): "SaffronCity 0",
    ("CopycatsHouse1F", 3, 7): "SaffronCity 0",
    ("GameCorner", 15, 17): "CeladonCity 7",
    ("GameCorner", 16, 17): "CeladonCity 7",
    ("IndigoPlateauLobby", 7, 11): "IndigoPlateau 0",
    ("IndigoPlateauLobby", 8, 11): "IndigoPlateau 1",
    ("MtMoon1F", 14, 35): "Route4 1",
    ("MtMoon1F", 15, 35): "Route4 1",
    ("MtMoonB1F", 27, 3): "Route4 2",
    ("Museum1F", 10, 7): "PewterCity 0",
    ("Museum1F", 11, 7): "PewterCity 0",
    ("Museum1F", 16, 7): "PewterCity 1",
    ("Museum1F", 17, 7): "PewterCity 1",
    ("PokemonMansion1F", 4, 27): "CinnabarIsland 0",
    ("PokemonMansion1F", 5, 27): "CinnabarIsland 0",
    ("PokemonMansion1F", 6, 27): "CinnabarIsland 0",
    ("PokemonMansion1F", 7, 27): "CinnabarIsland 0",
    ("PokemonMansion1F", 26, 27): "CinnabarIsland 0",
    ("PokemonMansion1F", 27, 27): "CinnabarIsland 0",
    ("PokemonTower1F", 10, 17): "LavenderTown 1",
    ("PokemonTower1F", 11, 17): "LavenderTown 1",
    ("RedsHouse1F", 2, 7): "PalletTown 0",
    ("RedsHouse1F", 3, 7): "PalletTown 0",
    ("RockTunnel1F", 15, 0): "Route10 1",
    ("RockTunnel1F", 15, 3): "Route10 1",
    ("RockTunnel1F", 15, 33): "Route10 2",
    ("RockTunnel1F", 15, 35): "Route10 2",
    ("Route11Gate1F", 0, 4): "Route11 0",
    ("Route11Gate1F", 7, 4): "Route11 2",
    ("Route11Gate1F", 0, 5): "Route11 1",
    ("Route11Gate1F", 7, 5): "Route11 3",
    ("Route12Gate1F", 4, 0): "Route12 0",
    ("Route12Gate1F", 5, 0): "Route12 1",
    ("Route12Gate1F", 4, 7): "Route12 2",
    ("Route12Gate1F", 5, 7): "Route12 2",

    ("Route15Gate1F", 0, 4): "Route15 0",
    ("Route15Gate1F", 7, 4): "Route15 2",
    ("Route15Gate1F", 0, 5): "Route15 1",
    ("Route15Gate1F", 7, 5): "Route15 3",
    ("Route16Gate1F", 0, 2): "Route16 4",
    ("Route16Gate1F", 7, 2): "Route16 6",
    ("Route16Gate1F", 0, 3): "Route16 5",
    ("Route16Gate1F", 7, 3): "Route16 7",
    ("Route16Gate1F", 0, 8): "Route16 0",
    ("Route16Gate1F", 7, 8): "Route16 2",
    ("Route16Gate1F", 0, 9): "Route16 1",
    ("Route16Gate1F", 7, 9): "Route16 2",
    ("Route18Gate1F", 0, 4): "Route18 0",
    ("Route18Gate1F", 7, 4): "Route18 2",
    ("Route18Gate1F", 0, 5): "Route18 1",
    ("Route18Gate1F", 7, 5): "Route18 3",
    ("Route22Gate", 4, 0): "Route23 0",
    ("Route22Gate", 5, 0): "Route23 1",
    ("Route22Gate", 4, 7): "Route22 0",
    ("Route22Gate", 5, 7): "Route22 0",

    ("SafariZoneGate", 3, 5): "FuchsiaCity 4",
    ("SafariZoneGate", 4, 5): "FuchsiaCity 4",
    ("SaffronGym", 8, 17): "SaffronCity 2",
    ("SaffronGym", 9, 17): "SaffronCity 2",
    ("SeafoamIslands1F", 4, 17): "Route20 0",
    ("SeafoamIslands1F", 5, 17): "Route20 0",
    ("SeafoamIslands1F", 26, 17): "Route20 1",
    ("SeafoamIslands1F", 27, 17): "Route20 1",
    ("SilphCo1F", 10, 17): "SaffronCity 5",
    ("SilphCo1F", 11, 17): "SaffronCity 5",
    ("UndergroundPathRoute7", 3, 7): "Route7 4",
    ("UndergroundPathRoute7", 4, 7): "Route7 4",
    ("UndergroundPathRoute8", 3, 7): "Route8 4",
    ("UndergroundPathRoute8", 4, 7): "Route8 4",
    ("VermilionDock", 14, 0): "VermilionCity 5",
    ("VictoryRoad1F", 8, 17): "Route23 2",
    ("VictoryRoad1F", 9, 17): "Route23 2",
    ("VictoryRoad2F", 29, 7): "Route23 3",
    ("VictoryRoad2F", 29, 8): "Route23 3",
    ("ViridianForestNorthGate", 5, 0): "Route2 1",
    ("ViridianForestSouthGate", 4, 7): "Route2 5",
    ("ViridianForestSouthGate", 5, 7): "Route2 5",
}

def lastmap_destination(rom, map_id):
    global _LASTMAP_SRC
    if _LASTMAP_SRC is None:
        srcs = {}
        for mid, nm in enumerate(BP.MAP_NAMES):
            if not nm or mid >= BP.NUM_REAL_MAPS or mid in BP.MAP_FILLER:
                continue
            if is_duplicate_label(mid):
                continue
            try:
                ob = parse_objects(rom, mid)
            except Exception:
                continue
            for (_x, _y, dm, _dw) in ob["warps"]:
                if dm != 0xFF:
                    srcs.setdefault(dm, set()).add(mid)
        _LASTMAP_SRC = srcs
    s = _LASTMAP_SRC.get(map_id) or set()
    return next(iter(s)) if len(s) == 1 else None

_SCENE_BINDINGS = None

def scene_bindings(name):
    global _SCENE_BINDINGS
    if _SCENE_BINDINGS is None:
        _SCENE_BINDINGS = {}
        p = os.path.join(REPO, "mod_runtime", "scenes", "bindings.txt")
        try:
            with open(p, encoding="utf-8") as fh:
                for ln in fh:
                    ln = ln.strip()
                    if not ln or ln.startswith("#"):
                        continue
                    f = ln.split()
                    if len(f) >= 3:
                        _SCENE_BINDINGS.setdefault(f[1], []).append(ln)
        except OSError:

            raise SystemExit(
                "emit_kanto: FATAL -- cannot read %s\n"
                "  Scene bindings live there and are injected into every "
                "generated map.\n"
                "  Without them all 223 maps emit with NO NPC dialogue at "
                "all.\n"
                "  A packaged build must embed it -- see "
                "tools/dist/scan_extractor_sources.py." % p)
    return _SCENE_BINDINGS.get(name, [])

def is_duplicate_label(map_id):
    nm = BP.MAP_NAMES[map_id] if map_id < len(BP.MAP_NAMES) else ""
    if not nm:
        return False
    for i, other in enumerate(BP.MAP_NAMES):
        if other == nm and i < BP.NUM_REAL_MAPS and i not in BP.MAP_FILLER:
            return i != map_id
    return False

def map_music(rom, map_id):
    t = rom.offset("MapSongBanks")
    sid, bank = rom.data[t + map_id * 2], rom.data[t + map_id * 2 + 1]
    return PO.music_for_map(map_id, song_names(rom).get((bank, sid)))

ART_REL = "mod_runtime/custom_art/kanto"

TILESET_DSL = {
    0:  "overworld",   1:  "reds_house_1", 2:  "mart",        3:  "forest",
    4:  "reds_house_2",5:  "dojo",         6:  "pokecenter",  7:  "gym",
    8:  "house",       9:  "forest_gate",  10: "museum",      11: "underground",
    12: "gate",        13: "ship",         14: "ship_port",   15: "cemetery",
    16: "interior",    17: "cavern",       18: "lobby",       19: "mansion",
    20: "lab",         21: "club",         22: "facility",    23: "plateau",
}

def tileset_name(rom, map_id):
    ts = tileset_of(rom, map_id)
    name = TILESET_DSL.get(ts)
    if name is None:
        raise RomError("map %d: unknown tileset id %r -- add it to TILESET_DSL"
                       % (map_id, ts))
    return name

OUTDOOR_TILESETS = {0, 3, 23}

def is_indoor(rom, map_id):
    return tileset_of(rom, map_id) not in OUTDOOR_TILESETS

def block_quads(rom, map_id, bid):
    blocks, _gfx, coll, grass = tileset_parts(rom, map_id)
    base = bid * 16
    if base + 16 > len(blocks):
        return []
    out = []
    for cx, cy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        tl = blocks[base + (cy * 2) * 4 + cx * 2]
        tr = blocks[base + (cy * 2) * 4 + cx * 2 + 1]
        bl = blocks[base + (cy * 2 + 1) * 4 + cx * 2]
        br = blocks[base + (cy * 2 + 1) * 4 + cx * 2 + 1]
        out.append(((tl, tr, bl, br), (bl in coll, bl == grass)))
    return out

CUT_TILE_BY_TILESET = {0: (0x3D, 0x52), 7: (0x50,)}

CUT_TREE_BLOCK_SWAPS = {
    0x32: 0x6D,
    0x33: 0x6C,
    0x34: 0x6F,
    0x35: 0x4C,
    0x60: 0x6E,
    0x0B: 0x0A,
    0x3C: 0x35,
    0x3F: 0x35,
    0x3D: 0x36,
}

SCRIPT_TILE_SWAPS = {

    "VermilionGym": [dict(prefix="vermiliongym_gate", bx=2, by=2,
                          states={"closed": 0x24, "open": 0x05})],
    "GameCorner":   [dict(prefix="gamecorner_stairs", bx=8, by=2,
                          states={"closed": 0x2A, "open": None})],

    "LoreleisRoom": [dict(prefix="loreleisroom_door", bx=2, by=0,
                          states={"closed": 0x24, "open": 0x05})],
    "BrunosRoom":   [dict(prefix="brunosroom_door", bx=2, by=0,
                          states={"closed": 0x24, "open": 0x05})],
    "AgathasRoom":  [dict(prefix="agathasroom_door", bx=2, by=0,
                          states={"closed": 0x3B, "open": 0x0E})],

    "LancesRoom":   [dict(prefix="lancesroom_door_l", bx=2, by=6,
                          states={"closed": 0x72, "open": 0x31}),
                     dict(prefix="lancesroom_door_r", bx=3, by=6,
                          states={"closed": 0x73, "open": 0x32})],

    "PokemonMansion1F": [
        dict(prefix="mansion1f_gate_a", bx=12, by=6, states={"off": 0x0E, "on": 0x2D}),
        dict(prefix="mansion1f_gate_b", bx=8,  by=3, states={"off": 0x2D, "on": 0x0E}),
        dict(prefix="mansion1f_gate_c", bx=10, by=8, states={"off": 0x2D, "on": 0x0E}),
        dict(prefix="mansion1f_gate_d", bx=13, by=13, states={"off": 0x2D, "on": 0x0E}),
    ],
    "PokemonMansion2F": [
        dict(prefix="mansion2f_gate_a", bx=4, by=2,  states={"off": 0x0E, "on": 0x5F}),
        dict(prefix="mansion2f_gate_b", bx=9, by=4,  states={"off": 0x54, "on": 0x0E}),
        dict(prefix="mansion2f_gate_c", bx=3, by=11, states={"off": 0x5F, "on": 0x0E}),
    ],
    "PokemonMansion3F": [
        dict(prefix="mansion3f_gate_a", bx=7, by=2, states={"off": 0x0E, "on": 0x5F}),
        dict(prefix="mansion3f_gate_b", bx=7, by=5, states={"off": 0x5F, "on": 0x0E}),
    ],
    "PokemonMansionB1F": [
        dict(prefix="mansionb1f_gate_a", bx=13, by=8,  states={"off": 0x0E, "on": 0x2D}),
        dict(prefix="mansionb1f_gate_b", bx=6,  by=11, states={"off": 0x0E, "on": 0x5F}),
        dict(prefix="mansionb1f_gate_c", bx=4,  by=3,  states={"off": 0x5F, "on": 0x0E}),
        dict(prefix="mansionb1f_gate_d", bx=8,  by=8,  states={"off": 0x54, "on": 0x0E}),
    ],

    "CinnabarGym": [
        dict(prefix="cinnabargym_gate0", bx=9, by=3, states={"closed": 0x54, "open": 0x0E}),
        dict(prefix="cinnabargym_gate1", bx=6, by=3, states={"closed": 0x54, "open": 0x0E}),
        dict(prefix="cinnabargym_gate2", bx=6, by=6, states={"closed": 0x54, "open": 0x0E}),
        dict(prefix="cinnabargym_gate3", bx=3, by=8, states={"closed": 0x5F, "open": 0x0E}),
        dict(prefix="cinnabargym_gate4", bx=2, by=6, states={"closed": 0x54, "open": 0x0E}),
        dict(prefix="cinnabargym_gate5", bx=2, by=3, states={"closed": 0x54, "open": 0x0E}),
    ],

    "VermilionDock": [
        dict(prefix="vdock_ship_h5", bx=5, by=2, states={"gone": 0x0D, "present": None}),
        dict(prefix="vdock_ship_h6", bx=6, by=2, states={"gone": 0x0D, "present": None}),
        dict(prefix="vdock_ship_h7", bx=7, by=2, states={"gone": 0x0D, "present": None}),
        dict(prefix="vdock_ship_h8", bx=8, by=2, states={"gone": 0x0D, "present": None}),
    ],

    "VictoryRoad1F": [dict(prefix="victoryroad1f_sw", bx=4, by=6,
                           states={"on": 0x1D, "off": None})],
    "VictoryRoad2F": [dict(prefix="victoryroad2f_sw1", bx=3,  by=4,
                           states={"on": 0x15, "off": None}),
                      dict(prefix="victoryroad2f_sw2", bx=11, by=7,
                           states={"on": 0x1D, "off": None})],
    "VictoryRoad3F": [dict(prefix="victoryroad3f_sw1", bx=3, by=5,
                           states={"on": 0x1D, "off": None})],

    "RocketHideoutB1F": [dict(prefix="rockethideoutb1f_door", bx=12, by=8,
                              states={"closed": 0x54})],
    "RocketHideoutB4F": [dict(prefix="rockethideoutb4f_door", bx=12, by=5,
                              states={"closed": 0x2D})],
    "SilphCo2F":  [dict(prefix="silphco2f_door1",  bx=2,  by=2, states={"closed": 0x54}),
                   dict(prefix="silphco2f_door2",  bx=2,  by=5, states={"closed": 0x54})],
    "SilphCo3F":  [dict(prefix="silphco3f_door1",  bx=4,  by=4, states={"closed": 0x5F}),
                   dict(prefix="silphco3f_door2",  bx=8,  by=4, states={"closed": 0x5F})],
    "SilphCo4F":  [dict(prefix="silphco4f_door1",  bx=2,  by=6, states={"closed": 0x54}),
                   dict(prefix="silphco4f_door2",  bx=6,  by=4, states={"closed": 0x54})],
    "SilphCo5F":  [dict(prefix="silphco5f_door1",  bx=3,  by=2, states={"closed": 0x5F}),
                   dict(prefix="silphco5f_door2",  bx=3,  by=6, states={"closed": 0x5F}),
                   dict(prefix="silphco5f_door3",  bx=7,  by=5, states={"closed": 0x5F})],
    "SilphCo6F":  [dict(prefix="silphco6f_door",   bx=2,  by=6, states={"closed": 0x5F})],
    "SilphCo7F":  [dict(prefix="silphco7f_door1",  bx=5,  by=3, states={"closed": 0x54}),
                   dict(prefix="silphco7f_door2",  bx=10, by=2, states={"closed": 0x54}),
                   dict(prefix="silphco7f_door3",  bx=10, by=6, states={"closed": 0x54})],
    "SilphCo8F":  [dict(prefix="silphco8f_door",   bx=3,  by=4, states={"closed": 0x5F})],
    "SilphCo9F":  [dict(prefix="silphco9f_door1",  bx=1,  by=4, states={"closed": 0x5F}),
                   dict(prefix="silphco9f_door2",  bx=9,  by=2, states={"closed": 0x54}),
                   dict(prefix="silphco9f_door3",  bx=9,  by=5, states={"closed": 0x54}),
                   dict(prefix="silphco9f_door4",  bx=5,  by=6, states={"closed": 0x5F})],
    "SilphCo10F": [dict(prefix="silphco10f_door",  bx=5,  by=4, states={"closed": 0x54})],
    "SilphCo11F": [dict(prefix="silphco11f_door",  bx=3,  by=6, states={"closed": 0x20})],
}

def map_block_at(rom, map_id, bx, by):
    _bank, hdr = map_header(rom, map_id)
    w, h = rom.data[hdr + 2], rom.data[hdr + 1]
    if not (0 <= bx < w and 0 <= by < h):
        return None
    reg = {a["name"]: a for a in BP.ASSETS}
    cells_blk = reg[BP.map_asset_name(BP.map_blocks_source(map_id))]["fn"](rom)
    return cells_blk[by * w + bx]

def script_tile_alias_quads(rom, map_id, name):
    out = []
    for spec in SCRIPT_TILE_SWAPS.get(name, []):
        for state, bid in sorted(spec["states"].items()):
            if bid is None:
                bid = map_block_at(rom, map_id, spec["bx"], spec["by"])
                if bid is None:
                    continue
            qs = block_quads(rom, map_id, bid)
            if len(qs) != 4:
                continue
            for i, corner in enumerate(("tl", "tr", "bl", "br")):

                cx = spec["bx"] * 2 + (i & 1)
                cy = spec["by"] * 2 + (i >> 1)
                out.append(("%s_%s_%s" % (spec["prefix"], state, corner),
                            qs[i][0], qs[i][1][0], cx, cy))
    return out

DARK_MAPS = {
    0x52,
    0xE8,
}

def quad_is_cuttable(key, tileset):
    return key[2] in CUT_TILE_BY_TILESET.get(tileset, ())

def counter_tiles(rom, tileset):
    reg = {a["name"]: a for a in BP.ASSETS}
    ct = reg["gTilesetCounterTiles"]["fn"](rom)
    return set(ct[tileset * 3:tileset * 3 + 3]) - {0xFF}

def cut_replacements(rom, map_id, tileset):
    out = {}
    for before, after in CUT_TREE_BLOCK_SWAPS.items():
        qb = block_quads(rom, map_id, before)
        qa = block_quads(rom, map_id, after)
        if len(qb) != 4 or len(qa) != 4:
            continue
        for i in range(4):
            key_before = qb[i][0]
            if quad_is_cuttable(key_before, tileset):
                out.setdefault(key_before, (qa[i][0], qa[i][1]))
    return out

def border_block(rom, map_id):
    bank, hdr = map_header(rom, map_id)
    return rom.data[object_offset(rom, bank, hdr)]

_BOOKSHELF = None

def bookshelf_tiles(rom):
    global _BOOKSHELF
    if _BOOKSHELF is not None:
        return _BOOKSHELF
    _BOOK_OR_SCULPTURE.clear()
    _X_PARITY_TEXT.clear()
    statues_script = rom.sym["IndigoPlateauStatues"][1] \
        if "IndigoPlateauStatues" in rom.sym else None
    tp = rom.offset("TextPredefs")
    bank = rom.sym["BookshelfTileIDs"][0]
    sculpture_script = rom.sym["BookOrSculptureText"][1]
    out = {}
    p = rom.offset("BookshelfTileIDs")
    while rom.data[p] != 0xFF:
        ts, tile, idx = rom.data[p], rom.data[p + 1], rom.data[p + 2]
        p += 3
        e = tp + (idx - 1) * 2
        addr = rom.data[e] | (rom.data[e + 1] << 8)
        if addr == sculpture_script:

            txt, _ = G.decode_text(rom, rom.offset("_PokemonBooksText"))
            _BOOK_OR_SCULPTURE.add((ts, tile))
        elif addr == statues_script:

            _X_PARITY_TEXT[(ts, tile)] = (
                decode_sym_text(rom, "_IndigoPlateauStatuesText1"),
                decode_sym_text(rom, "_IndigoPlateauStatuesText2"),
                decode_sym_text(rom, "_IndigoPlateauStatuesText3"))
            txt = "?"
        else:
            txt = predef_text(rom, idx, bank)
        out[(ts, tile)] = txt
    _BOOKSHELF = out
    return out

MANSION_TILESET = 0x13
SCULPTURE_TILE = 0x38
_BOOK_OR_SCULPTURE = set()

_X_PARITY_TEXT = {}

def bookshelf_events(rom, map_id, name, grid, taken):
    table = bookshelf_tiles(rom)
    ts = tileset_of(rom, map_id)
    want = {tile: txt for (t, tile), txt in table.items() if t == ts}
    if not want:
        return []
    out = []
    for (x, y), key in sorted(grid.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        if (x, y) in taken:
            continue
        tile = key[2] if key[2] in want else (key[3] if key[3] in want else None)
        if tile is None:
            continue
        txt = want[tile]
        if (ts == MANSION_TILESET and (ts, tile) in _BOOK_OR_SCULPTURE
                and key[0] == SCULPTURE_TILE):
            txt = decode_sym_text(rom, "_DiglettSculptureText") or txt
        parity = _X_PARITY_TEXT.get((ts, tile))
        if parity:
            head, odd, even = parity
            body = odd if (x & 1) else even
            if not (head and body):
                continue
            txt = head + "\f" + body
        if txt:
            out.append('hidden_event %s %d %d "%s" up'
                       % (name, x, y, escape(txt)))
    return out

FIXED_TEXT_CALLBACKS = {
    "PrintBookcaseText":        "_BookcaseText",
    "PrintMagazinesText":       "_MagazinesText",
    "PrintNewBikeText":         "_NewBicycleText",
    "PrintRedSNESText":         "_RedBedroomSNESText",
    "DisplayOakLabLeftPoster":  "_PushStartText",
    "PrintIndigoPlateauHQText": "_IndigoPlateauHQText",
    "PrintFightingDojoText":    "_FightingDojoText",
    "PrintFightingDojoText2":   "_EnemiesOnEverySideText",
    "PrintTrashText":           "_VermilionGymTrashText",
}

SERVICE_CALLBACKS = {
    "OpenPokemonCenterPC": "service:pc",
    "OpenRedsPC":          "service:player_pc",

    "Mansion1Script_Switches": "service:mansion_switch",
    "Mansion2Script_Switches": "service:mansion_switch",
    "Mansion3Script_Switches": "service:mansion_switch",
    "Mansion4Script_Switches": "service:mansion_switch",
}

TRASH_CALLBACK = "GymTrashScript"

QUIZ_CALLBACK = "PrintCinnabarQuiz"

DIRECTIVE_CALLBACKS = {"HiddenItems", "HiddenCoins", "StartSlotMachine"}

SERVICE_SCRIPTS = {
    "CeladonMartElevatorText":     "service:celadon_mart_elevator",
    "RocketHideoutElevatorText":   "service:rockethideout_elevator",
    "SilphCoElevatorElevatorText": "service:silph_co_elevator",
    "CeladonMartRoofVendingMachineText": "service:celadon_mart_roof_vending",
}

def callback_facing(rom, cb):
    if cb not in rom.sym or "wSpriteStateData1" not in rom.sym:
        return None
    fa = rom.sym["wSpriteStateData1"][1] + 9
    off = rom.offset(cb)
    if off is None:
        return None
    i = off
    while i < off + 16:
        op = rom.data[i]
        if op == 0xFA:
            if (rom.data[i + 1] | (rom.data[i + 2] << 8)) == fa:
                return (FACING_WORD.get(rom.data[i + 4])
                        if rom.data[i + 3] == 0xFE else None)
            i += 3
        elif op in (0xC9, 0xC3):
            return None
        elif op == 0xCD:
            i += 3
        elif op == 0x3E:
            i += 2
        elif op == 0x21 or op == 0xEA:
            i += 3
        elif op == 0xF0 or op == 0xE0:
            i += 2
        else:
            i += 1
    return None

SPRITE_FACING = {0x00: "down", 0x04: "up", 0x08: "left", 0x0C: "right"}

def predef_text(rom, idx, bank):
    tp = rom.offset("TextPredefs")
    e = tp + (idx - 1) * 2
    addr = rom.data[e] | (rom.data[e + 1] << 8)
    try:
        txt, _ = G.decode_text(rom, bank * 0x4000 + (addr - 0x4000))
    except (IndexError, RecursionError, TypeError):
        return None
    return txt

def bench_guy_texts(rom):
    out = {}
    bench_bank = rom.sym["BenchGuyTextPointers"][0]
    p = rom.offset("BenchGuyTextPointers")
    while rom.data[p] != 0xFF:
        mid, facing, idx = rom.data[p], rom.data[p + 1], rom.data[p + 2]
        p += 3
        txt, cond = predef_words(rom, idx, bench_bank)
        if txt:
            out[mid] = (txt, SPRITE_FACING.get(facing, ""), cond)
    return out

_HIDDEN_EV = None

def predef_words(rom, idx, bank):
    txt = predef_text(rom, idx, bank)
    if txt:
        return txt, None
    tp = rom.offset("TextPredefs")
    if tp is None or idx <= 0:
        return None, None
    e = tp + (idx - 1) * 2
    a = rom.data[e] | (rom.data[e + 1] << 8)
    if not a:
        return None, None
    off = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
    cond = script_cond_at(rom, bank, off, idx)
    if cond:
        return cond[0], cond
    words, _f = script_words_at(rom, bank, off, idx)
    return words, None

def hidden_event_entries(rom):
    global _HIDDEN_EV
    if _HIDDEN_EV is not None:
        return _HIDDEN_EV
    bank = rom.sym["HiddenEventPointers"][0]
    inv = {}
    for n, (b, a) in rom.sym.items():
        off = a if a < 0x4000 else b * 0x4000 + (a - 0x4000)
        inv.setdefault(off, n)
    ids, i = [], 0
    mo = rom.offset("HiddenEventMaps")
    while rom.data[mo + i] != 0xFF:
        ids.append(rom.data[mo + i])
        i += 1
    po = rom.offset("HiddenEventPointers")
    out = {}
    for k, mid in enumerate(ids):
        a = rom.data[po + k * 2] | (rom.data[po + k * 2 + 1] << 8)
        p = bank * 0x4000 + (a - 0x4000)
        ents = []
        while rom.data[p] != 0xFF:
            y, x = rom.data[p], rom.data[p + 1]
            fb = rom.data[p + 3]
            fa = rom.data[p + 4] | (rom.data[p + 5] << 8)
            off = fa if fa < 0x4000 else fb * 0x4000 + (fa - 0x4000)
            ents.append((x, y, inv.get(off, ""), rom.data[p + 2]))
            p += 6
        out[mid] = ents
    _HIDDEN_EV = out
    return out

BADGE_NAMES = ["BOULDERBADGE", "CASCADEBADGE", "THUNDERBADGE", "RAINBOWBADGE",
               "SOULBADGE", "MARSHBADGE", "VOLCANOBADGE", "EARTHBADGE"]

_GYM_STATUES = None

def gym_statue_table(rom):
    global _GYM_STATUES
    if _GYM_STATUES is None:
        _GYM_STATUES = {}
        if "GymStatues" in rom.sym:
            bank = rom.sym["GymStatues"][0]

            o = rom.offset("GymStatues")
            for i in range(o, o + 32):
                if rom.data[i] == 0x21:
                    a = rom.data[i + 1] | (rom.data[i + 2] << 8)
                    p = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
                    while rom.data[p] != 0xFF:
                        _GYM_STATUES[rom.data[p]] = rom.data[p + 1]
                        p += 2
                    break
    return _GYM_STATUES

def ram_text(rom, sym, inserts):
    if sym not in rom.sym:
        return None
    bank = rom.sym[sym][0]
    off = rom.offset(sym)
    cm = G.charmap()
    out = []
    nxt = list(inserts)
    for _ in range(512):
        b = rom.data[off]
        off += 1
        if b == 0x01:
            off += 2
            out.append(nxt.pop(0) if nxt else "")
        elif b == 0x00:
            continue
        elif b == 0x50:
            if rom.data[off] not in (0x00, 0x01):
                break
        elif b in (0x57, 0x58):
            break
        elif b in G.CONTROL:
            out.append(G.CONTROL[b])
        elif b == G.TX_FAR:
            a = rom.data[off] | (rom.data[off + 1] << 8)
            fb = rom.data[off + 2]
            off = a if a < 0x4000 else fb * 0x4000 + (a - 0x4000)
            bank = fb
        elif b < 0x18:
            break
        else:
            tok = cm.get(b, "")
            out.append(G.SUBSTITUTIONS.get(tok, tok) if len(tok) > 1 else tok)
    return "".join(out).rstrip(" ")

def gym_statue_lines(rom, map_id, name, ents, taken):
    mask = gym_statue_table(rom).get(map_id)
    if not mask:
        return []

    city = leader = None
    for s in rom.sym:
        if not s.startswith(name):
            continue
        if s.endswith(".CityName"):
            city = decode_sym_text(rom, s)
        elif s.endswith(".LeaderName"):
            leader = decode_sym_text(rom, s)
    if not city or not leader:
        return []
    base = ram_text(rom, "_GymStatueText1", (city, leader))
    won = ram_text(rom, "_GymStatueText2", (city, leader))
    if not base:
        return []
    badge = BADGE_NAMES[mask.bit_length() - 1]
    out = []
    for x, y, cb, _arg in ents:
        if cb != "GymStatues" or (x, y) in taken:
            continue

        out.append('hidden_event %s %d %d "%s" up' % (name, x, y, escape(base)))
        if won and won != base:
            out.append('badge_if %s "%s"' % (badge, escape(won)))
        taken.add((x, y))
    return out

def callback_text(rom, cb, arg=0):
    if cb not in rom.sym:
        return None, None, None
    bank = rom.sym[cb][0]
    off = rom.offset(cb)
    if off is None:
        return None, None, None

    facing = callback_facing(rom, cb)

    if "PrintPredefTextID" in rom.sym:
        pa = rom.sym["PrintPredefTextID"][1]
        lo, hi = pa & 0xFF, (pa >> 8) & 0xFF
        for i in range(off, off + 64):

            if rom.data[i] == 0x3E:
                n = i + 2
                if (rom.data[n] in (0xCD, 0xC3) and rom.data[n + 1] == lo
                        and rom.data[n + 2] == hi):
                    return predef_words(rom, rom.data[i + 1], bank) + (facing,)
            elif rom.data[i] == 0xFA:
                n = i + 3
                if (rom.data[n] in (0xCD, 0xC3) and rom.data[n + 1] == lo
                        and rom.data[n + 2] == hi):

                    return predef_words(rom, arg, bank) + (facing,)

    load = None
    i = off
    while i < off + 64:
        op = rom.data[i]
        if op == 0x21:
            if load is not None:
                return None, None, facing
            load = rom.data[i + 1] | (rom.data[i + 2] << 8)
            i += 3
        elif op in (0xCD, 0xC3, 0xC9):
            break
        elif op in (0x20, 0x28, 0x30, 0x38):
            return None, None, facing
        else:
            i += 1
    if not load:
        return None, None, facing
    o = load if load < 0x4000 else bank * 0x4000 + (load - 0x4000)
    try:
        txt, _ = G.decode_text(rom, o)
    except (IndexError, RecursionError, TypeError):
        return None, None, facing
    return txt, None, facing

def decode_sym_text(rom, sym):
    try:
        t, _ = G.decode_text(rom, rom.offset(sym))
    except (IndexError, RecursionError, TypeError):
        return None
    return t

def hidden_object_events(rom, map_id, name, taken):
    ents = hidden_event_entries(rom).get(map_id)
    if not ents:
        return []
    bench = bench_guy_texts(rom)
    out = gym_statue_lines(rom, map_id, name, ents, taken)
    for x, y, cb, _arg in ents:
        if (x, y) in taken:
            continue
        face_override = cond = None
        if cb == TRASH_CALLBACK:

            out.append('hidden_event %s %d %d "service:vermilion_gym_trash_%d"'
                       % (name, x, y, _arg))
            taken.add((x, y))
            continue
        if cb == QUIZ_CALLBACK:

            out.append('hidden_event %s %d %d "service:cinnabar_gym_quiz%d" up'
                       % (name, x, y, _arg & 0xF))
            taken.add((x, y))
            continue
        if cb in SERVICE_CALLBACKS:
            sf = callback_facing(rom, cb)
            out.append('hidden_event %s %d %d "%s"%s'
                       % (name, x, y, SERVICE_CALLBACKS[cb],
                          (" " + sf) if sf else ""))
            taken.add((x, y))
            continue
        if cb == "PrintBenchGuyText":
            hit = bench.get(map_id)
            if not hit:
                continue
            txt, face_override, cond = hit
        else:
            sym = FIXED_TEXT_CALLBACKS.get(cb)
            if not sym:
                if cb in DIRECTIVE_CALLBACKS:
                    continue

                txt, cond, cbface = callback_text(rom, cb, _arg)
                if txt:
                    out.append('hidden_event %s %d %d "%s"%s'
                               % (name, x, y, escape(txt),
                                  (" " + cbface) if cbface else ""))
                    if cond:
                        out.append('text_if %s "%s"'
                                   % (event_flag_names().get(cond[1], str(cond[1])),
                                      escape(cond[2])))
                    taken.add((x, y))
                continue
            if sym not in rom.sym:
                continue
            try:
                txt, _ = G.decode_text(rom, rom.offset(sym))
            except (IndexError, RecursionError, TypeError):
                continue
        if not txt:
            continue
        derived = callback_facing(rom, cb)
        face = (" " + face_override) if face_override else (
            (" " + derived) if derived else "")
        out.append('hidden_event %s %d %d "%s"%s'
                   % (name, x, y, escape(txt), face))
        if cond:
            out.append('text_if %s "%s"'
                       % (event_flag_names().get(cond[1], str(cond[1])),
                          escape(cond[2])))
        taken.add((x, y))
    return out

COIN_ITEM_ID = 0x3B
SLOT_KINDS = {0xFD: "out_of_order", 0xFE: "out_to_lunch", 0xFF: "someones_keys"}

GENDER = ""

def dsl_constant(rom, off, width):
    out = []
    for i in range(width):
        c = rom.data[off + i]
        if c == 0x50:
            break
        if 0x80 <= c <= 0x99:
            out.append(chr(ord("A") + c - 0x80))
        elif 0xA0 <= c <= 0xB9:
            out.append(chr(ord("a") + c - 0xA0))
        elif 0xF6 <= c <= 0xFF:
            out.append(chr(ord("0") + c - 0xF6))
        elif c == 0x7F:
            out.append(" ")
        elif c == 0xBA:
            out.append("E")
        elif c == 0xEF:
            out.append(GENDER + "M")
        elif c == 0xF5:
            out.append(GENDER + "F")
        elif c == 0xE3:
            out.append("-")
        elif c == 0xE8:
            out.append(".")
        elif c == 0xE6:
            out.append("?")
        elif c == 0xBD:
            out.append("S")
        elif c == 0xE1:
            out.append("PK")
        elif c == 0xE2:
            out.append("MN")

    s = re.sub(r"[^A-Z0-9" + GENDER + r"]+", "_", "".join(out).upper()).strip("_")
    return s.replace(GENDER, ".")

_ITEM_NAMES = None

def item_constant_names(rom):
    global _ITEM_NAMES
    if _ITEM_NAMES is not None:
        return _ITEM_NAMES
    out = {}
    p = rom.offset("ItemNames")

    for i in range(1, 98):
        end = p
        while end < p + 20 and rom.data[end] != 0x50:
            end += 1
        c = dsl_constant(rom, p, end - p)
        if c:
            out[i] = c
        p = end + 1

    tm = rom.offset("TechnicalMachines")
    if tm is not None:
        moves = move_names(rom)
        for n in range(NUM_HMS):
            mv = moves.get(rom.data[tm + NUM_TMS + n])
            if mv:
                out[HM01_ITEM_ID + n] = "HM_" + mv
        for n in range(NUM_TMS):
            mv = moves.get(rom.data[tm + n])
            if mv:
                out[TM01_ITEM_ID + n] = "TM_" + mv
    _ITEM_NAMES = out
    return out

HM01_ITEM_ID = 0xC4
TM01_ITEM_ID = 0xC9
NUM_TMS = 50
NUM_HMS = 5

_MOVE_NAMES = None

def move_names(rom):
    global _MOVE_NAMES
    if _MOVE_NAMES is None:
        _MOVE_NAMES = {}
        p = rom.offset("MoveNames")
        if p is not None:
            for i in range(1, 166):
                end = p
                while end < p + 20 and rom.data[end] != 0x50:
                    end += 1
                c = dsl_constant(rom, p, end - p)
                if c:
                    _MOVE_NAMES[i] = c
                p = end + 1
    return _MOVE_NAMES

def hidden_object_directives(rom, map_id, name, taken):
    ents = hidden_event_entries(rom).get(map_id)
    if not ents:
        return []
    items = item_constant_names(rom)
    out = []
    for x, y, cb, arg in ents:
        if (x, y) in taken:
            continue
        if cb == "HiddenItems":
            out.append("hidden_item %s %d %d %s"
                       % (name, x, y, items.get(arg, str(arg))))
        elif cb == "HiddenCoins":
            out.append("hidden_coin %s %d %d %d"
                       % (name, x, y, arg - COIN_ITEM_ID))
        elif cb == "StartSlotMachine":
            kind = SLOT_KINDS.get(arg)
            out.append("slot_machine %s %d %d%s"
                       % (name, x, y, " " + kind if kind else ""))
        else:
            continue
        taken.add((x, y))
    return out

_SPECIES_NAMES = None

def species_names(rom):
    global _SPECIES_NAMES
    if _SPECIES_NAMES is not None:
        return _SPECIES_NAMES
    base = rom.offset("MonsterNames")
    out = {}
    for i in range(1, 191):
        c = dsl_constant(rom, base + (i - 1) * 10, 10)
        if c:
            out[i] = c
    _SPECIES_NAMES = out
    return out

def wild_lines(rom, map_id, name):
    wp = rom.offset("WildDataPointers")
    bank = rom.sym["WildDataPointers"][0]
    a = rom.data[wp + map_id * 2] | (rom.data[wp + map_id * 2 + 1] << 8)
    if not a:
        return []
    o = bank * 0x4000 + (a - 0x4000)
    names = species_names(rom)
    out = []
    for kind in ("grass", "water"):
        rate = rom.data[o]
        o += 1
        if not rate:
            continue
        out.append("wild_rate %s %s %d" % (name, kind, rate))
        for slot in range(1, 11):
            lvl, sp = rom.data[o], rom.data[o + 1]
            o += 2
            out.append("wild_encounter %s %s %d %s %d"
                       % (name, kind, slot, names.get(sp, str(sp)), lvl))
    return out

OPP_BASE = 200
TRAINER_HEADER_LEN = 12

_EVENT_NAMES = None

def event_flag_names():
    global _EVENT_NAMES
    if _EVENT_NAMES is None:
        _EVENT_NAMES = {}
        path = os.path.join(REPO, "src", "data", "event_flag_ids.h")
        try:
            txt = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            return _EVENT_NAMES
        for n, v in re.findall(r'\{\s*"(\w+)"\s*,\s*(\d+)u?\s*\}', txt):
            _EVENT_NAMES.setdefault(int(v), n)
    return _EVENT_NAMES

def trainer_header(rom, bank, hdr, text_id):
    if text_id <= 0:
        return None
    a = rom.data[hdr + 5] | (rom.data[hdr + 6] << 8)
    tbl = a if a < 0x4000 else bank * 0x4000 + (a - 0x4000)
    p = tbl + (text_id - 1) * 2
    ta = rom.data[p] | (rom.data[p + 1] << 8)
    if not ta:
        return None
    off = ta if ta < 0x4000 else bank * 0x4000 + (ta - 0x4000)
    if rom.data[off] != 0x08 or rom.data[off + 1] != 0x21:
        return None

    if "TalkToTrainer" in rom.sym:
        ca = rom.sym["TalkToTrainer"][1]
        q = off + 4
        for _ in range(4):
            if rom.data[q] != 0x18:
                break
            d = rom.data[q + 1]
            q = q + 2 + (d - 256 if d > 127 else d)
        if not (rom.data[q] == 0xCD
                and rom.data[q + 1] == (ca & 0xFF)
                and rom.data[q + 2] == ((ca >> 8) & 0xFF)):
            return None
    ha = rom.data[off + 2] | (rom.data[off + 3] << 8)
    h = ha if ha < 0x4000 else bank * 0x4000 + (ha - 0x4000)

    bit = rom.data[h]
    sight = rom.data[h + 1] >> 4
    flag_addr = rom.data[h + 2] | (rom.data[h + 3] << 8)
    base = rom.sym["wEventFlags"][1]
    flag = (flag_addr - base) * 8 + bit

    def _txt(i):
        pa = rom.data[h + i] | (rom.data[h + i + 1] << 8)
        if not pa:
            return None
        o = pa if pa < 0x4000 else bank * 0x4000 + (pa - 0x4000)
        try:
            t, _ = G.decode_text(rom, o)
        except (IndexError, RecursionError, TypeError):
            return None
        return t

    return flag, sight, _txt(4), _txt(6), _txt(8)

ITEM_BALL_SHOW_IF = {
    ("RocketHideoutB4F", 10, 2): "EVENT_ROCKET_DROPPED_LIFT_KEY",
    ("RocketHideoutB4F", 25, 2): "EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI",
}

NPC_HIDE_IF = {
    ("CeruleanCity", 4, 12): "EVENT_BEAT_CHAMPION_RIVAL",
}

def item_ball_lines(rom, map_id, name):
    items = item_constant_names(rom)
    out = []
    for r in parse_objects(rom, map_id)["items"]:
        if not r["extra"]:
            continue
        out.append("item_ball %s %d %d %s"
                   % (name, r["x"], r["y"],
                      items.get(r["extra"][0], str(r["extra"][0]))))
        gate = ITEM_BALL_SHOW_IF.get((name, r["x"], r["y"]))
        if gate:
            out.append("show_if %s" % gate)
    return out

_CLASS_NAMES = None

def trainer_class_names(rom):
    global _CLASS_NAMES
    if _CLASS_NAMES is None:
        _CLASS_NAMES = {}
        p = rom.offset("TrainerNames")
        if p is not None:
            raw = {}
            has_gender_glyph = {}
            for i in range(1, NUM_TRAINER_CLASSES + 1):
                end = p
                while end < p + 20 and rom.data[end] != 0x50:
                    end += 1

                has_gender_glyph[i] = any(rom.data[j] in (0xEF, 0xF5) for j in range(p, end))
                raw[i] = dsl_constant(rom, p, end - p)
                p = end + 1
            seen = collections.Counter(v for v in raw.values() if v)
            _CLASS_NAMES = {k: v for k, v in raw.items()
                             if v and seen[v] == 1 and not has_gender_glyph[k]}
    return _CLASS_NAMES

NUM_TRAINER_CLASSES = 47

STATIC_DEFAULT_SPRITE = "POKE_BALL"
STATIC_DEFAULT_FACING = "down"

MAP_SCRIPTS = {
    "Route16Gate1F": "route16gate1f",
    "Route18Gate1F": "route18gate1f",
}

def static_encounter_lines(rom, map_id, name):
    bank, hdr = map_header(rom, map_id)
    ob = parse_objects(rom, map_id)
    species = species_names(rom)
    sn = sprite_names()
    out = []
    for r in ob["trainers"]:
        raw = r["extra"][0]
        if raw >= OPP_BASE:
            continue
        info = trainer_header(rom, bank, hdr, r["tid"])
        if not info:
            continue
        flag, _sight, before, _after, _defeat = info
        mon = species.get(raw)
        if not mon:
            continue
        line = ('static_encounter %s %s %d %d %d %s "%s"'
                % (name, mon, r["extra"][1], r["x"], r["y"],
                   event_flag_names().get(flag, str(flag)),
                   escape(before or "")))
        s = sn.get(r["sprite"])
        if s and s != STATIC_DEFAULT_SPRITE:
            line += " sprite:%s" % s
        face = FACING.get(r["range"], STATIC_DEFAULT_FACING)
        if face != STATIC_DEFAULT_FACING:
            line += " facing:%s" % face
        out.append(line)
    return out

GYM_LEADER_MAP_SERVICE = {}

def gym_leader_lines(rom, map_id, name):
    entry = GYM_LEADER_MAP_SERVICE.get(name)
    if entry is None:
        return []
    cls, service = entry
    ob = parse_objects(rom, map_id)
    sn = sprite_names()
    out = []
    for r in ob["trainers"]:
        raw = r["extra"][0]
        if raw < OPP_BASE or raw - OPP_BASE != cls:
            continue
        out.append('npc %s %s %d %d %s "%s"'
                   % (name, sn.get(r["sprite"], str(r["sprite"])),
                      r["x"], r["y"], movement_word(r["mv"], r["range"]),
                      service))
    return out

SCRIPT_NPC_TEXT_OVERRIDE = {
    ("CeruleanCity", "COOLTRAINER_F", 29, 26): "CeruleanCityCooltrainerF1Text.SlowbroWithdrawText",
    ("CeruleanCity", "MONSTER", 28, 26): "CeruleanCitySlowbroText.IgnoredOrdersText",

    ("SSAnneKitchen", "COOK", 11, 13): (
        "SSAnneKitchenCook7Text.MainCourseIsText",
        "SSAnneKitchenCook7Text.PrimeBeefSteakText",
    ),
}

_SILPH_FLOOR_RE = re.compile(r"^SilphCo(\d+)F$")

def silph_co_rocket_hide_flag(map_name):
    m = _SILPH_FLOOR_RE.match(map_name or "")
    if not m:
        return None
    return "EVENT_BEAT_SILPH_CO_GIOVANNI" if int(m.group(1)) >= 2 else None

TRAINER_HIDE_IF = {
    ("CeruleanCity", 30, 8): "EVENT_BEAT_CERULEAN_ROCKET_THIEF",

    ("GameCorner", 9, 5): "EVENT_1BF",
}

AFTER_BATTLE_SCENES = {
    ("FightingDojo", 5, 3): "kanto/fightingdojo/karate_master_post",
    ("PokemonTower7F", 9, 11): "kanto/pokemontower7f/rocket1_leave",
    ("PokemonTower7F", 12, 9): "kanto/pokemontower7f/rocket2_leave",
    ("PokemonTower7F", 9, 7): "kanto/pokemontower7f/rocket3_leave",
}

TRAINER_HEADER_OVERRIDE = {

    ("GameCorner", 9, 5): (
        447,
        0,
        "GameCornerRocketText.ImGuardingThisPosterText",
        None,
        "GameCornerRocketText.BattleEndText",
    ),

    ("FightingDojo", 5, 3): (
        849,
        0,
        "FightingDojoKarateMasterText.Text",
        "FightingDojoKarateMasterText.IWillGiveYouAPokemonText",
        "FightingDojoKarateMasterText.DefeatedText",
    ),

    ("PewterGym", 4, 1): (
        119,
        0,
        "",
        "",
        "PewterGymBrockReceivedBoulderBadgeText",
    ),

    ("CeruleanGym", 4, 2): (
        191,
        0,
        "",
        "",
        "CeruleanGymMistyReceivedCascadeBadgeText",
    ),
    ("VermilionGym", 5, 1): (
        359,
        0,
        "",
        "",
        "VermilionGymLTSurgeReceivedThunderBadgeText",
    ),
    ("CeladonGym", 4, 3): (
        425,
        0,
        "",
        "",
        "_CeladonGymErikaReceivedRainbowBadgeText",
    ),
    ("FuchsiaGym", 4, 10): (
        601,
        0,
        "",
        "",
        "FuchsiaGymKogaText.ReceivedSoulBadgeText",
    ),
    ("SaffronGym", 9, 8): (
        865,
        0,
        "",
        "",
        "SaffronGymSabrinaText.ReceivedMarshBadgeText",
    ),
    ("CinnabarGym", 3, 3): (
        665,
        0,
        "",
        "",
        "_CinnabarGymBlaineReceivedVolcanoBadgeText",
    ),
    ("ViridianGym", 2, 1): (
        81,
        0,
        "",
        "",
        "_ViridianGymGiovanniReceivedEarthBadgeText",
    ),
    ("MtMoonB2F", 12, 8): (
        1401,
        0,

        "MtMoonB2FSuperNerdTheyreBothMineText",
        "MtMoonB2FSuperNerdOkIllShareText",
        "MtMoonB2FSuperNerdOkIllShareText",
    ),

    ("CeruleanCity", 30, 8): (
        167,
        0,

        "CeruleanCityRocketText.Text",
        "CeruleanCityRocketText.IGiveUpText",
        "CeruleanCityRocketText.IGiveUpText",
    ),
}

TRAINER_AS_SERVICE = {
    ("CinnabarGym", 17,  2): "service:cinnabar_gym_trainer1",
    ("CinnabarGym", 17,  8): "service:cinnabar_gym_trainer2",
    ("CinnabarGym", 11,  4): "service:cinnabar_gym_trainer3",
    ("CinnabarGym", 11,  8): "service:cinnabar_gym_trainer4",
    ("CinnabarGym", 11, 14): "service:cinnabar_gym_trainer5",
    ("CinnabarGym",  3, 14): "service:cinnabar_gym_trainer6",
    ("CinnabarGym",  3,  8): "service:cinnabar_gym_trainer7",
}

def trainer_lines(rom, map_id, name):
    bank, hdr = map_header(rom, map_id)
    ob = parse_objects(rom, map_id)
    sn = sprite_names()
    out = []
    for r in ob["trainers"]:
        if r["extra"][0] < OPP_BASE:
            continue
        gl = GYM_LEADER_MAP_SERVICE.get(name)
        if gl and (r["extra"][0] - OPP_BASE) == gl[0]:
            continue
        info = trainer_header(rom, bank, hdr, r["tid"])
        if not info:
            ov = TRAINER_HEADER_OVERRIDE.get((name, r["x"], r["y"]))
            if not ov:

                svc = TRAINER_AS_SERVICE.get((name, r["x"], r["y"]))
                if svc:
                    out.append('npc %s %s %d %d %s "%s"'
                               % (name, sn.get(r["sprite"], str(r["sprite"])),
                                  r["x"], r["y"],
                                  movement_word(r["mv"], r["range"]), svc))
                    continue

                out.append("# ROM trainer object NOT emitted: %s at (%d,%d), "
                           "party %d -- no TrainerHeader and no "
                           "TRAINER_HEADER_OVERRIDE entry. Expected if a scene "
                           "or a *_scripts.c owns this fight; a BUG if "
                           "anything calls `engage_trainer %d %d`."
                           % (trainer_class_names(rom).get(
                                  r["extra"][0] - OPP_BASE, "?"),
                              r["x"], r["y"], r["extra"][1],
                              r["x"], r["y"]))
                sys.stderr.write(
                    "[emit_kanto] %s: dropped trainer %s at (%d,%d) "
                    "-- no header, no override\n"
                    % (name, trainer_class_names(rom).get(
                           r["extra"][0] - OPP_BASE, "?"), r["x"], r["y"]))
                continue
            flag, sight, before_sym, after_sym, defeat_sym = ov
            before = rom_text(rom, before_sym) if before_sym else None
            after = rom_text(rom, after_sym) if after_sym else None
            defeat = rom_text(rom, defeat_sym) if defeat_sym else None
        else:
            flag, sight, before, after, defeat = info
        cls = r["extra"][0] - OPP_BASE
        party = r["extra"][1]
        face = FACING.get(r["range"], "down")

        line = ('trainer %s %s %d %d %d %s %d "%s" "%s" "%s" %s'
                % (name, trainer_class_names(rom).get(cls, cls),
                   party, r["x"], r["y"], face, sight,
                   escape(before or ""), escape(after or ""),
                   escape(defeat or ""),
                   event_flag_names().get(flag, str(flag))))

        s = sn.get(r["sprite"])
        if s:
            line += " sprite:%s" % s
        out.append(line)
        hide_flag = (TRAINER_HIDE_IF.get((name, r["x"], r["y"]))
                     or silph_co_rocket_hide_flag(name))
        if hide_flag:
            out.append("hide_if %s" % hide_flag)

        ab = AFTER_BATTLE_SCENES.get((name, r["x"], r["y"]))
        if ab:
            out.append("after_battle %s" % ab)
    return out

_DOOR_TILES = None
_WARP_TILES = None

def door_tiles(rom):
    global _DOOR_TILES
    if _DOOR_TILES is None:
        _DOOR_TILES = {}
        bank = rom.sym["DoorTileIDPointers"][0]
        p = rom.offset("DoorTileIDPointers")
        while rom.data[p] != 0xFF:
            ts = rom.data[p]
            a = rom.data[p + 1] | (rom.data[p + 2] << 8)
            p += 3
            q = bank * 0x4000 + (a - 0x4000)
            s = set()
            while rom.data[q]:
                s.add(rom.data[q])
                q += 1
            _DOOR_TILES[ts] = s
    return _DOOR_TILES

def warp_tiles(rom):
    global _WARP_TILES
    if _WARP_TILES is None:
        _WARP_TILES = {}
        bank = rom.sym["WarpTileIDPointers"][0]
        base = rom.offset("WarpTileIDPointers")
        for ts in range(BP.NUM_TILESETS):
            a = rom.data[base + ts * 2] | (rom.data[base + ts * 2 + 1] << 8)
            q = bank * 0x4000 + (a - 0x4000)
            s = set()
            while rom.data[q] != 0xFF:
                s.add(rom.data[q])
                q += 1
            _WARP_TILES[ts] = s
    return _WARP_TILES

WALK_INTO_STEP = {"left": (-1, 0), "right": (1, 0),
                  "up": (0, -1), "down": (0, 1)}

def walk_into_direction(x, y, w, h, warp_cells, passable):
    horiz = (x - 1, y) in warp_cells or (x + 1, y) in warp_cells
    vert = (x, y - 1) in warp_cells or (x, y + 1) in warp_cells
    if vert and not horiz:
        pair = ("right", "left")
    elif horiz and not vert:
        pair = ("down", "up")
    else:
        pair = ("right", "left", "down", "up")

    open_ = [d for d in pair
             if passable.get((x - WALK_INTO_STEP[d][0],
                              y - WALK_INTO_STEP[d][1]), False)]
    if len(open_) == 1:
        return open_[0]

    edges = [s for s, t in (("left", x == 0), ("right", x == w - 1),
                            ("up", y == 0), ("down", y == h - 1)) if t]
    if len(edges) == 1:
        return edges[0]

    blocked = [d for d, (dx, dy) in WALK_INTO_STEP.items()
               if not passable.get((x + dx, y + dy), False)
               and passable.get((x - dx, y - dy), False)]
    return blocked[0] if len(blocked) == 1 else None

F2_TILESETS = frozenset((0, 13, 14, 23))
F2_MAP_NAMES = frozenset(("RockTunnel1F", "RocketHideoutB1F",
                          "RocketHideoutB2F", "RocketHideoutB4F"))

def _extra_warp_check_is_walk_into(name, tileset):
    return name in F2_MAP_NAMES or tileset in F2_TILESETS

def dead_position_warp_indices(rom, map_id, name):
    ts = tileset_of(rom, map_id)
    if _extra_warp_check_is_walk_into(name, ts):
        return frozenset()
    doors = door_tiles(rom).get(ts, set())
    warps = warp_tiles(rom).get(ts, set())
    fire_on_step = doors | warps
    try:
        _defs, grid, gw, gh = quads(rom, map_id)
    except Exception:
        return frozenset()
    dead = set()
    for idx, w in enumerate(parse_objects(rom, map_id)["warps"]):
        x, y = w[0], w[1]
        key = grid.get((x, y))
        if key is None or key[2] in fire_on_step:
            continue
        if x == 0 or y == 0 or x == gw - 1 or y == gh - 1:
            continue
        dead.add(idx)
    return frozenset(dead)

def warp_kind_lines(rom, map_id, name, grid, defs=None, gw=0, gh=0):
    ts = tileset_of(rom, map_id)
    doors = door_tiles(rom).get(ts, set())
    warps = warp_tiles(rom).get(ts, set())
    fire_on_step = doors | warps
    stair_ids = warps - doors
    indoor = is_indoor(rom, map_id)
    cells = [(x, y) for x, y, _dm, _dw in parse_objects(rom, map_id)["warps"]]
    warp_cells = set(cells)
    passable = ({c: defs[k][0] for c, k in grid.items() if k in defs}
                if defs else {})
    out = []
    dead = dead_position_warp_indices(rom, map_id, name)
    dead_cells = {c for i, c in enumerate(cells) if i in dead}
    for (x, y) in cells:
        key = grid.get((x, y))
        if key is None:
            continue

        if (x, y) in dead_cells:
            continue
        tile = key[2]
        if tile not in fire_on_step:
            d = (walk_into_direction(x, y, gw, gh, warp_cells, passable)
                 if passable else None)
            out.append("warp_walk_into %s %d %d%s"
                       % (name, x, y, (" " + d) if d else ""))
        elif (indoor and tile in stair_ids
                and (x == 0 or y == 0 or x == gw - 1 or y == gh - 1)):

            out.append("warp_stair %s %d %d" % (name, x, y))
        elif tile in doors:

            out.append("warp_stair %s %d %d" % (name, x, y))
    return out

_WARP_PAD_CELLS = {}

def warp_pad_owned_cells(name):
    if name in _WARP_PAD_CELLS:
        return _WARP_PAD_CELLS[name]
    owned = set()
    for ln in scene_bindings(name):
        f = ln.split()

        if len(f) < 5 or f[0] != "scene_trigger":
            continue
        if not (f[2].isdigit() and f[3].isdigit()):
            continue
        sp = os.path.join(REPO, "mod_runtime", "scenes", f[4] + ".scene")
        try:
            body = open(sp, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for line in body.splitlines():
            if line.split("#")[0].strip().startswith("warp_pad "):
                owned.add((int(f[2]), int(f[3])))
                break
    _WARP_PAD_CELLS[name] = owned
    return owned

def warp_triggers(rom, map_id, name, grid):
    ob = parse_objects(rom, map_id)
    stem = name.lower()
    cells, blocks, seen, used = {}, {}, {}, {}

    pad_cells = warp_pad_owned_cells(name)

    dead_warps = dead_position_warp_indices(rom, map_id, name)
    for _i, (x, y, dm, dw) in enumerate(PO.warps_for_map(map_id, ob["warps"])):
        if _i in dead_warps:
            continue
        key = grid.get((x, y))
        if key is None:
            continue
        if (x, y) in pad_cells:

            continue
        if dm == 255:

            override = WARP_LAST_OVERRIDE.get((name, x, y))
            if override is not None:
                dest, dw = override.split()
                dw = int(dw)
            else:
                back = lastmap_destination(rom, map_id)
                dest = (BP.MAP_NAMES[back] if back is not None
                        and back < len(BP.MAP_NAMES) and BP.MAP_NAMES[back]
                        else "last")
        elif dm < len(BP.MAP_NAMES) and BP.MAP_NAMES[dm]:
            dest = BP.MAP_NAMES[dm]
        else:
            dest = str(dm)
        g = (key, dest, dw)
        if g not in seen:

            seen[g] = "kanto_w%08x" % (
                zlib.crc32(repr((map_id, g)).encode("ascii")) & 0xFFFFFFFF)
            blocks[seen[g]] = g
        cells[(x, y)] = seen[g]
    return cells, blocks

def quad_name(key, tileset):
    return "kanto_q%08x" % (zlib.crc32(repr((tileset, key)).encode("ascii"))
                            & 0xFFFFFFFF)

def tileset_of(rom, map_id):
    _bank, hdr = map_header(rom, map_id)
    return rom.data[hdr]

def tileset_parts(rom, map_id):
    ts = tileset_of(rom, map_id)
    reg = {a["name"]: a for a in BP.ASSETS}
    blocks = reg[BP.tileset_asset_name(ts, 0)]["fn"](rom)
    gfx = reg[BP.tileset_asset_name(ts, 1)]["fn"](rom)
    coll = reg[BP.tileset_asset_name(ts, 2)]["fn"](rom)
    ga = reg["gTilesetGrassAnim"]["fn"](rom)
    return blocks, gfx, set(coll) - {0xFF}, ga[ts * 2]

def quads(rom, map_id):
    blocks, _gfx, coll, grass = tileset_parts(rom, map_id)
    _bank, hdr = map_header(rom, map_id)
    w, h = rom.data[hdr + 2], rom.data[hdr + 1]
    reg = {a["name"]: a for a in BP.ASSETS}
    src = BP.map_blocks_source(map_id)
    cells_blk = reg[BP.map_asset_name(src)]["fn"](rom)

    grid = {}
    defs = {}
    for by in range(h):
        for bx in range(w):
            bid = cells_blk[by * w + bx]
            base = bid * 16
            if base + 16 > len(blocks):
                continue
            for cy in range(2):
                for cx in range(2):
                    tl = blocks[base + (cy * 2) * 4 + cx * 2]
                    tr = blocks[base + (cy * 2) * 4 + cx * 2 + 1]
                    bl = blocks[base + (cy * 2 + 1) * 4 + cx * 2]
                    br = blocks[base + (cy * 2 + 1) * 4 + cx * 2 + 1]
                    key = (tl, tr, bl, br)
                    defs[key] = (bl in coll, bl == grass)
                    grid[(bx * 2 + cx, by * 2 + cy)] = key
    return defs, grid, w * 2, h * 2

SHADES = ((0xFF, 0xFF, 0xFF), (0xAA, 0xAA, 0xAA),
          (0x55, 0x55, 0x55), (0x00, 0x00, 0x00))

KANTO_SHARED_TILESET = {
    3: 0, 5: 0, 6: 0, 7: 0, 8: 0, 9: 0, 10: 0, 11: 0, 12: 0, 13: 5,
    14: 0, 15: 0, 16: 5, 17: 5, 18: 0, 20: 0, 21: 0, 22: 0, 23: 0, 24: 0,
    25: 0, 26: 0, 27: 0, 28: 0, 29: 5, 30: 5, 31: 0, 34: 0, 35: 0, 37: 0,
    38: 0, 40: 0, 41: 0, 42: 0, 43: 0, 44: 0, 50: 0, 51: 0, 52: 5, 54: 5,
    55: 5, 56: 0, 57: 0, 58: 0, 59: 0, 67: 5, 70: 0, 71: 0, 75: 0, 78: 0,
    79: 0, 82: 0, 83: 0, 84: 0, 85: 0, 86: 0, 87: 0, 88: 5, 89: 5, 90: 5,
    91: 5, 92: 0, 93: 0, 94: 5, 95: 5,
}

def emit_shared_art(rom, outdir):
    reg = {a["name"]: a for a in BP.ASSETS}
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for idx, ts in sorted(KANTO_SHARED_TILESET.items()):
        gfx = reg[BP.tileset_asset_name(ts, 1)]["fn"](rom)
        write_tile(gfx[idx * 16:(idx + 1) * 16],
                   os.path.join(outdir, "kanto_t%03d" % idx))
        n += 1
    return n

def _png_gray8(pixels, w, h):
    import struct
    import zlib

    raw = b"".join(b"\x00" + bytes(pixels[y * w:(y + 1) * w]) for y in range(h))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))

def write_tile(blob, stem_path):
    with open(stem_path + ".bin", "wb") as fh:
        fh.write(blob)
    px = bytearray(64)
    for row in range(8):
        lo, hi = blob[row * 2], blob[row * 2 + 1]
        for b in range(8):
            v = ((lo >> (7 - b)) & 1) | (((hi >> (7 - b)) & 1) << 1)
            px[row * 8 + b] = SHADES[v][0]
    with open(stem_path + ".png", "wb") as fh:
        fh.write(_png_gray8(px, 8, 8))

def subtile_name(tileset, tile):
    return "kanto_ts%02d_t%03d" % (tileset, tile)

def emit_art(rom, map_id, name, outdir):
    _blocks, gfx, _coll, _grass = tileset_parts(rom, map_id)
    os.makedirs(outdir, exist_ok=True)
    ts = tileset_of(rom, map_id)
    count = len(gfx) // 16
    for i in range(count):
        write_tile(gfx[i * 16:(i + 1) * 16],
                   os.path.join(outdir, subtile_name(ts, i)))
    return count

STAY_MOVEMENT = {0xD0: "stay", 0xD1: "face_up",
                 0xD2: "face_left", 0xD3: "face_right",
                 0x10: "stay"}

def movement_word(mv, rng):
    if mv == 0xFF:
        return STAY_MOVEMENT.get(rng, "look_around")
    if mv == 0xFE:
        return RANGE_MOVEMENT.get(rng, "walk_random")
    return "stay"

def escape(s):

    return (s.replace("\\", "\\\\").replace('"', '\\"')
             .replace("\n", "\\n").replace("\f", "\\f")
             .replace("\x0b", "\\c"))

_TOGGLE_OFF = None

def toggle_off_objects(rom):
    global _TOGGLE_OFF
    if _TOGGLE_OFF is not None:
        return _TOGGLE_OFF
    out = {}
    try:
        bank, addr = rom.sym["ToggleableObjectStates"]
        end_bank, end_addr = rom.sym["MarkTownVisitedAndLoadToggleableObjects"]
    except (KeyError, TypeError):
        _TOGGLE_OFF = out
        return out

    base = bank * 0x4000 + (addr - 0x4000)
    stop = end_bank * 0x4000 + (end_addr - 0x4000)
    q = base

    OFF, ON = 0x11, 0x15
    while q + 2 < stop and q + 2 < len(rom.data):
        m, oid, state = rom.data[q], rom.data[q + 1], rom.data[q + 2]
        if m == 0xFF:
            break
        if state not in (OFF, ON):
            break
        if state == OFF:
            out.setdefault(m, set()).add(oid)
        q += 3
    _TOGGLE_OFF = out
    return out

SUPPRESS_STATIC_NPC = {
    ("PalletTown", "OAK"),
}

SUPPRESS_STATIC_NPC_AT = {
    ("OaksLab", "OAK", 5, 10),
}

EXTRA_NPCS = {
    "OaksLab": [
        ("BLUE", 4, 3, "look_around", "_OaksLabRivalGrampsIsntAroundText", [
            ("text_if", "EVENT_GOT_STARTER", "_OaksLabRivalMyPokemonLooksStrongerText"),
            ("text_if", "EVENT_FOLLOWED_OAK_INTO_LAB_2", "_OaksLabRivalGoAheadAndChooseText"),
            ("hide_if", "EVENT_BATTLED_RIVAL_IN_OAKS_LAB"),
        ]),
    ],
    "RocketHideoutB4F": [
        ("GIOVANNI", 25, 3, "stay", None, [
            ("hide_if", "EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI"),
        ]),
    ],
    "SilphCo11F": [
        ("GIOVANNI", 6, 9, "stay", "_SilphCo11FGiovanniText", [
            ("hide_if", "EVENT_BEAT_SILPH_CO_GIOVANNI"),
        ]),
    ],
    "Route24": [
        ("COOLTRAINER_M", 11, 15, "face_left",
         "_Route24CooltrainerM1YouCouldBecomeATopLeaderText", [
            ("hide_if", "EVENT_LEFT_BILLS_HOUSE_AFTER_HELPING"),
        ]),
    ],

}

EXTRA_GATES = {
    "BillsHouse": [
        ("MONSTER", 6, 5, ["hide_if EVENT_BILL_SAID_USE_CELL_SEPARATOR"]),
        ("SUPER_NERD", 4, 4, ["hide_if EVENT_LEFT_BILLS_HOUSE_AFTER_HELPING",
                               "show_if EVENT_MET_BILL"]),
        ("SUPER_NERD", 6, 5, ["show_if EVENT_LEFT_BILLS_HOUSE_AFTER_HELPING"]),
    ],
    "BluesHouse": [
        ("DAISY", 2, 3, ["hide_if EVENT_GOT_TOWN_MAP"]),
        ("DAISY", 6, 4, ["show_if EVENT_GOT_TOWN_MAP"]),
        ("POKEDEX", 3, 3, ["hide_if EVENT_GOT_TOWN_MAP"]),
    ],
    "CeladonMansionRoofHouse": [
        ("POKE_BALL", 4, 3, ["hide_if EVENT_GOT_EEVEE"]),
    ],
    "CeruleanCity": [
        ("GUARD", 28, 12, ["show_if EVENT_GOT_SS_TICKET"]),
        ("GUARD", 27, 12, ["hide_if EVENT_GOT_SS_TICKET"]),

        ("BLUE", 20, 2, ["hide_if EVENT_BEAT_CERULEAN_RIVAL"]),

        ("COOLTRAINER_F", 29, 26, [
            ("_TR", 76, "CeruleanCityCooltrainerF1Text.SlowbroUseSonicboomText"),
            ("_TR", 80, "CeruleanCityCooltrainerF1Text.SlowbroPunchText"),
        ]),
        ("MONSTER", 28, 26, [
            ("_TR", 76, "CeruleanCitySlowbroText.TookASnoozeText"),
            ("_TR", 60, "CeruleanCitySlowbroText.IsLoafingAroundText"),
            ("_TR", 60, "CeruleanCitySlowbroText.TurnedAwayText"),
        ]),
    ],
    "SSAnneKitchen": [

        ("COOK", 11, 13, [
            ("_TR", 128, ("SSAnneKitchenCook7Text.MainCourseIsText",
                          "SSAnneKitchenCook7Text.SalmonDuSaladText")),
            ("_TR", 64, ("SSAnneKitchenCook7Text.MainCourseIsText",
                         "SSAnneKitchenCook7Text.EelsAuBarbecueText")),
        ]),
    ],
    "FightingDojo": [
        ("POKE_BALL", 4, 1, ["hide_if EVENT_GOT_HITMONLEE"]),
        ("POKE_BALL", 5, 1, ["hide_if EVENT_GOT_HITMONCHAN"]),
    ],
    "MrFujisHouse": [
        ("MR_FUJI", 3, 1, ["show_if EVENT_RESCUED_MR_FUJI"]),
    ],
    "MtMoonB2F": [
        ("FOSSIL", 12, 6, ["hide_if EVENT_GOT_DOME_FOSSIL", "hide_if EVENT_GOT_HELIX_FOSSIL"]),
        ("FOSSIL", 13, 6, ["hide_if EVENT_GOT_DOME_FOSSIL", "hide_if EVENT_GOT_HELIX_FOSSIL"]),
    ],
    "OaksLab": [
        ("OAK", 5, 2, ["show_if EVENT_FOLLOWED_OAK_INTO_LAB_2"]),

        ("POKEDEX", 2, 1, ["hide_if EVENT_GOT_POKEDEX"]),
        ("POKEDEX", 3, 1, ["hide_if EVENT_GOT_POKEDEX"]),

        ("POKE_BALL", 6, 3, ["hide_if EVENT_HIDE_STARTER_BALL_1"]),
        ("POKE_BALL", 7, 3, ["hide_if EVENT_HIDE_STARTER_BALL_2"]),
        ("POKE_BALL", 8, 3, ["hide_if EVENT_HIDE_STARTER_BALL_3"]),
    ],
    "PewterCity": [
        ("YOUNGSTER", 35, 16, ["hide_if EVENT_BEAT_BROCK"]),
    ],
    "PokemonTower2F": [
        ("BLUE", 14, 5, ["hide_if EVENT_BEAT_POKEMON_TOWER_RIVAL"]),
    ],
    "PokemonTower7F": [
        ("MR_FUJI", 10, 3, ["hide_if EVENT_RESCUED_MR_FUJI"]),
    ],
    "Route12": [
        ("SNORLAX", 10, 62, ["hide_if EVENT_BEAT_ROUTE12_SNORLAX"]),
    ],
    "Route16": [
        ("SNORLAX", 26, 10, ["hide_if EVENT_BEAT_ROUTE16_SNORLAX"]),
    ],

    "Route22": [
        ("BLUE", 25, 5, ["show_if EVENT_1ST_ROUTE22_RIVAL_BATTLE"], 0),
        ("BLUE", 25, 5, ["show_if EVENT_2ND_ROUTE22_RIVAL_BATTLE"], 1),
    ],
    "SaffronCity": [
        ("ROCKET", 7, 6, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 20, 8, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 34, 4, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 13, 12, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 11, 25, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 32, 13, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 18, 30, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKET", 18, 22, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI", "hide_if EVENT_RESCUED_MR_FUJI"]),
        ("ROCKET", 19, 22, ["hide_if EVENT_BEAT_SILPH_CO_GIOVANNI", "show_if EVENT_RESCUED_MR_FUJI"]),
        ("SCIENTIST", 8, 14, ["show_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("SILPH_WORKER_M", 23, 23, ["show_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("SILPH_WORKER_F", 17, 30, ["show_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("GENTLEMAN", 30, 12, ["show_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("BIRD", 31, 12, ["show_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
        ("ROCKER", 18, 8, ["show_if EVENT_BEAT_SILPH_CO_GIOVANNI"]),
    ],
    "SilphCo7F": [
        ("BLUE", 3, 7, ["hide_if EVENT_BEAT_SILPH_CO_RIVAL"]),
    ],
    "ViridianCity": [
        ("GAMBLER_ASLEEP", 18, 9, ["hide_if EVENT_GOT_POKEDEX"]),
        ("GAMBLER", 17, 5, ["show_if EVENT_GOT_POKEDEX"]),
    ],
}

def rom_text(rom, symbol):
    off = rom.offset(symbol)
    txt, _ = G.decode_text(rom, off)
    return txt or ""

def emit(rom, map_id, name):
    ob = parse_objects(rom, map_id)
    L = ["# GENERATED by tools/romimport/emit_kanto.py from the user's own ROM.",
         "# DO NOT EDIT -- regenerate with: emit_kanto.py --all",
         "#",
         "# This is the ONE source of %s's geometry/NPCs/warps. Do NOT create" % name,
         "# mod_runtime/blocks/%s.block to 'override' it -- authored maps and" % name,
         "# generated maps must never coexist (docs/amberscript/pipeline.md).",
         "# Tile properties go in mod_runtime/blocks/vmap_%s_properties.block;" % name,
         "# dialogue set-pieces go in mod_runtime/scenes/.",
         ""]

    tile_occurrence = {}
    for r in ob["npcs"]:
        sn = sprite_names().get(r["sprite"], str(r["sprite"]))

        occ_key = (sn, r["x"], r["y"])
        occ = tile_occurrence.get(occ_key, 0)
        tile_occurrence[occ_key] = occ + 1
        txt = r["text"]
        if (name, sn) in SUPPRESS_STATIC_NPC or (name, sn, r["x"], r["y"]) in SUPPRESS_STATIC_NPC_AT:
            L.append("# %s at (%d,%d) suppressed here -- owned entirely by a scene "
                     "(see SUPPRESS_STATIC_NPC/SUPPRESS_STATIC_NPC_AT in emit_kanto.py), "
                     "not a static npc." % (sn, r["x"], r["y"]))
            continue

        starts_off = r.get("obj_id") in toggle_off_objects(rom).get(map_id, ())

        if starts_off:
            for _e in EXTRA_GATES.get(name, []):
                _gs, _gx, _gy, _gl = _e[:4]
                _occ = _e[4] if len(_e) > 4 else None
                if _occ is not None and _occ != occ:
                    continue
                if (sn, r["x"], r["y"]) != (_gs, _gx, _gy):
                    continue
                if any(isinstance(l, str) and l.startswith("show_if") for l in _gl):
                    starts_off = False
                    break
        override_sym = SCRIPT_NPC_TEXT_OVERRIDE.get((name, sn, r["x"], r["y"]))
        if override_sym:

            osyms = override_sym if isinstance(override_sym, tuple) else (override_sym,)
            txt = "\f".join(rom_text(rom, s) for s in osyms)
        if txt is None:
            txt = SCRIPT_NPC_SERVICE.get((name, sn, r["x"], r["y"]))
        if txt is None:

            L.append("# script NPC -- driven by %s in the ROM."
                     % (r["script"] or "an unnamed routine"))
            L.append("# Bind behaviour in the override layer:")
            L.append("#     scene_npc %s <scene> %d %d"
                     % (name, r["x"], r["y"]))
        L.append('npc %s %s %d %d %s %s'
                 % (name, sn, r["x"], r["y"], movement_word(r["mv"], r["range"]),
                    '"%s"' % escape(txt) if txt else '""'))
        hide_flag = NPC_HIDE_IF.get((name, r["x"], r["y"]))
        if hide_flag:
            L.append("hide_if %s" % hide_flag)
        if starts_off:
            L.append("hidden")
        if r["cond"]:
            _d, flag, variant = r["cond"]
            L.append('text_if %s "%s"'
                     % (event_flag_names().get(flag, str(flag)), escape(variant)))
        for entry in EXTRA_GATES.get(name, []):
            g_sprite, g_x, g_y, g_lines = entry[:4]
            want_occ = entry[4] if len(entry) > 4 else None
            if want_occ is not None and want_occ != occ:
                continue
            if (sn, r["x"], r["y"]) == (g_sprite, g_x, g_y):
                for gl in g_lines:

                    if isinstance(gl, tuple) and gl[0] == "_TR":
                        _tr, weight, sym = gl

                        syms = sym if isinstance(sym, tuple) else (sym,)
                        txt = "\f".join(rom_text(rom, s) for s in syms)
                        L.append('text_random %d "%s"' % (weight, escape(txt)))
                    else:
                        L.append(gl)
    if name in EXTRA_NPCS:
        L.append("# Synthetic fixture(s) -- no ROM object-event, see EXTRA_NPCS above.")
        for sprite, x, y, move, text_symbol, extra in EXTRA_NPCS[name]:
            txt = rom_text(rom, text_symbol) if text_symbol else ""
            L.append('npc %s %s %d %d %s %s'
                     % (name, sprite, x, y, move,
                        '"%s"' % escape(txt) if txt else '""'))
            for line in extra:
                if line[0] == "text_if":
                    _kind, event_name, sym = line
                    L.append('text_if %s "%s"' % (event_name, escape(rom_text(rom, sym))))
                elif line[0] == "hide_if":
                    L.append("hide_if %s" % line[1])
    sign_cells = set()
    for (x, y, txt, cond, facing) in ob["signs"]:
        sign_cells.add((x, y))
        if txt:
            L.append('hidden_event %s %d %d "%s"%s'
                     % (name, x, y, escape(txt),
                        (" " + facing) if facing else ""))
            if cond:
                _d, flag, variant = cond
                L.append('text_if %s "%s"'
                         % (event_flag_names().get(flag, str(flag)),
                            escape(variant)))

    L.extend(hidden_object_events(rom, map_id, name, sign_cells))
    L.extend(hidden_object_directives(rom, map_id, name, sign_cells))

    _bdefs, bgrid, _bw, _bh = quads(rom, map_id)
    L.extend(bookshelf_events(rom, map_id, name, bgrid, sign_cells))

    warps = PO.warps_for_map(map_id, ob["warps"])
    dead_warps = dead_position_warp_indices(rom, map_id, name)
    for i, (x, y, _dm, _dw) in enumerate(warps):

        if i in dead_warps:
            L.append("# warp %d at (%d,%d) omitted: position warp on a map whose"
                     % (i, x, y))
            L.append("# ExtraWarpCheck is IsPlayerFacingEdgeOfMap, mid-map --")
            L.append("# it cannot fire in the ROM. See dead_position_warp_indices.")
            continue
        L.append("warpspot %s %d %d %d" % (name, i, x, y))

    defs, grid, gw, gh = quads(rom, map_id)
    tiles_used = set()
    for d, dest, _c, _a in connections(rom, map_id):

        try:
            if tileset_of(rom, dest) != tileset_of(rom, map_id):
                continue
            ndefs, _ng, _w, _h = quads(rom, dest)
        except (KeyError, IndexError):
            continue
        defs.update(ndefs)

    for key, props in block_quads(rom, map_id, border_block(rom, map_id)):
        defs.setdefault(key, props)

    qts = tileset_of(rom, map_id)

    cutrep = cut_replacements(rom, map_id, qts)
    cutrep = {k: v for k, v in cutrep.items() if k in defs}
    qcounter = counter_tiles(rom, qts)
    for _kb, (key_after, props_after) in cutrep.items():
        defs.setdefault(key_after, props_after)

    tile_aliases = script_tile_alias_quads(rom, map_id, name)

    L.append("")
    L.append("gbc_tileset %s %s" % (name, tileset_name(rom, map_id)))
    for key in sorted(defs):
        tiles_used.update(key)
    for _an, akey, _ap, _cx, _cy in tile_aliases:
        tiles_used.update(akey)
    stem = name.lower()
    for t in sorted(tiles_used):
        L.append("subtile %s %s/%s.png"
                 % (subtile_name(qts, t), ART_REL, subtile_name(qts, t)))
    L.append("")
    for key in sorted(defs):
        passable, grass = defs[key]
        L.append("block %s" % quad_name(key, qts))
        L.append("    source quad %s" % " ".join(subtile_name(qts, t)
                                                 for t in key))
        L.append("    passable %s" % ("yes" if passable else "no"))
        if grass:
            L.append("    grass yes")

        if quad_is_cuttable(key, qts):
            L.append("    cuttable yes")
            rep = cutrep.get(key)
            if rep is not None:
                L.append("    cut_replacement %s" % quad_name(rep[0], qts))

        if key[2] in qcounter:
            L.append("    counter yes")
        L.append("end")

    awcells, awblocks = warp_triggers(rom, map_id, name, grid)
    for aname, akey, apass, acx, acy in tile_aliases:
        L.append("block %s" % aname)
        L.append("    source quad %s" % " ".join(subtile_name(qts, t)
                                                 for t in akey))
        L.append("    passable %s" % ("yes" if apass else "no"))

        wname = awcells.get((acx, acy))
        if wname:
            L.append("    warp %s %d" % (awblocks[wname][1], awblocks[wname][2]))
        L.append("end")

    _wcells, wblocks = warp_triggers(rom, map_id, name, grid)
    for bn in sorted(wblocks):
        key, dest, idx = wblocks[bn]
        passable, grass = defs[key]
        L.append("block %s" % bn)
        L.append("    source quad %s" % " ".join(subtile_name(qts, t)
                                                 for t in key))

        L.append("    passable %s" % ("yes" if passable else "no"))
        if grass:
            L.append("    grass yes")

        if key[2] in qcounter:
            L.append("    counter yes")
        L.append("    warp %s %d" % (dest, idx))
        L.append("end")

    L.append("")
    L.append("mapsize %s %d %d" % (name, gw // 2, gh // 2))
    bq = block_quads(rom, map_id, border_block(rom, map_id))
    if bq:
        L.append("border %s %s"
                 % (name, " ".join(quad_name(k, qts) for k, _p in bq)))
    if is_indoor(rom, map_id):
        L.append("indoor %s" % name)
    if map_id in DARK_MAPS:
        L.append("dark %s" % name)

    for d, dest, coord, adj in connections(rom, map_id):
        L.append("connect %s %s %s %d %d"
                 % (name, d, BP.MAP_NAMES[dest], coord, adj))

    L.extend(scene_bindings(name))
    L.extend(gym_leader_lines(rom, map_id, name))
    L.extend(trainer_lines(rom, map_id, name))

    if name in MAP_SCRIPTS:
        L.append("map_script %s %s" % (name, MAP_SCRIPTS[name]))
    L.extend(static_encounter_lines(rom, map_id, name))
    L.extend(item_ball_lines(rom, map_id, name))
    L.extend(warp_kind_lines(rom, map_id, name, grid, defs, gw, gh))
    L.extend(wild_lines(rom, map_id, name))

    track = map_music(rom, map_id)
    if track:
        L.append("music %s %s" % (name, track))
    return "\n".join(L) + "\n"

def emit_grid(rom, map_id, name):
    _defs, grid, gw, gh = quads(rom, map_id)

    wcells, _wblocks = warp_triggers(rom, map_id, name, grid)
    out = []
    for y in range(gh):
        for x in range(gw):
            key = grid.get((x, y))
            if key is None:
                continue
            out.append("%d %d custom %s"
                       % (x, y, wcells.get((x, y))
                          or quad_name(key, tileset_of(rom, map_id))))
    return "\n".join(out) + "\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    ap.add_argument("--map", type=int, default=0)
    ap.add_argument("--print", action="store_true", dest="show")
    ap.add_argument("--art-all", action="store_true", dest="art_all",
                    help="regenerate every Kanto tile into "
                         "mod_runtime/custom_art/kanto (the paths the authored "
                         ".block files already reference)")
    ap.add_argument("--all", action="store_true", dest="do_all",
                    help="emit EVERY Kanto map into mod_runtime/generatedmaps/"
                         "kanto/{blocks,map_edits}. The engine checks that root "
                         "BEFORE mod_runtime/blocks, so the generated set "
                         "shadows the hand-authored one without touching it -- "
                         "delete or rename the directory to go back. Needs "
                         "--art-all to have been run once.")
    ap.add_argument("--all-out", dest="all_out",
                    help="where --all writes, if not "
                         "mod_runtime/generatedmaps/kanto")
    ap.add_argument("--art-out", dest="art_out",
                    help="where --art-all writes, if not this checkout's "
                         "mod_runtime/custom_art/kanto. A DISTRIBUTED build "
                         "needs this: the frozen first-run setup lives in a "
                         "temporary unpack directory, so a REPO-relative "
                         "destination would write 20,900 files into a folder "
                         "that disappears when it exits.")
    a = ap.parse_args()
    from gen1_rom import default_paths
    dr, ds = default_paths(REPO)
    try:
        rom = Gen1Rom(a.rom or dr, a.sym or ds)
    except RomError as e:
        sys.exit("error: %s" % e)

    if a.art_all:
        art = a.art_out or os.path.join(REPO, "mod_runtime",
                                        "custom_art", "kanto")
        n = 0
        for i, nm in enumerate(BP.MAP_NAMES):
            if i in BP.MAP_FILLER or i >= BP.NUM_REAL_MAPS or not nm:
                continue
            try:
                n += emit_art(rom, i, nm, art)
            except (RomError, KeyError, IndexError):
                continue
        n += emit_shared_art(rom, art)
        print("wrote %d tiles to %s" % (n, art))
        return

    if a.do_all:

        root = a.all_out or os.path.join(
            REPO, "mod_runtime", "generatedmaps",
            BP.ROM_PACKAGE_ID.get(rom.sha1, "red"))
        bd = os.path.join(root, "blocks")
        gd = os.path.join(root, "map_edits")
        os.makedirs(bd, exist_ok=True)
        os.makedirs(gd, exist_ok=True)
        ok = skipped = failed = 0
        for i, nm in enumerate(BP.MAP_NAMES):
            if not nm or i >= BP.NUM_REAL_MAPS or i in BP.MAP_FILLER:
                continue

            if is_duplicate_label(i):
                skipped += 1
                continue
            try:
                text = emit(rom, i, nm)
                grid = emit_grid(rom, i, nm)
            except Exception as e:
                failed += 1
                print("  FAILED %s (%s: %s)" % (nm, type(e).__name__, e))
                continue

            over = [ln.split()[1] for ln in text.splitlines()
                    if ln.startswith("block ") and len(ln.split()[1]) > NAME_MAX]
            if over:
                failed += 1
                print("  FAILED %s: %d block name(s) over %d chars, which the "
                      "engine truncates silently -- e.g. %s (%d)"
                      % (nm, len(over), NAME_MAX, over[0], len(over[0])))
                continue
            with open(os.path.join(bd, nm + ".block"), "w",
                      encoding="utf-8") as fh:
                fh.write(text)
            with open(os.path.join(gd, "vmap_%s.txt" % nm), "w",
                      encoding="utf-8") as fh:
                fh.write(grid)
            ok += 1
        print("wrote %d maps to %s" % (ok, root))
        if skipped:
            print("  %d duplicate-label ids skipped" % skipped)
        if failed:
            print("  %d FAILED -- those maps will fall back to the "
                  "hand-authored file" % failed)
        return

    name = BP.MAP_NAMES[a.map]
    text = emit(rom, a.map, name)
    if a.show:
        sys.stdout.buffer.write(text.encode("utf-8"))
        return
    d = os.path.join(OUT, "blocks")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name + ".block"), "w", encoding="utf-8") as fh:
        fh.write(text)
    print("wrote %s/blocks/%s.block" % (OUT, name))

    g = os.path.join(OUT, "map_edits")
    os.makedirs(g, exist_ok=True)
    gp = os.path.join(g, "vmap_%s.txt" % name)
    with open(gp, "w", encoding="utf-8") as fh:
        fh.write(emit_grid(rom, a.map, name))
    print("wrote %s/map_edits/vmap_%s.txt" % (OUT, name))

if __name__ == "__main__":
    main()
