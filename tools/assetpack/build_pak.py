
import argparse
import bisect
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "romimport"))

sys.path.insert(0, str(Path(__file__).resolve().parent))

import gen1_pic
from gen1_rom import Gen1Rom, RomError, default_paths
from pak import PakBuilder

PACKAGE_SCHEMA_VERSION = 1
IMPORTER_VERSION = "0.1.0"

PACKAGE_ROMS = {
    "red":     ("pokered-master/pokered.gbc",          "pokered-master/pokered.sym"),
    "blue":    ("pokered-master/pokeblue.gbc",         "pokered-master/pokeblue.sym"),
    "crystal": ("pokecrystal-master/pokecrystal.gbc",  "pokecrystal-master/pokecrystal.sym"),
}

ROM_PACKAGE_ID = {
    "ea9bcae617fdf159b045185467ae58b2e4a48b9a": "red",
    "d7037c83e1ae5b39bde3c30787637ba1d4c48ce2": "blue",
}

REPO = Path(getattr(sys, "_MEIPASS", None)
            or Path(__file__).resolve().parent.parent.parent)

ASSETS = []

def write_text_lf(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)

def asset(name, stride=1, verify=None, symbol=None, shape=None, expect_diff=None,
          bind=False, elem=None, elem_type="uint8_t", fields=None, count=None,
          ctype=None, ctype_header=None, cut=False, package="red"):
    def deco(fn):
        ASSETS.append({
            "name": name, "stride": stride, "verify": verify,
            "symbol": symbol or name, "shape": shape,
            "fields": fields, "count": count,
            "expect_diff": expect_diff, "bind": bind, "cut": cut,
            "ctype": ctype, "ctype_header": ctype_header,
            "package": package,
            "elem": elem if elem is not None else
                    (f"[{stride}]" if stride > 1 else ""),
            "elem_type": elem_type,
            "fn": fn,
        })
        return fn
    return deco

def parse_c_struct(path, symbol, count, fields):
    offs, pos = {}, 0
    for nm, w, n in fields:
        offs[nm] = (pos, w, n)
        pos += w * n
    elem_bytes = pos

    text = _c_text(path)
    m = _find_definition(text, symbol)
    if not m:
        raise SystemExit(f"verify: symbol '{symbol}' not found in {path}")
    i = text.index("{", m.end())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                body = text[i + 1:j]
                break
    body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
    body = re.sub(r"//[^\n]*", " ", body)

    out = bytearray(count * elem_bytes)
    ENTRY = re.compile(r"\[\s*(0[xX][0-9a-fA-F]+|\d+)\s*\]\s*=\s*\{")
    p = 0
    while True:
        mm = ENTRY.search(body, p)
        if not mm:
            break
        d, k = 1, mm.end()
        while d:
            if body[k] == "{":
                d += 1
            elif body[k] == "}":
                d -= 1
            k += 1
        idx, txt, p = int(mm.group(1), 0), body[mm.end():k - 1], k
        if idx >= count:
            continue
        base = idx * elem_bytes
        for fm in re.finditer(r"\.(\w+)\s*=\s*(\{[^}]*\}|[^,}]+)", txt):
            nm, val = fm.group(1), fm.group(2)
            if nm not in offs:
                raise SystemExit(f"verify: {symbol} has unknown field .{nm}")
            fo, w, n = offs[nm]
            vals = [int(x, 0) for x in
                    re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", val)]
            for e, v in enumerate(vals[:n]):
                out[base + fo + e * w: base + fo + (e + 1) * w] = \
                    int(v).to_bytes(w, "little")
    return bytes(out)

def _parse_c_struct_positional(path, symbol, count, widths):
    text = _c_text(path)
    m = _find_definition(text, symbol)
    if not m:
        raise SystemExit(f"verify: symbol '{symbol}' not found in {path}")
    i = text.index("{", m.end())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                body = text[i + 1:j]
                break
    body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
    body = re.sub(r"//[^\n]*", " ", body)

    elems = {}
    depth, start, nxt = 0, None, 0
    idx_re = re.compile(r"\[\s*(0[xX][0-9a-fA-F]+|\d+)\s*\]\s*=\s*$")
    for k, ch in enumerate(body):
        if ch == "{":
            if depth == 0:
                start = k
                mm = idx_re.search(body[:k].rstrip()[-24:].strip() or "")
                cur = int(mm.group(1), 0) if mm else nxt
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                elems[cur] = body[start + 1:k]
                nxt = cur + 1

    elem_bytes = sum(widths)
    out = bytearray(count * elem_bytes)
    for idx, txt in elems.items():
        if idx >= count:
            continue
        vals = [int(x, 0) for x in
                re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", txt)]
        pos = idx * elem_bytes
        for w, v in zip(widths, vals):
            out[pos:pos + w] = int(v).to_bytes(w, "little")
            pos += w
    return bytes(out)

_C_TEXT_CACHE = {}

def _c_text(path):
    key = str(path)
    if key not in _C_TEXT_CACHE:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
        text = re.sub(r"/\*.*?\*/|//[^\n]*",
                      lambda m: re.sub(r"[^\n]", " ", m.group(0)),
                      text, flags=re.S)
        _C_TEXT_CACHE[key] = text
    return _C_TEXT_CACHE[key]

def _find_definition(text, symbol):
    return re.search(rf"\b{re.escape(symbol)}\s*\[[^=;{{}}]*\]\s*=\s*\{{", text)

def parse_c_bytes(path, symbol, shape=None):
    text = _c_text(path)
    m = _find_definition(text, symbol)
    if not m:
        raise SystemExit(f"verify: symbol '{symbol}' not found in {path}")

    i = text.index("{", m.end())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                body = text[i:j + 1]
                break
    else:
        raise SystemExit(f"verify: unterminated initialiser for '{symbol}' in {path}")

    body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
    body = re.sub(r"//[^\n]*", " ", body)

    def scalars(s):

        out = bytearray()
        for tok in re.finditer(r"-?\s*(?:0[xX][0-9a-fA-F]+|\b\d+\b)", s):
            raw = tok.group(0).replace(" ", "")
            v = int(raw, 0)
            if -0x80 <= v < 0:
                v &= 0xFF
            if not 0 <= v <= 0xFF:
                raise SystemExit(f"verify: {symbol} in {path} has non-byte value {v}")
            out.append(v)
        return bytes(out)

    if not shape:
        return scalars(body)

    def split_groups(s):
        groups, depth, start = [], 0, None
        for k, ch in enumerate(s):
            if ch == "{":
                depth += 1
                if depth == 1:
                    start = k
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    groups.append(s[start + 1:k])
        return groups

    def build(s, dims):
        if len(dims) == 1:
            b = scalars(s)
            if len(b) > dims[0]:
                raise SystemExit(f"verify: {symbol} row has {len(b)} > {dims[0]} bytes")
            return b + bytes(dims[0] - len(b))
        groups = split_groups(s)
        if len(groups) > dims[0]:
            raise SystemExit(f"verify: {symbol} has {len(groups)} > {dims[0]} entries")
        inner = 1
        for d in dims[1:]:
            inner *= d
        out = bytearray()
        for g in groups:
            out += build(g, dims[1:])
        return bytes(out) + bytes((dims[0] - len(groups)) * inner)

    return build(body[1:-1], list(shape))

@asset("gCutTreeAnimTiles", stride=16,
       verify="src/data/cut_anim_tiles.c", bind=True, elem="[16]")
def cut_tree_anim_tiles(rom):
    base = rom.offset("Overworld_GFX")
    out = bytearray()
    for tile in (0x2D, 0x2E, 0x3D, 0x3E):
        out += rom.read(base + tile * 16, 16)
    return bytes(out)

@asset("gPlayerGfx", stride=16, verify="src/data/player_sprite.c",
       bind=True, elem="[4][16]")
def player_gfx(rom):
    return rom.read("RedSprite", 6 * 4 * 16)

@asset("gBikePlayerGfx", stride=16, verify="src/data/bike_sprite.c",
       bind=True, elem="[4][16]")
def bike_player_gfx(rom):
    return rom.read("RedBikeSprite", 6 * 4 * 16)

@asset("gSeelSpriteGfx", stride=16, verify="src/data/seel_sprite.c",
       bind=True, elem="[4][16]")
def seel_sprite_gfx(rom):
    return rom.read("SeelSprite", 6 * 4 * 16)

@asset("kGhostFrontSprite", stride=16, verify="src/data/ghost_front_sprite.c",
       bind=True, elem="[16]")
def ghost_front_sprite(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("GhostPic"))
    return gen1_pic.to_canvas(tiles, w, h)

def _mon_pic(rom, symbol):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset(symbol))
    return gen1_pic.to_canvas(tiles, w, h)

@asset("gFossilAerodactylSprite", stride=16, verify="src/data/fossil_sprites.c",
       bind=True, elem="[16]")
def fossil_aerodactyl(rom):
    return _mon_pic(rom, "FossilAerodactylPic")

@asset("gFossilKabutopsSprite", stride=16, verify="src/data/fossil_sprites.c",
       bind=True, elem="[16]")
def fossil_kabutops(rom):
    return _mon_pic(rom, "FossilKabutopsPic")

_DEX_NAMES_H = REPO / "src" / "data" / "pokemon_names_gen.h"
_dex_syms_cache = None

def _dex_name_key(raw):
    s = re.sub(r"\\x([0-9A-Fa-f]{2})", lambda m: chr(int(m.group(1), 16)), raw)
    s = (s.replace("õ", "F").replace("ï", "M")
          .replace("♀", "F").replace("♂", "M"))
    return re.sub(r"[^A-Za-z]", "", s).lower()

def dex_pic_symbols(rom, kind="PicFront"):
    global _dex_syms_cache
    key = kind
    if _dex_syms_cache is None:
        _dex_syms_cache = {}
    if key in _dex_syms_cache:
        return _dex_syms_cache[key]

    text = _DEX_NAMES_H.read_text(encoding="utf-8", errors="replace")
    names = {int(d): n for d, n in re.findall(r'\[\s*(\d+)\]\s*=\s*"([^"]*)"', text)}
    syms = {s.lower(): s for s in rom.sym if s.endswith(kind)}

    out, missing = {}, []
    for dex in range(1, 152):
        cand = _dex_name_key(names.get(dex, "")) + kind.lower()
        if cand in syms:
            out[dex] = syms[cand]
        else:
            missing.append(dex)
    if missing:
        raise RomError(f"no {kind} symbol for dex {missing}")
    _dex_syms_cache[key] = out
    return out

@asset("gPokemonFrontSprite", stride=16, verify="src/data/pokemon_sprites.c",
       shape=(152, 49, 16), bind=True, elem="[49][16]")
def pokemon_front_sprites(rom):
    syms = dex_pic_symbols(rom, "PicFront")
    out = bytearray(bytes(49 * 16))
    for dex in range(1, 152):
        tiles, w, h = gen1_pic.decompress(rom.data, rom.offset(syms[dex]))
        out += gen1_pic.to_canvas(tiles, w, h)
    return bytes(out)

@asset("gPokemonBackSprite", stride=16, verify="src/data/pokemon_sprites.c",
       shape=(152, 49, 16), bind=True, elem="[49][16]")
def pokemon_back_sprites(rom):
    syms = dex_pic_symbols(rom, "PicBack")
    out = bytearray(bytes(49 * 16))
    for dex in range(1, 152):
        tiles, w, h = gen1_pic.decompress(rom.data, rom.offset(syms[dex]))
        out += gen1_pic.scale_by_two(tiles, w, h)
    return bytes(out)

@asset("gTrainerFrontSprite", stride=16, verify="src/data/trainer_sprites.c",
       shape=(47, 49, 16), bind=True, elem="[49][16]",
       expect_diff="entry 26 (CHIEF): the ROM aliases ChiefPic to ScientistPic "
                   ""
                   "ScientistPic's INCBIN, so both pointer-table entries read "
                   "0x627d). The committed data has YOUNGSTER there: the old "
                   "extractor worked from PNGs, found no chief.png, and "
                   "substituted one (extract_trainer_sprites.py:63). The ROM "
                   "value is correct; this fixes it.")
def trainer_front_sprites(rom):
    bank = rom.sym["YoungsterPic"][0]
    table = rom.offset("TrainerPicAndMoneyPointers")
    out = bytearray()
    for i in range(47):
        entry = rom.read(table + i * 5, 5)
        addr = entry[0] | (entry[1] << 8)
        off = bank * 0x4000 + (addr - 0x4000)
        tiles, w, h = gen1_pic.decompress(rom.data, off)
        out += gen1_pic.to_canvas(tiles, w, h)
    return bytes(out)

@asset("kHofRedFrontSprite", stride=16, verify="src/data/hof_player_sprites.h",
       shape=(49, 16), cut=True, bind=True, elem="[16]")
def hof_red_front(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("RedPicFront"))
    return gen1_pic.to_canvas(tiles, w, h)

@asset("kHofRedBackSprite", stride=16, verify="src/data/hof_player_sprites.h",
       shape=(49, 16), cut=True, bind=True, elem="[16]")
def hof_red_back(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("RedPicBack"))
    return gen1_pic.scale_by_two(tiles, w, h)

@asset("gOldManBackSprite", stride=16, shape=(49, 16), cut=True, bind=True, elem="[16]")
def old_man_back(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("OldManPicBack"))
    return gen1_pic.scale_by_two(tiles, w, h)

@asset("gIntroOakPic", stride=16, shape=(49, 16), cut=True, bind=True, elem="[16]")
def intro_oak_pic(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("ProfOakPic"))
    return gen1_pic.to_canvas(tiles, w, h)

@asset("gIntroRival1Pic", stride=16, shape=(49, 16), cut=True, bind=True, elem="[16]")
def intro_rival1_pic(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("Rival1Pic"))
    return gen1_pic.to_canvas(tiles, w, h)

@asset("gIntroShrinkPic1", stride=16, shape=(49, 16), cut=True, bind=True, elem="[16]")
def intro_shrink_pic1(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("ShrinkPic1"))
    return gen1_pic.to_canvas(tiles, w, h)

@asset("gIntroShrinkPic2", stride=16, shape=(49, 16), cut=True, bind=True, elem="[16]")
def intro_shrink_pic2(rom):
    tiles, w, h = gen1_pic.decompress(rom.data, rom.offset("ShrinkPic2"))
    return gen1_pic.to_canvas(tiles, w, h)

@asset("gPokemonFrontSpriteW", verify="src/data/pokemon_sprites.c",
       bind=True, elem="")
def pokemon_front_w(rom):
    syms = dex_pic_symbols(rom, "PicFront")
    return bytes([0] + [gen1_pic.decompress(rom.data, rom.offset(syms[d]))[1]
                        for d in range(1, 152)])

@asset("gPokemonFrontSpriteH", verify="src/data/pokemon_sprites.c",
       bind=True, elem="")
def pokemon_front_h(rom):
    syms = dex_pic_symbols(rom, "PicFront")
    return bytes([0] + [gen1_pic.decompress(rom.data, rom.offset(syms[d]))[2]
                        for d in range(1, 152)])

NUM_SPRITES = 73
SPRITE_TILES = 24
SPRITE_GFX_SIZE = SPRITE_TILES * 16

_symbol_offsets_cache = {}

def _all_symbol_offsets(rom):
    key = id(rom)
    if key not in _symbol_offsets_cache:
        _symbol_offsets_cache[key] = sorted(
            {a if a < 0x4000 else b * 0x4000 + (a - 0x4000)
             for b, a in rom.sym.values()})
    return _symbol_offsets_cache[key]

def _sprite_sheets(rom):
    table = rom.offset("SpriteSheetPointerTable")
    out = []
    for i in range(NUM_SPRITES - 1):
        e = rom.read(table + i * 4, 4)
        addr, length, bank = e[0] | (e[1] << 8), e[2], e[3]
        out.append((bank * 0x4000 + (addr - 0x4000), length))
    return out

@asset("gSpriteGfx", stride=SPRITE_GFX_SIZE, verify="src/data/sprite_data.c",
       shape=(NUM_SPRITES, SPRITE_GFX_SIZE), bind=True,
       elem=f"[{SPRITE_GFX_SIZE}]")
def sprite_gfx(rom):
    sheets = _sprite_sheets(rom)

    labels = _all_symbol_offsets(rom)

    out = bytearray(bytes(SPRITE_GFX_SIZE))
    for off, length in sheets:
        i = bisect.bisect_right(labels, off)
        extent = labels[i] - off if i < len(labels) else length
        gfx = rom.read(off, min(SPRITE_GFX_SIZE, max(length, extent)))
        out += gfx + bytes(SPRITE_GFX_SIZE - len(gfx))
    return bytes(out)

@asset("gSpriteTileCount", verify="src/data/sprite_data.c",
       shape=(NUM_SPRITES,), bind=True, elem="")
def sprite_tile_count(rom):
    return bytes([0] + [length // 16 for _, length in _sprite_sheets(rom)])

TILESET_HEADER_SIZE = 12
NUM_TILESETS = 24

def _tileset_headers(rom):
    base = rom.offset("Tilesets")
    out = []
    for i in range(NUM_TILESETS):
        e = rom.read(base + i * TILESET_HEADER_SIZE, TILESET_HEADER_SIZE)
        bank = e[0]
        ptrs = [(e[1 + n * 2] | (e[2 + n * 2] << 8)) for n in range(3)]

        out.append((bank, *[p if p < 0x4000 else bank * 0x4000 + (p - 0x4000)
                            for p in ptrs]))
    return out

def _tileset_c_names():
    path = REPO / "src" / "data" / "tileset_data.c"
    if not path.is_file():

        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    out = {}
    for m in re.finditer(r"\[\s*(\d+)\]\s*=\s*\{(.*?)\}", text, re.S):
        idx, body = int(m.group(1)), m.group(2)
        f = dict(re.findall(r"\.(\w+)\s*=\s*([A-Za-z_0-9]+)", body))
        if "blocks" in f:
            out[idx] = (f["blocks"], f.get("gfx"), f.get("coll_tiles"),
                        int(f.get("num_blocks", 0)), int(f.get("gfx_tiles", 0)))
    return out

def _blob_extent(rom, off, labels):
    i = bisect.bisect_right(labels, off)
    return labels[i] - off if i < len(labels) else 0

@asset("gTilesetGrassAnim", stride=2, bind=False)
def tileset_grass_anim(rom):
    base = rom.offset("Tilesets")
    out = bytearray()
    for i in range(NUM_TILESETS):
        e = rom.read(base + i * TILESET_HEADER_SIZE, TILESET_HEADER_SIZE)
        out += bytes((e[10], e[11]))
    return bytes(out)

@asset("gTilesetCounterTiles", stride=3, bind=False)
def tileset_counter_tiles(rom):
    base = rom.offset("Tilesets")
    out = bytearray()
    for i in range(NUM_TILESETS):
        e = rom.read(base + i * TILESET_HEADER_SIZE, TILESET_HEADER_SIZE)
        out += bytes(e[7:10])
    return bytes(out)

TILESET_SLOTS = ("blocks", "gfx", "coll")

TILESET_ELEM_COUNTS = {
    0:  (128, 96),
    1:  (19,  80),
    2:  (37,  96),
    3:  (128, 96),
    4:  (19,  80),
    5:  (116, 96),
    6:  (37,  96),
    7:  (116, 96),
    8:  (35,  96),
    9:  (128, 96),
    10: (128, 96),
    11: (17,  32),
    12: (128, 96),
    13: (62,  96),
    14: (23,  96),
    15: (110, 96),
    16: (58,  96),
    17: (128, 80),
    18: (79,  96),
    19: (72,  96),
    20: (58,  96),
    21: (36,  80),
    22: (128, 96),
    23: (73,  80),
}

def tileset_asset_name(idx, slot):
    return f"tileset_{idx:02d}_{TILESET_SLOTS[slot]}"

def _register_tileset_leaves():
    names = _tileset_c_names()
    for idx in range(NUM_TILESETS):
        cnames = names.get(idx)
        nb, gt = TILESET_ELEM_COUNTS[idx]
        counts = (nb, gt, 0)
        for slot in range(3):
            cname = cnames[slot] if cnames else None

            def make(idx=idx, slot=slot, want_elems=counts[slot]):
                def provider(rom):
                    off = _tileset_headers(rom)[idx][1 + slot]
                    if slot == 2:

                        end = rom.data.index(0xFF, off)
                        return rom.data[off:end + 1]

                    extent = _blob_extent(rom, off, _all_symbol_offsets(rom))
                    size = want_elems * 16 if want_elems else extent - extent % 16
                    blob = rom.read(off, min(size, extent))
                    return blob + bytes(max(0, size - len(blob)))
                return provider

            asset(tileset_asset_name(idx, slot),
                  stride=16 if slot != 2 else 1,
                  verify="src/data/tileset_data.c",
                  symbol=cname, bind=False)(make())

_register_tileset_leaves()

BASE_STATS_FIELDS = (
    ("dex_id", 1, 1), ("hp", 1, 1), ("atk", 1, 1), ("def", 1, 1),
    ("spd", 1, 1), ("spc", 1, 1), ("type1", 1, 1), ("type2", 1, 1),
    ("catch_rate", 1, 1), ("base_exp", 1, 1), ("sprite_dim", 1, 1),
    ("front_ptr", 2, 1), ("back_ptr", 2, 1),
    ("start_moves", 1, 4), ("growth_rate", 1, 1), ("tmhm", 1, 7),
)
BASE_STATS_BYTES = sum(w * n for _, w, n in BASE_STATS_FIELDS)
ROM_BASE_STATS_STRIDE = 28

@asset("gBaseStats", stride=BASE_STATS_BYTES,
       verify="src/data/base_stats.c",
       fields=BASE_STATS_FIELDS, count=152,
       bind=True, ctype="base_stats_t", ctype_header="game/types.h",
       expect_diff="sprite_dim / front_ptr / back_ptr are populated from the "
                   "ROM where the committed table leaves them ZERO -- "
                   "base_stats.c simply never writes those three fields, "
                   "because the port reads sprites from gPokemonFrontSprite "
                   "and friends instead. Every field the port actually uses "
                   "is byte-identical. Harmless to populate, and more faithful "
                   "than shipping zeroes.")
def base_stats(rom):
    base = rom.offset("BaseStats")
    out = bytearray(BASE_STATS_BYTES)
    for d in range(150):
        e = rom.read(base + d * ROM_BASE_STATS_STRIDE, ROM_BASE_STATS_STRIDE)
        out += e[:BASE_STATS_BYTES]

    out += rom.read("MewBaseStats", ROM_BASE_STATS_STRIDE)[:BASE_STATS_BYTES]

    tail = BASE_STATS_BYTES - 1
    for d in range(152):
        out[d * BASE_STATS_BYTES + tail] &= 0x7F
    return bytes(out)

@asset("gTMHMMoves", verify="src/data/tmhm_data.c", bind=True, elem="")
def tmhm_moves(rom):
    return rom.read("TechnicalMachines", 55)

@asset("gTitleLogoTiles", stride=16, verify="src/data/title_screen_data.c",
       bind=True)
def title_logo_tiles(rom):
    return rom.read("PokemonLogoGraphics", 112 * 16)

def _flat(name, sym, size, path, stride=16, extra=0, bind=False, elem=None):
    @asset(name, stride=stride, verify=path, bind=bind, elem=elem)
    def provider(rom, sym=sym, size=size, extra=extra):
        return rom.read(sym, size, extra)
    provider.__name__ = "flat_" + name
    return provider

SPLASH = "src/data/splash_screen_data.c"
TITLE = "src/data/title_screen_data.c"
TRADE = "src/data/trade_gfx.c"
SLOTS = "src/data/slots_gfx.c"
TCARD = "src/data/trainer_card_tiles.c"

_flat("gSplashGameFreakLogoTiles",     "GameFreakIntro",              96,  SPLASH, extra=208, bind=True)
_flat("gSplashGameFreakPresentsTiles", "GameFreakIntro",             208,  SPLASH, bind=True)
_flat("gSplashFallingStarTiles",       "FallingStar",                 16,  SPLASH, bind=True)
_flat("gSplashLegalTiles",             "BattleHudTiles3End",         448,  SPLASH, bind=True)
_flat("gTitlePlayerTiles",             "PlayerCharacterTitleGraphics", 560, TITLE, bind=True)
_flat("gCreditsTheEndTiles",           "TheEndGfx",                  160,  "src/data/credits_data.c", bind=True)
_flat("gMoveAnimTileset0",             "MoveAnimationTiles0",       1264,  "src/data/move_anim_tiles.c", bind=True)
_flat("gMoveAnimTileset1",             "MoveAnimationTiles1",       1264,  "src/data/move_anim_tiles.c", bind=True)
_flat("gTownMapWorldTiles",            "PokedexTileGraphicsEnd",     256,  "src/data/town_map_data.c", bind=True)
_flat("gTradeGameBoyTiles",            "PokeballTileGraphicsEnd",    544,  TRADE, bind=True)
_flat("gTradeLinkCableTiles",          "PokeballTileGraphicsEnd",    240,  TRADE, extra=544, bind=True)
_flat("gTradeCableBallTiles",          "TradingAnimationGraphics2",   64,  TRADE, bind=True)

SGBB = "src/data/sgb_border_data.c"
_flat("gSgbBorderTilemap",  "BorderPalettes",       1792, SGBB, stride=2,  elem="[2]", bind=True)
_flat("gSgbBorderPalettes", "BorderPalettes",         96, SGBB, stride=32, elem="[32]",
      extra=0x800, bind=True)
_flat("gSgbBorderTiles",    "SGBBorderGraphics",    1536, SGBB, bind=True)

_flat("kSlotsReel",                    "SlotMachineMapEnd",          108,  SLOTS, stride=36, elem="[36]", bind=True)
_flat("kSlotsTileMap",                 "SlotMachineMap",             240,  SLOTS, stride=20, elem="[20]", bind=True)

_flat("kBadgeCircleTile",              "CircleTile",                  16,  TCARD, stride=1, elem="", bind=True)

@asset("kBadgeNumberTiles", stride=16, verify="src/data/trainer_card_tiles.c",
       bind=True)
def badge_number_tiles(rom):
    return rom.read("BadgeNumbersTileGraphics", 8 * 16)

NUM_MAPS = 256
NUM_REAL_MAPS = 248

MAP_FILLER = {
    11:  (20, 18, 0, 10),
    105: (13, 13, 5, 113), 106: (13, 13, 5, 113), 107: (13, 13, 5, 113),
    109: (13, 13, 5, 113), 110: (13, 13, 5, 113), 111: (13, 13, 5, 113),
    112: (13, 13, 5, 113), 114: (13, 13, 5, 113), 115: (13, 13, 5, 113),
    116: (13, 13, 5, 113), 117: (13, 13, 5, 113),
    204: (3, 4, 18, 203),  205: (3, 4, 18, 203),  206: (3, 4, 18, 203),
    231: (4, 7, 12, 186),
    237: (15, 9, 22, 207), 238: (15, 9, 22, 207), 241: (15, 9, 22, 207),
    242: (15, 9, 22, 207), 243: (15, 9, 22, 207), 244: (15, 9, 22, 207),
}

MAP_NAMES = [
    "PalletTown", "ViridianCity", "PewterCity", "CeruleanCity", "LavenderTown", "VermilionCity", "CeladonCity", "FuchsiaCity",
    "CinnabarIsland", "IndigoPlateau", "SaffronCity", "SaffronCity", "Route1", "Route2", "Route3", "Route4",
    "Route5", "Route6", "Route7", "Route8", "Route9", "Route10", "Route11", "Route12",
    "Route13", "Route14", "Route15", "Route16", "Route17", "Route18", "Route19", "Route20",
    "Route21", "Route22", "Route23", "Route24", "Route25", "RedsHouse1F", "RedsHouse2F", "BluesHouse",
    "OaksLab", "ViridianPokecenter", "ViridianMart", "ViridianSchoolHouse", "ViridianNicknameHouse", "ViridianGym", "DiglettsCaveRoute2", "ViridianForestNorthGate",
    "Route2TradeHouse", "Route2Gate", "ViridianForestSouthGate", "ViridianForest", "Museum1F", "Museum2F", "PewterGym", "PewterNidoranHouse",
    "PewterMart", "PewterSpeechHouse", "PewterPokecenter", "MtMoon1F", "MtMoonB1F", "MtMoonB2F", "CeruleanTrashedHouse", "CeruleanTradeHouse",
    "CeruleanPokecenter", "CeruleanGym", "BikeShop", "CeruleanMart", "MtMoonPokecenter", "CeruleanTrashedHouse", "Route5Gate", "UndergroundPathRoute5",
    "Daycare", "Route6Gate", "UndergroundPathRoute6", "UndergroundPathRoute6", "Route7Gate", "UndergroundPathRoute7", "UndergroundPathRoute7Copy", "Route8Gate",
    "UndergroundPathRoute8", "RockTunnelPokecenter", "RockTunnel1F", "PowerPlant", "Route11Gate1F", "DiglettsCaveRoute11", "Route11Gate2F", "Route12Gate1F",
    "BillsHouse", "VermilionPokecenter", "PokemonFanClub", "VermilionMart", "VermilionGym", "VermilionPidgeyHouse", "VermilionDock", "SSAnne1F",
    "SSAnne2F", "SSAnne3F", "SSAnneB1F", "SSAnneBow", "SSAnneKitchen", "SSAnneCaptainsRoom", "SSAnne1FRooms", "SSAnne2FRooms",
    "SSAnneB1FRooms", "LancesRoom", "LancesRoom", "LancesRoom", "VictoryRoad1F", "LancesRoom", "LancesRoom", "LancesRoom",
    "LancesRoom", "LancesRoom", "LancesRoom", "LancesRoom", "LancesRoom", "LancesRoom", "HallOfFame", "UndergroundPathNorthSouth",
    "ChampionsRoom", "UndergroundPathWestEast", "CeladonMart1F", "CeladonMart2F", "CeladonMart3F", "CeladonMart4F", "CeladonMartRoof", "CeladonMartElevator",
    "CeladonMansion1F", "CeladonMansion2F", "CeladonMansion3F", "CeladonMansionRoof", "CeladonMansionRoofHouse", "CeladonPokecenter", "CeladonGym", "GameCorner",
    "CeladonMart5F", "GameCornerPrizeRoom", "CeladonDiner", "CeladonChiefHouse", "CeladonHotel", "LavenderPokecenter", "PokemonTower1F", "PokemonTower2F",
    "PokemonTower3F", "PokemonTower4F", "PokemonTower5F", "PokemonTower6F", "PokemonTower7F", "MrFujisHouse", "LavenderMart", "LavenderCuboneHouse",
    "FuchsiaMart", "FuchsiaBillsGrandpasHouse", "FuchsiaPokecenter", "WardensHouse", "SafariZoneGate", "FuchsiaGym", "FuchsiaMeetingRoom", "SeafoamIslandsB1F",
    "SeafoamIslandsB2F", "SeafoamIslandsB3F", "SeafoamIslandsB4F", "VermilionOldRodHouse", "FuchsiaGoodRodHouse", "PokemonMansion1F", "CinnabarGym", "CinnabarLab",
    "CinnabarLabTradeRoom", "CinnabarLabMetronomeRoom", "CinnabarLabFossilRoom", "CinnabarPokecenter", "CinnabarMart", "CinnabarMart", "IndigoPlateauLobby", "CopycatsHouse1F",
    "CopycatsHouse2F", "FightingDojo", "SaffronGym", "SaffronPidgeyHouse", "SaffronMart", "SilphCo1F", "SaffronPokecenter", "MrPsychicsHouse",
    "Route15Gate1F", "Route15Gate2F", "Route16Gate1F", "Route16Gate2F", "Route16FlyHouse", "Route12SuperRodHouse", "Route18Gate1F", "Route18Gate2F",
    "SeafoamIslands1F", "Route22Gate", "VictoryRoad2F", "Route12Gate2F", "VermilionTradeHouse", "DiglettsCave", "VictoryRoad3F", "RocketHideoutB1F",
    "RocketHideoutB2F", "RocketHideoutB3F", "RocketHideoutB4F", "RocketHideoutElevator", "RocketHideoutElevator", "RocketHideoutElevator", "RocketHideoutElevator", "SilphCo2F",
    "SilphCo3F", "SilphCo4F", "SilphCo5F", "SilphCo6F", "SilphCo7F", "SilphCo8F", "PokemonMansion2F", "PokemonMansion3F",
    "PokemonMansionB1F", "SafariZoneEast", "SafariZoneNorth", "SafariZoneWest", "SafariZoneCenter", "SafariZoneCenterRestHouse", "SafariZoneSecretHouse", "SafariZoneWestRestHouse",
    "SafariZoneEastRestHouse", "SafariZoneNorthRestHouse", "CeruleanCave2F", "CeruleanCaveB1F", "CeruleanCave1F", "NameRatersHouse", "CeruleanBadgeHouse", "Route16Gate1F",
    "RockTunnelB1F", "SilphCo9F", "SilphCo10F", "SilphCo11F", "SilphCoElevator", "SilphCo2F", "SilphCo2F", "TradeCenter",
    "Colosseum", "SilphCo2F", "SilphCo2F", "SilphCo2F", "SilphCo2F", "LoreleisRoom", "BrunosRoom", "AgathasRoom",
    "", "", "", "", "", "", "", "",
]

def _map_headers(rom):
    ptrs = rom.offset("MapHeaderPointers")
    banks = rom.offset("MapHeaderBanks")
    out = {}
    for i in range(NUM_REAL_MAPS):
        bank = rom.data[banks + i]
        addr = rom.data[ptrs + i * 2] | (rom.data[ptrs + i * 2 + 1] << 8)
        off = addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)
        t, h, w = rom.data[off], rom.data[off + 1], rom.data[off + 2]
        ba = rom.data[off + 3] | (rom.data[off + 4] << 8)
        boff = ba if ba < 0x4000 else bank * 0x4000 + (ba - 0x4000)
        out[i] = (t, h, w, boff)
    return out

def map_blocks_source(i):
    return MAP_FILLER[i][3] if i in MAP_FILLER else i

def map_asset_name(i):
    return f"map_{i:03d}_blocks"

MAP_BORDER_OVERRIDE = {94: 0x0F}

def map_scalars(rom, i):
    border = MAP_BORDER_OVERRIDE.get(i, 0)
    if i in MAP_FILLER:
        w, h, t, _src = MAP_FILLER[i]
        return (w, h, t, border)
    if i >= NUM_REAL_MAPS:

        return (32, 20, 0, border)
    t, h, w, _b = _map_headers(rom)[i]
    return (w, h, t, border)

@asset("gMapMeta", stride=4, verify="src/data/map_data.c")
def map_meta(rom):
    out = bytearray()
    for i in range(NUM_MAPS):
        out += bytes(map_scalars(rom, i))
    return bytes(out)

def _register_map_blocks():
    for i in range(NUM_REAL_MAPS):
        if map_blocks_source(i) != i:
            continue

        def make(i=i):
            def provider(rom):
                t, h, w, boff = _map_headers(rom)[i]

                return rom.data[boff:boff + w * h]
            return provider

        asset(map_asset_name(i), bind=False,
              verify="src/data/map_data.c")(make())

_register_map_blocks()

AUDIO_BANKS = (0x02, 0x08, 0x1F)
AUDIO_BANK_SIZE = 0x4000

def _register_audio_banks():
    for i, bank in enumerate(AUDIO_BANKS, start=1):
        def make(bank=bank, i=i):
            def provider(rom):
                return rom.read(bank * AUDIO_BANK_SIZE, AUDIO_BANK_SIZE)
            provider.__doc__ = (
                f"AUDIO_{i}: ROM bank ${bank:02X} verbatim. Addresses $4000-$7FFF "
                f"index it directly as [addr - 0x4000].")
            provider.__name__ = f"audio_bank_{i}"
            return provider

        asset(f"gAudioBank{i}", stride=1, verify=None,
              bind=True, elem="")(make())

_register_audio_banks()

AUDIO_BANK_META_SYMS = ("Pitches", "WavePointers", "CryRet")

@asset("gAudioBankMeta", stride=2, verify=None, bind=True, elem="")
def audio_bank_meta(rom):
    out = bytearray()
    for i in range(1, 4):
        for field in AUDIO_BANK_META_SYMS:
            name = f"Audio{i}_{field}"
            if name not in rom.sym:
                raise SystemExit(f"audio meta: {name} not in the .sym")
            _, addr = rom.sym[name]
            out += struct.pack("<H", addr)
    return bytes(out)

MUSIC_SONGS = (
    ("MUSIC_NONE", ()),
    ("MUSIC_PALLET_TOWN", ("PalletTown", (1, 2, 3))),
    ("MUSIC_POKECENTER", ("Pokecenter", (1, 2, 3))),
    ("MUSIC_GYM", ("Gym", (1, 2, 3))),
    ("MUSIC_CITIES1", ("Cities1", (1, 2, 3))),
    ("MUSIC_CITIES2", ("Cities2", (1, 2, 3))),
    ("MUSIC_CELADON", ("Celadon", (1, 2, 3))),
    ("MUSIC_CINNABAR", ("Cinnabar", (1, 2, 3))),
    ("MUSIC_VERMILION", ("Vermilion", (1, 2, 3))),
    ("MUSIC_LAVENDER", ("Lavender", (1, 2, 3))),
    ("MUSIC_SS_ANNE", ("SSAnne", (1, 2, 3))),
    ("MUSIC_ROUTES1", ("Routes1", (1, 2, 3))),
    ("MUSIC_ROUTES2", ("Routes2", (1, 2, 3))),
    ("MUSIC_ROUTES3", ("Routes3", (1, 2, 3))),
    ("MUSIC_ROUTES4", ("Routes4", (1, 2, 3, 4))),
    ("MUSIC_INDIGO_PLATEAU", ("IndigoPlateau", (1, 2, 3, 4))),
    ("MUSIC_OAKS_LAB", ("OaksLab", (1, 2, 3))),
    ("MUSIC_DUNGEON1", ("Dungeon1", (1, 2, 3))),
    ("MUSIC_DUNGEON2", ("Dungeon2", (1, 2, 3))),
    ("MUSIC_DUNGEON3", ("Dungeon3", (1, 2, 3))),
    ("MUSIC_POKEMON_TOWER", ("PokemonTower", (1, 2, 3))),
    ("MUSIC_SILPH_CO", ("SilphCo", (1, 2, 3))),
    ("MUSIC_SAFARI_ZONE", ("SafariZone", (1, 2, 3))),
    ("MUSIC_TITLE", ("TitleScreen", (1, 2, 3, 4))),
    ("MUSIC_JIGGLYPUFF", ("JigglypuffSong", (1, 2))),
    ("MUSIC_WILD_BATTLE", ("WildBattle", (1, 2, 3))),
    ("MUSIC_DEFEATED_WILD_MON", ("DefeatedWildMon", (1, 2, 3))),
    ("MUSIC_DEFEATED_TRAINER", ("DefeatedTrainer", (1, 2, 3))),
    ("MUSIC_DEFEATED_GYM_LEADER", ("DefeatedGymLeader", (1, 2, 3))),
    ("MUSIC_PKMN_HEALED", ("PkmnHealed", (1, 2, 3))),
    ("MUSIC_GYM_LEADER_BATTLE", ("GymLeaderBattle", (1, 2, 3))),
    ("MUSIC_TRAINER_BATTLE", ("TrainerBattle", (1, 2, 3))),
    ("MUSIC_MEET_RIVAL", ("MeetRival", (1, 2, 3))),
    ("MUSIC_MEET_MALE_TRAINER", ("MeetMaleTrainer", (1, 2, 3))),
    ("MUSIC_MEET_FEMALE_TRAINER", ("MeetFemaleTrainer", (1, 2, 3))),
    ("MUSIC_MUSEUM_GUY", ("MuseumGuy", (1, 2, 3))),
    ("MUSIC_MEET_EVIL_TRAINER", ("MeetEvilTrainer", (1, 2, 3))),
    ("MUSIC_SURFING", ("Surfing", (1, 2, 3))),
    ("MUSIC_MEET_PROF_OAK", ("MeetProfOak", (1, 2, 3))),
    ("MUSIC_INTRO_BATTLE", ("IntroBattle", (1, 2, 3, 4))),
    ("MUSIC_GAME_CORNER", ("GameCorner", (1, 2, 3))),
    ("MUSIC_BIKE_RIDING", ("BikeRiding", (1, 2, 3))),
    ("MUSIC_CINNABAR_MANSION", ("CinnabarMansion", (1, 2, 3, 4))),
    ("MUSIC_FINAL_BATTLE", ("FinalBattle", (1, 2, 3))),
    ("MUSIC_HALL_OF_FAME", ("HallOfFame", (1, 2, 3))),
    ("MUSIC_CREDITS", ("Credits", (1, 2, 3))),
    ("MUSIC_POKEFLUTE", ("Pokeflute", (3,))),
)

MUSIC_VARIANTS = (
    ("Cities1AltTempo", (1, 2, 3)),
    ("MeetRivalAltStart", (1, 2, 3)),
    ("MeetRivalAltTempo", (1, 2, 3)),
    ("MeetRivalAltStartTempo", (1, 2, 3)),
)

NUM_DRUM_INSTRUMENTS = 19
NOTE_EVT_BYTES = 13
DRUM_STEP_BYTES = 3

def music_channels():
    seen, out = set(), []
    for _, entry in MUSIC_SONGS:
        if not entry:
            continue
        stem, chans = entry
        for c in chans:
            if (stem, c) not in seen:
                seen.add((stem, c))
                out.append((stem, c))
    for stem, chans in MUSIC_VARIANTS:
        for c in chans:
            if (stem, c) not in seen:
                seen.add((stem, c))
                out.append((stem, c))
    return out

MUSIC_CHANNELS = music_channels()

def channel_asset_name(stem, ch):
    return f"k{stem}_Ch{ch}_notes"

def _decode_music_channel(rom, stem, ch):
    import gen1_audio as GA
    chans = GA.song_channels(rom, stem)
    tempo = GA.song_tempo(rom, [c[1] for c in chans])
    for cid, flat in chans:
        if cid + 1 != ch:
            continue
        ev = GA.decode_channel(rom, flat, is_drum=(cid == 3),
                               is_wave=(cid == 2), tempo=tempo)
        return ev, ev.loop_start
    raise SystemExit(f"music: {stem} has no channel {ch}")

def _pack_events(events):
    out = bytearray()
    for e in events:
        vib = e.get("vib", (0, 0, 0))
        slide = e.get("slide", (0, 0))
        vol = e.get("volume", 0)
        out += struct.pack("<HHBBBBBBHB",
                           e.get("freq", 0) & 0xFFFF,
                           e["frames"] & 0xFFFF,
                           e.get("duty", 0) & 0xFF,
                           vol & 0xFF,
                           ((vol << 4) | e.get("env", 0)) & 0xFF,
                           vib[0] & 0xFF, vib[1] & 0xFF, vib[2] & 0xFF,
                           slide[0] & 0xFFFF, slide[1] & 0xFF)
    return bytes(out)

def _register_music():
    for stem, ch in MUSIC_CHANNELS:
        def make(stem=stem, ch=ch):
            def provider(rom):
                ev, _ = _decode_music_channel(rom, stem, ch)
                return _pack_events(ev)
            provider.__name__ = f"music_{stem}_Ch{ch}"
            return provider

        asset(channel_asset_name(stem, ch), stride=NOTE_EVT_BYTES,
              verify=None, cut=True)(make())

    @asset("gMusicChannelLoops", stride=2, verify=None, cut=True)
    def music_channel_loops(rom):
        out = bytearray()
        for stem, ch in MUSIC_CHANNELS:
            _, ls = _decode_music_channel(rom, stem, ch)
            out += struct.pack("<h", ls)
        return bytes(out)

    for i in range(1, NUM_DRUM_INSTRUMENTS + 1):
        def make(i=i):
            def provider(rom):

                off = rom.offset(f"SFX_Noise_Instrument{i:02d}_1_Ch8")
                out = bytearray()
                while rom.data[off] != 0xFF:
                    length = (rom.data[off] & 0x0F) + 1
                    out += bytes((rom.data[off + 2], rom.data[off + 1], length))
                    off += 3
                return bytes(out)
            provider.__name__ = f"drum_inst_{i}"
            return provider
        asset(f"kDrumInst{i}_steps", stride=DRUM_STEP_BYTES,
              verify=None, cut=True)(make())

    @asset("kMapMusicID", verify=None, cut=True)
    def map_music_id(rom):
        import gen1_audio as GA
        base_local = rom.offset("SFX_Headers_1") % 0x4000
        rom_id = {}
        for idx, (_, entry) in enumerate(MUSIC_SONGS):
            if not entry:
                continue
            sym = GA.song_symbol(rom, entry[0])
            if sym not in rom.sym:
                continue
            local = rom.offset(sym) % 0x4000
            rom_id[((local - base_local) // 3, rom.sym[sym][0])] = idx
        off = rom.offset("MapSongBanks")
        return bytes(rom_id.get((rom.data[off + 2 * i],
                                 rom.data[off + 2 * i + 1]), 0)
                     for i in range(NUM_REAL_MAPS))

_register_music()

MOVE_BYTES = 6
NUM_MOVE_DEFS = 166

@asset("gMoves", stride=MOVE_BYTES, verify="src/data/moves_data.c",
       shape=(NUM_MOVE_DEFS, MOVE_BYTES),
       bind=True, ctype="move_t", ctype_header="game/types.h")
def moves(rom):
    return bytes(MOVE_BYTES) + rom.read("Moves", (NUM_MOVE_DEFS - 1) * MOVE_BYTES)

WILD_SLOTS = 10
WILD_MONS_BYTES = 1 + WILD_SLOTS * 2

def _wild_table(rom, want_water):
    base = rom.offset("WildDataPointers")
    bank = rom.sym["WildDataPointers"][0]
    out = bytearray()
    for i in range(NUM_MAPS):

        if i >= NUM_REAL_MAPS:
            out += bytes(WILD_MONS_BYTES)
            continue
        addr = rom.data[base + 2 * i] | (rom.data[base + 2 * i + 1] << 8)
        p = bank * 0x4000 + (addr - 0x4000)
        blocks = []
        for _ in range(2):
            rate = rom.data[p]
            p += 1
            slots = b""
            if rate:
                slots = bytes(rom.data[p:p + WILD_SLOTS * 2])
                p += WILD_SLOTS * 2
            blocks.append((rate, slots))
        rate, slots = blocks[1 if want_water else 0]
        out += bytes([rate]) + slots.ljust(WILD_SLOTS * 2, b"\0")
    return bytes(out)

@asset("gFishingPoseTiles", stride=16, shape=(6, 16), bind=True, elem="[16]")
def fishing_pose_tiles(rom):
    return (rom.read("RedFishingTilesFront", 2 * 16)
            + rom.read("RedFishingTilesBack", 2 * 16)
            + rom.read("RedFishingTilesSide", 2 * 16))

@asset("gFishingRodTiles", stride=16, shape=(3, 16), bind=True, elem="[16]")
def fishing_rod_tiles(rom):
    return rom.read("RedFishingRodTiles", 3 * 16)

@asset("gGoodRodMons", stride=2, shape=(2, 2), bind=True, elem="[2]")
def good_rod_mons(rom):
    return rom.read("GoodRodMons", 2 * 2)

SUPER_ROD_SLOTS = 4
SUPER_ROD_BYTES = 1 + SUPER_ROD_SLOTS * 2

@asset("gSuperRodData", stride=SUPER_ROD_BYTES,
       shape=(NUM_MAPS, SUPER_ROD_BYTES), bind=True,
       ctype="super_rod_group_t", ctype_header="data/fishing_types.h")
def super_rod_data(rom):
    base = rom.offset("SuperRodData")
    groups = {}
    p = base
    while rom.data[p] != 0xFF:
        map_id = rom.data[p]
        addr = rom.data[p + 1] | (rom.data[p + 2] << 8)
        p += 3
        bank = rom.sym["SuperRodData"][0]
        gp = bank * 0x4000 + (addr - 0x4000)
        n = rom.data[gp]
        if n > SUPER_ROD_SLOTS:
            raise AssertionError(
                "SuperRodData group for map %d has %d mons, more than the %d "
                "the 2-bit roll can select" % (map_id, n, SUPER_ROD_SLOTS))
        groups[map_id] = bytes([n]) + bytes(rom.data[gp + 1:gp + 1 + n * 2])

    out = bytearray()
    for i in range(NUM_MAPS):
        rec = groups.get(i, b"\0")
        out += rec.ljust(SUPER_ROD_BYTES, b"\0")
    return bytes(out)

WILD_VERSION_DIFF = (
    "13 maps carry VERSION-EXCLUSIVE encounters and the committed table has "
    "them wrong. data/wild/maps/*.asm guards those slots with IF DEF(_RED) / "
    "IF DEF(_BLUE), and the old extractor read the disassembly's TEXT and "
    "concatenated BOTH branches, then truncated to 10 slots -- so a map got "
    "Red's slots followed by the first few of Blue's, and the genuinely shared "
    "tail after the ENDC fell off the end. Route 11 is the clearest: the ROM "
    "gives 17 SPEAROW / 11 DROWZEE / 15 DROWZEE for slots 8-10, while the "
    "committed table has 14 SANDSHREW / 15 SPEAROW / 12 SANDSHREW -- Blue's "
    "first three slots, and SANDSHREW is BLUE-EXCLUSIVE, so a Red port was "
    "spawning a species that cannot appear in it. Affected: Route11, Route14, "
    "Route15, Route23, Route24, ViridianForest, SeafoamIslandsB3F, "
    "SeafoamIslandsB4F, SafariZoneNorth, SafariZoneWest, SafariZoneCenter, "
    "CeruleanCave1F, CeruleanCaveB1F. The ROM is the version the user owns and "
    "wins outright."
)

@asset("gWildGrass", stride=WILD_MONS_BYTES, verify="src/data/wild_data.c",
       shape=(NUM_MAPS, WILD_MONS_BYTES), expect_diff=WILD_VERSION_DIFF,
       bind=True, ctype="wild_mons_t", ctype_header="data/wild_types.h")
def wild_grass(rom):
    return _wild_table(rom, want_water=False)

@asset("gWildWater", stride=WILD_MONS_BYTES, verify="src/data/wild_data.c",
       shape=(NUM_MAPS, WILD_MONS_BYTES),
       bind=True, ctype="wild_mons_t", ctype_header="data/wild_types.h")
def wild_water(rom):
    return _wild_table(rom, want_water=True)

def _flat1bpp(name, sym, tiles, path, extra=0, bind=False, end_sym=None):
    @asset(name, stride=16, verify=path, bind=bind, elem="[16]")
    def provider(rom, sym=sym, tiles=tiles, extra=extra, end_sym=end_sym):

        if end_sym:
            tiles = (rom.offset(end_sym) - rom.offset(sym)) // 8
        raw = rom.read(sym, tiles * 8, extra)
        out = bytearray()
        for b in raw:
            out += bytes((b, b))
        return bytes(out)
    provider.__name__ = "flat1bpp_" + name
    return provider

SLOTS_C = "src/data/slots_gfx.c"
TCARD_C = "src/data/trainer_card_tiles.c"
TOWNMAP_C = "src/data/town_map_data.c"
INTRO_C = "src/data/intro_scene_data.c"

_flat("kSlotsFrameTiles",     "SlotMachineTiles1",              37 * 16, SLOTS_C, bind=True)
_flat("kSlotsSymbolTiles",    "SlotMachineTiles2",              24 * 16, SLOTS_C, bind=True)
_flat("kTrainerInfoBoxTiles", "TrainerInfoTextBoxTileGraphics",  9 * 16, TCARD_C, bind=True)

_flat("kTimeColonTile",  "TextBoxGraphics",          16, TCARD_C, extra=208,
      stride=1, elem="", bind=True)
_flat("gTradeGlyphTiles", "HpBarAndStatusGraphics",  32, "src/data/trade_gfx.c",
      extra=272, bind=True)

_flat1bpp("gTownMapCursorTiles",   "TownMapCursor",  4, TOWNMAP_C, bind=True)
_flat1bpp("gTitleRedVersionTiles", "Version_GFX",   10,
          "src/data/title_screen_data.c", bind=True, end_sym="Version_GFXEnd")

_flat("gDefaultNamesPlayer", "DefaultNamesPlayerList", 40,
      "src/game/intro.c", stride=1, elem="", bind=True)
_flat("gDefaultNamesRival",  "DefaultNamesRivalList",  40,
      "src/game/intro.c", stride=1, elem="", bind=True)

_flat("gPrizeMon1Entries", "PrizeMenuMon1Entries", 4,
      "src/game/amberscript_scene.c", stride=1, elem="", bind=True)
_flat("gPrizeMon1Cost",    "PrizeMenuMon1Cost",    7,
      "src/game/amberscript_scene.c", stride=1, elem="", bind=True)
_flat("gPrizeMon2Entries", "PrizeMenuMon2Entries", 4,
      "src/game/amberscript_scene.c", stride=1, elem="", bind=True)
_flat("gPrizeMon2Cost",    "PrizeMenuMon2Cost",    7,
      "src/game/amberscript_scene.c", stride=1, elem="", bind=True)
_flat("gPrizeMonLevels",   "PrizeMonLevelDictionary", 12,
      "src/game/amberscript_scene.c", stride=1, elem="", bind=True)

_flat("gTitleMons", "TitleMons", 16,
      "src/data/title_screen_data.c", stride=1, elem="", bind=True)

_flat("gTitleVersionTileMap", "VersionOnTitleScreenText", 8,
      "src/data/title_screen_data.c", stride=1, elem="", bind=True)

_flat("gTownMapCompressedMap", "CompressedMap", 171, TOWNMAP_C, stride=1,
      elem="", bind=True)
_flat("gTownMapOrderMapIds",   "TownMapOrder",   47, TOWNMAP_C, stride=1,
      elem="", bind=True)

for _i in (1, 2, 3):
    _flat(f"gIntroGengarTilemap{_i}", f"GengarIntroTiles{_i}", 49, INTRO_C,
          stride=1, elem="", bind=True)

GEN1_HUD_FIRST_CHAR = 0x62
GEN1_HUD_TILES = 23
GEN1_BOX_FIRST_CHAR = 0x79
GEN1_BOX_TILES = 7

def _gen1_span(rom, start, end):
    a, b = rom.offset(start), rom.offset(end)
    if b <= a:
        raise RomError(f"{start}..{end} is empty or inverted -- wrong ROM?")
    return rom.data[a:b]

def _gen1_double(src):
    out = bytearray()
    for b in src:
        out += bytes((b, b))
    return bytes(out)

@asset("gFontTiles", stride=16, cut=True, bind=True, elem="[16]")
def gen1_font_tiles(rom):
    return _gen1_double(_gen1_span(rom, "FontGraphics", "FontGraphicsEnd"))

@asset("gBoxTiles", stride=16, cut=True, bind=True, elem="[16]")
def gen1_box_tiles(rom):
    box = _gen1_span(rom, "TextBoxGraphics", "TextBoxGraphicsEnd")
    first = (GEN1_BOX_FIRST_CHAR - 0x60) * 16
    return bytes(box[first:first + GEN1_BOX_TILES * 16])

@asset("gHudTiles", stride=16, cut=True, bind=True, elem="[16]")
def gen1_hud_tiles(rom):
    span = {}

    def blit(data, first_char):
        for i in range(0, len(data), 16):
            span[first_char + i // 16] = data[i:i + 16]

    blit(_gen1_span(rom, "HpBarAndStatusGraphics", "HpBarAndStatusGraphicsEnd"),
         GEN1_HUD_FIRST_CHAR)
    blit(_gen1_double(_gen1_span(rom, "BattleHudTiles1", "BattleHudTiles1End")),
         0x6D)
    blit(_gen1_double(_gen1_span(rom, "BattleHudTiles2", "BattleHudTiles3End")),
         0x73)

    blank = b"\x00" * 16
    return b"".join(span.get(GEN1_HUD_FIRST_CHAR + i, blank)
                    for i in range(GEN1_HUD_TILES))

NUM_ICON_TILES = 256
ICON_HEADERS = 29

@asset("gMonPartyIconType", verify="src/data/party_icon_data.c",
       shape=(152,), bind=True, elem="")
def mon_party_icon_type(rom):
    out = bytearray([0])
    for b in rom.read("MonPartyData", 76):
        out += bytes((b >> 4, b & 0x0F))
    return bytes(out[:152])

ICON_GFX_DIFF = (
    "Two deliberate differences, both because this is now the ROM's real VRAM "
    "image rather than a PNG-derived approximation.\n"
    "  (1) THE ODD TILES ARE NO LONGER BLANK. A symmetric icon is drawn as two "
    "tiles mirrored across the centre (pm_write_slot_oam's else branch uses "
    "tile_base+0 and +2 with OAM_FLAG_FLIP_X, never +1 or +3), so the old "
    "extractor stored only the left halves. The ROM loads all four, and "
    "MonPartySpritePointers says so. Filling them is inert for every symmetric "
    "icon and correct for the one that is not.\n"
    "  (2) ICON_HELIX (tiles 8-11) NOW MATCHES THE ROM. There is no helix "
    "header in the table at all -- PokeBallSprite loads EIGHT tiles at "
    "ICON_BALL << 2 = 4, so it covers 4..11, and ICON_HELIX << 2 = 8 is simply "
    "its second half. The committed data had something else there, and the "
    "helix is the ASYMMETRIC case (pm_write_slot_oam uses tile_base+0,1,2,3 "
    "for it), so those tiles are the ones actually drawn for Omanyte, Kabuto "
    "and the other fossil-shaped mon."
)

@asset("gIconTileGfx", stride=16, verify="src/data/party_icon_data.c",
       shape=(NUM_ICON_TILES, 16), expect_diff=ICON_GFX_DIFF,
       bind=True, elem="[16]")
def icon_tile_gfx(rom):
    base = rom.offset("MonPartySpritePointers")
    out = bytearray(NUM_ICON_TILES * 16)
    for i in range(ICON_HEADERS):
        p = base + 6 * i
        src = rom.data[p] | (rom.data[p + 1] << 8)
        ntiles = rom.data[p + 2]
        bank = rom.data[p + 3]
        dest = rom.data[p + 4] | (rom.data[p + 5] << 8)
        flat = bank * 0x4000 + (src - 0x4000)
        slot = (dest - 0x8000) // 16
        if slot < 0 or slot + ntiles > NUM_ICON_TILES:
            continue
        out[slot * 16:(slot + ntiles) * 16] = rom.data[flat:flat + ntiles * 16]
    return bytes(out)

EVOS_MOVES_TABLE_SIZE = 191
NO_ENTRY = 0xFFFF

TABLES_CHECKED_ELSEWHERE = {
    "gEvosMovesBlob", "gEvosMovesOffsets",
    "gTrainerPartyBlob", "gTrainerPartyOffsets",
    "gTrainerBaseMoney",
    "g_cry_sq_notes", "g_cry_noise_notes", "g_cry_defs_meta",
    "gSfxDefs", "gSfxChannels", "gSfxCmds", "gMoveSfxData",
    "gMapEventBlob", "gMapEventOffsets", "gMapEventText",
    "gMoveNamesBlob", "gMoveNamesOffsets",
    "gTrainerClassNamesBlob", "gTrainerClassNamesOffsets",
    "gMoveAnimFrameBlockBlob", "gMoveAnimFrameBlockOffsets",
    "gMoveAnimSubanimBlob", "gMoveAnimSubanimOffsets",
    "gMoveAnimScriptBlob", "gMoveAnimScriptOffsets",
}

def _evos_moves_entries(rom):
    base = rom.offset("EvosMovesPointerTable")
    bank = rom.sym["EvosMovesPointerTable"][0]
    out = []
    for i in range(EVOS_MOVES_TABLE_SIZE - 1):
        addr = rom.data[base + 2 * i] | (rom.data[base + 2 * i + 1] << 8)
        p = start = bank * 0x4000 + (addr - 0x4000)
        while rom.data[p]:
            p += 1
        p += 1
        while rom.data[p]:
            p += 2
        p += 1
        out.append(bytes(rom.data[start:p]))
    return out

def _blob_and_offsets(entries, table_size, first_index):
    blob = bytearray()
    seen = {}
    offs = [NO_ENTRY] * table_size
    for i, rec in enumerate(entries):
        if rec not in seen:
            seen[rec] = len(blob)
            blob += rec
        offs[first_index + i] = seen[rec]
    return bytes(blob), offs

@asset("gEvosMovesBlob", verify=None, cut=True)
def evos_moves_blob(rom):
    return _blob_and_offsets(_evos_moves_entries(rom),
                             EVOS_MOVES_TABLE_SIZE, 1)[0]

@asset("gEvosMovesOffsets", stride=2, verify=None, cut=True)
def evos_moves_offsets(rom):
    offs = _blob_and_offsets(_evos_moves_entries(rom),
                             EVOS_MOVES_TABLE_SIZE, 1)[1]
    return b"".join(struct.pack("<H", o) for o in offs)

GBC_NUM_RED_SGB_PALS = 37
GBC_NUM_MON_PALETTES = 152

@asset("gGbcRedSgbPalettes", stride=8, elem="[4]", elem_type="uint16_t",
       verify=None, bind=True)
def gbc_red_sgb_palettes(rom):
    return rom.read("SuperPalettes", GBC_NUM_RED_SGB_PALS * 8)

@asset("gGbcYellowMonPaletteId", stride=1, verify=None, bind=True)
def gbc_mon_palette_id(rom):
    return rom.read("MonsterPalettes", GBC_NUM_MON_PALETTES)

NUM_TRAINERS = 47

def _trainer_party_entries(rom):
    base = rom.offset("TrainerDataPointers")
    bank = rom.sym["TrainerDataPointers"][0]
    starts = []
    for i in range(NUM_TRAINERS):
        addr = rom.data[base + 2 * i] | (rom.data[base + 2 * i + 1] << 8)
        starts.append(bank * 0x4000 + (addr - 0x4000))
    after = min((rom.offset(k) for k in rom.sym
                 if rom.sym[k][0] == bank and rom.offset(k) > starts[-1]),
                default=len(rom.data))
    bounds = starts[1:] + [after]
    out = []
    for st, end in zip(starts, bounds):
        rec = bytes(rom.data[st:end])

        out.append(rec if rec else b"\x00")
    return out

@asset("gTrainerPartyBlob", verify=None, cut=True)
def trainer_party_blob(rom):
    return _blob_and_offsets(_trainer_party_entries(rom), NUM_TRAINERS, 0)[0]

@asset("gTrainerPartyOffsets", stride=2, verify=None, cut=True)
def trainer_party_offsets(rom):
    offs = _blob_and_offsets(_trainer_party_entries(rom), NUM_TRAINERS, 0)[1]
    return b"".join(struct.pack("<H", o) for o in offs)

GYM_SHEET_GROUPS = 8
GYM_SHEET_TILES = 4

def _gym_sheet(rom, badge):
    base = rom.offset("GymLeaderFaceAndBadgeTileGraphics")
    out = bytearray()
    for g in range(GYM_SHEET_GROUPS):
        start = base + g * (GYM_SHEET_TILES * 2 * 16) + (64 if badge else 0)
        out += rom.data[start:start + GYM_SHEET_TILES * 16]
    return bytes(out)

@asset("kTrainerFaceTiles", stride=64, cut=True, bind=True, elem="[4][16]",
       verify=TCARD_C)
def trainer_face_tiles(rom):
    return _gym_sheet(rom, badge=False)

@asset("kTrainerBadgeTiles", stride=64, cut=True, bind=True, elem="[4][16]",
       verify=TCARD_C)
def trainer_badge_tiles(rom):
    return _gym_sheet(rom, badge=True)

@asset("gPokedexTiles", stride=16, cut=True, bind=True, elem="[16]",
       verify="src/data/pokedex_tiles.c")
def pokedex_tiles(rom):
    base = rom.offset("TextBoxGraphicsEnd")
    return bytes(rom.data[base:base + 288])

@asset("gTradeLinkCableMap", stride=12, cut=True, bind=True, elem="[12]",
       verify="src/data/trade_gfx.c")
def trade_linkcable_map(rom):
    base = rom.offset("LinkCableTiles")
    return bytes(rom.data[base:base + 36])

@asset("gPokedexOwnedBallTile", stride=1, cut=True, bind=True,
       verify="src/data/pokedex_tiles.c")
def pokedex_owned_ball(rom):
    base = rom.offset("PokeballTileGraphics")
    return bytes(rom.data[base:base + 16])

@asset("gIntroNidorinoFrontTiles", stride=16, cut=True, bind=True, elem="[16]",
       verify="src/data/intro_scene_data.c")
def intro_nidorino(rom):
    base = rom.offset("FightIntroFrontMon")
    return bytes(rom.data[base:base + 108 * 16])

@asset("gTownMapExternalCoords", stride=1, cut=True, bind=True,
       verify="src/data/town_map_data.c")
def town_map_external(rom):
    base = rom.offset("ExternalMapEntries")
    return bytes(rom.data[base + i * 3] for i in range(0x25))

TITLE_COPYRIGHT_SLOTS = (0, 1, 2, 3, 4, 19, 20, 21, 22, 23, 24, 25, 17, 18)

@asset("gTitleCopyrightTiles", stride=16, cut=True, bind=True, elem="[16]",
       verify="src/data/title_screen_data.c")
def title_copyright_tiles(rom):
    sheet = rom.read("NintendoCopyrightLogoGraphics", 26 * 16)
    return b"".join(sheet[s * 16: s * 16 + 16] for s in TITLE_COPYRIGHT_SLOTS)

@asset("gIntroGengarBackTiles", stride=16, cut=True, bind=True, elem="[16]",
       verify="src/data/intro_scene_data.c")
def intro_gengar_back(rom):
    base = rom.offset("GameFreakIntroEnd")
    return bytes(rom.data[base + 16:base + 16 + 94 * 16])

@asset("gSplashShootingStarTiles", stride=16, cut=True, bind=True, elem="[16]",
       verify="src/data/splash_screen_data.c")
def splash_shooting_star(rom):
    base = rom.offset("MoveAnimationTiles1")
    return bytes(rom.data[base + 48:base + 64]) + \
           bytes(rom.data[base + 304:base + 320])

@asset("gTownMapInternalCoords", stride=2, cut=True, bind=True,
       ctype="town_map_internal_entry_t",
       ctype_header="data/town_map_data.h",
       verify="src/data/town_map_data.c")
def town_map_internal(rom):
    base = rom.offset("InternalMapEntries")
    out = bytearray()
    for i in range(60):
        out += bytes(rom.data[base + i * 4: base + i * 4 + 2])
    return bytes(out)

@asset("gTradeGameBoyMap", stride=6, cut=True, bind=True, elem="[6]",
       verify="src/data/trade_gfx.c")
def trade_gameboy_map(rom):
    base = rom.offset("GameBoyTiles")
    return bytes(rom.data[base:base + 48])

NUM_MAP_CONNECTIONS = 256
CONN_DIRS = (("north", 0x8), ("south", 0x4), ("west", 0x2), ("east", 0x1))

@asset("gMapConnections", stride=24, cut=True, bind=True,
       ctype="map_connections_t", ctype_header="data/connection_data.h",
       verify=None)
def map_connections(rom):
    real = _mo.real_map_headers(rom.sym)
    hp = _mo.flat(*rom.sym["MapHeaderPointers"])
    hb = _mo.flat(*rom.sym["MapHeaderBanks"])
    out = bytearray()

    for mid in range(NUM_MAP_CONNECTIONS):
        conns = {d: (0xFF, 0, 0) for d, _ in CONN_DIRS}
        hdr = (rom.data[hb + mid],
               rom.data[hp + mid * 2] | (rom.data[hp + mid * 2 + 1] << 8))
        if hdr in real:
            bank, addr = hdr
            h = _mo.flat(bank, addr)
            byte = rom.data[h + 9]
            p = h + 10
            for d, bit in CONN_DIRS:
                if not (byte & bit):
                    continue
                dest = rom.data[p]
                y_align, x_align = rom.data[p + 7], rom.data[p + 8]
                p += 11
                ns = d in ("north", "south")
                coord = (y_align if ns else x_align)
                free = (x_align if ns else y_align)
                if free > 127:
                    free -= 256
                conns[d] = (dest, coord * 2 + (1 if ns else 0), free * 2)
        for d, _ in CONN_DIRS:
            dest, coord, adj = conns[d]
            out += struct.pack("<BxhH", dest, coord, adj & 0xFFFF)

    out[0x0B * 24: 0x0C * 24] = out[0x0A * 24: 0x0B * 24]
    return bytes(out)

NUM_DEX_ENTRIES = 152
_DEX = {}

def _dex_entries(rom):
    if "v" in _DEX:
        return _DEX["v"]
    import extract_trainer_texts_rom as _t
    cm = _t.load_charmap()
    ptr = rom.offset("PokedexEntryPointers")
    bank = rom.sym["PokedexEntryPointers"][0]

    texts, tindex = bytearray(), {}

    def put(s):
        if s is None:
            return 0xFFFFFFFF
        b = s.encode("utf-8", "replace") + b"\0"
        if b not in tindex:
            tindex[b] = len(texts)
            texts.extend(b)
        return tindex[b]

    d2s = dex_to_species(rom)
    recs = bytearray(12)
    for dex in range(1, NUM_DEX_ENTRIES):
        internal = d2s[dex]
        a = (rom.data[ptr + (internal - 1) * 2]
             | (rom.data[ptr + (internal - 1) * 2 + 1] << 8))
        p = bank * 0x4000 + (a - 0x4000)
        cat = []
        while rom.data[p] != 0x50:
            cat.append(cm.get(rom.data[p], "?"))
            p += 1
        p += 1
        ft, inch = rom.data[p], rom.data[p + 1]
        weight = rom.data[p + 2] | (rom.data[p + 3] << 8)

        desc = (_t.decode(rom.data, cm, p + 4)
                .replace("<PAGE>", "\\f")
                .replace("\n\n", "\\f")
                .replace("\n", "\\n"))
        recs += struct.pack("<BBHII", ft, inch, weight,
                            put("".join(cat)), put(desc))
    _DEX["v"] = (bytes(recs), bytes(texts))
    return _DEX["v"]

@asset("gDexEntryRecords", stride=12, verify=None, cut=True)
def dex_entry_records(rom):
    return _dex_entries(rom)[0]

@asset("gDexEntryText", verify=None, cut=True)
def dex_entry_text(rom):
    return _dex_entries(rom)[1]

NUM_MOVE_NAMES = 166
NUM_TRAINER_NAMES = 47

def _names(rom, symbol, count):
    import extract_trainer_texts_rom as _t
    cm = _t.load_charmap()
    p = rom.offset(symbol)
    out = []
    for _ in range(count):
        s = []
        while rom.data[p] != 0x50:
            s.append(cm.get(rom.data[p], "?"))
            p += 1
        p += 1
        out.append("".join(s))
    return out

def _move_names(rom):
    return ["------"] + _names(rom, "MoveNames", NUM_MOVE_NAMES - 1)

_GENDER_SIGN_BYTES = {"♂": 0xEF, "♀": 0xF5}

def _encode_name(n):
    out = bytearray()
    for ch in n:
        b = _GENDER_SIGN_BYTES.get(ch)
        if b is not None:
            out.append(b)
            continue
        try:
            out += ch.encode("latin-1")
        except UnicodeEncodeError:
            out += b"?"
    return bytes(out)

def _name_blob(names):
    blob, offs = bytearray(), []
    for n in names:
        offs.append(len(blob))
        blob += _encode_name(n) + b"\0"
    return bytes(blob), offs

@asset("gMoveNamesBlob", verify=None, cut=True)
def move_names_blob(rom):
    return _name_blob(_move_names(rom))[0]

@asset("gMoveNamesOffsets", stride=2, verify=None, cut=True)
def move_names_offsets(rom):
    offs = _name_blob(_move_names(rom))[1]
    return b"".join(struct.pack("<H", o) for o in offs)

@asset("gTrainerClassNamesBlob", verify=None, cut=True)
def trainer_names_blob(rom):
    return _name_blob(_names(rom, "TrainerNames", NUM_TRAINER_NAMES))[0]

@asset("gTrainerClassNamesOffsets", stride=2, verify=None, cut=True)
def trainer_names_offsets(rom):
    offs = _name_blob(_names(rom, "TrainerNames", NUM_TRAINER_NAMES))[1]
    return b"".join(struct.pack("<H", o) for o in offs)

ITEM_DATA_H = "src/data/item_data_gen.h"
ITEM_NAME_LENGTH = 13
NUM_ITEM_NAME_ROWS = 98
NUM_KEY_ITEM_FLAG_BYTES = 11

@asset("gItemNames", stride=ITEM_NAME_LENGTH, verify=ITEM_DATA_H,
       symbol="kItemNames", shape=(NUM_ITEM_NAME_ROWS, ITEM_NAME_LENGTH),
       cut=True, bind=True, elem=f"[{ITEM_NAME_LENGTH}]")
def item_names(rom):
    rows = [b"\x50" * ITEM_NAME_LENGTH]
    off = rom.offset("ItemNames")
    while len(rows) < NUM_ITEM_NAME_ROWS:
        end = rom.data.index(b"\x50", off)
        name = rom.data[off:end][:ITEM_NAME_LENGTH - 1]
        rows.append(name + b"\x50" * (ITEM_NAME_LENGTH - len(name)))
        off = end + 1
    return b"".join(rows)

@asset("gKeyItemFlags", stride=1, verify=ITEM_DATA_H, symbol="kKeyItemFlags",
       cut=True, bind=True, elem="")
def key_item_flags(rom):
    return rom.read("KeyItemFlags", NUM_KEY_ITEM_FLAG_BYTES)

@asset("gMoveAnimFallingDeltaX", stride=1, verify="src/game/battle/move_anim.c",
       symbol="sMoveAnimFallingDeltaX", cut=True, bind=True, elem="")
def move_anim_falling_delta_x(rom):
    return rom.read("FallingObjects_DeltaXs", 128)

@asset("kWavePatterns", stride=16, verify="src/platform/audio.c", cut=True,
       bind=True, elem="[16]")
def wave_patterns(rom):
    return rom.read("Audio1_WavePointers.wave0", 5 * 16)

@asset("kShockEmoteTiles", stride=1, verify="src/game/trainer_sight.c",
       cut=True, bind=True, elem="")
def shock_emote_tiles(rom):
    return rom.read("ShockEmote", 64)

@asset("kHappyEmoteTiles", stride=1, verify="src/game/trainer_sight.c",
       cut=True, bind=True, elem="")
def happy_emote_tiles(rom):
    return rom.read("HappyEmote", 64)

@asset("kPokeballTiles", stride=16, verify="src/game/battle/battle_ui.c",
       cut=True, bind=True, elem="[16]")
def pokeball_tiles(rom):
    return rom.read("PokeballTileGraphics", 4 * 16)

def _slice(name, sym, length, path, extra=0, stride=1, elem=None, symbol=None):
    @asset(name, stride=stride, verify=path, symbol=symbol or name,
           cut=True, bind=True, elem=elem if elem is not None else "")
    def provider(rom, sym=sym, length=length, extra=extra):
        return rom.read(sym, length, extra)
    provider.__name__ = "slice_" + name
    return provider

ANIM_C = "src/game/anim.c"
BCORE_C = "src/game/battle/battle_core.c"
MANIM_C = "src/game/battle/move_anim.c"
CREDITS_C = "src/game/credits_scripts.c"
FIELD_C = "src/game/field_moves.c"
GYM_C = "src/game/gym_scripts.c"
NAMING_C = "src/game/naming_screen.c"
PLAYER_C = "src/game/player.c"
PCENTER_C = "src/game/pokecenter.c"
PDEX_C = "src/game/pokedex.c"
TILEMOD_C = "src/game/amberscript_tilemod.c"
SSANNE_C = "src/game/ss_anne_scripts.c"
SUMMARY_C = "src/game/summary_screen.c"
TITLE_C = "src/game/title_screen.c"
DISPLAY_C = "src/platform/display.c"

_slice("kFlowerFrames",      "FlowerTile1", 48, ANIM_C,    stride=16, elem="[16]")
_slice("kKantoFlowerFrames", "FlowerTile1", 48, TILEMOD_C, stride=16, elem="[16]")

_slice("gMoveAnimSpiralCoords", "SpiralBallAnimationCoordinates", 43, MANIM_C,
       symbol="sMoveAnimSpiralCoords")
_slice("gMoveAnimFallingInitialX", "FallingObjects_InitialXCoords", 20, MANIM_C,
       symbol="sMoveAnimFallingInitialX")
_slice("gMoveAnimFallingInitialMove", "FallingObjects_InitialMovementData", 20,
       MANIM_C, symbol="sMoveAnimFallingInitialMove")

_slice("kCopyrightRow2", "CopyrightTextString", 16, CREDITS_C, extra=15)
_slice("kCopyrightRow3", "CopyrightTextString", 17, CREDITS_C, extra=32)
_slice("kLegalLine2",    "CopyrightTextString", 16, TITLE_C,   extra=15)
_slice("kLegalLine3",    "CopyrightTextString", 17, TITLE_C,   extra=32)

_slice("kCutTreeBlockSwaps", "CutTreeBlockSwaps", 18, FIELD_C, stride=2,
       elem="[2]")

for _i in range(4):
    _slice(f"kSpinnerFrame{_i}", "SpinnerArrowAnimTiles", 16, GYM_C,
           extra=_i * 16)

_slice("kRaisedUnderscoreTile", "HpBarAndStatusGraphics", 16, NAMING_C, extra=336)
_slice("kIdGlyph",             "HpBarAndStatusGraphics", 16, SUMMARY_C, extra=272)
_slice("kNoGlyph",             "HpBarAndStatusGraphics", 16, SUMMARY_C, extra=288)
_slice("kBoulderSmokeTile",    "SSAnneSmokePuffTile",    16, PLAYER_C)
_slice("kSmokeTileGfx",        "SSAnneSmokePuffTile",    16, SSANNE_C)
_slice("kHealMachineTiles", "PokeCenterFlashingMonitorAndHealBall", 32, PCENTER_C)
_slice("kDexDivider",       "PokedexDataDividerLine",              20, PDEX_C)

_slice("kKantoWaterBase",     "Overworld_GFX",  16, TILEMOD_C, extra=320)
_slice("kKantoFlowerBase",    "Overworld_GFX",  16, TILEMOD_C, extra=48)
_slice("kKantoGymFlowerBase", "Version_GFXEnd", 16, TILEMOD_C, extra=48)

_slice("kFadePals", "FadePal1", 24, DISPLAY_C, stride=3, elem="[3]")

BUI_C = "src/game/battle/battle_ui.c"
for _name, _off in (
        ("kBallTileNeutTop",  32), ("kBallTileNeutBot", 288),
        ("kBallTileTiltTL",   96), ("kBallTileTiltTR",  112),
        ("kBallTileTiltBL",  352), ("kBallTileTiltBR",  368),
        ("kPoofTile20",      512), ("kPoofTile21",      528),
        ("kPoofTile23",      560), ("kPoofTile24",      576),
        ("kPoofTile25",      592), ("kPoofTile30",      768),
        ("kPoofTile31",      784), ("kPoofTile32",      800),
        ("kPoofTile33",      816), ("kPoofTile34",      832)):
    _slice(_name, "MoveAnimationTiles2", 16, BUI_C, extra=_off)

sys.path.insert(0, str(REPO / "tools"))
import extract_map_objects as _mo
import extract_trainer_texts_rom as _tt

MAP_EVENT_MAPS = 256
_MAPEV = {}

def _map_events(rom):
    if "v" in _MAPEV:
        return _MAPEV["v"]
    cm = _tt.load_charmap()
    texts, tindex = bytearray(), {}

    def put(s):
        if s is None:
            return 0xFFFFFFFF
        b = s.encode("utf-8", "replace") + b"\0"
        if b not in tindex:
            tindex[b] = len(texts)
            texts.extend(b)
        return tindex[b]

    real = _mo.real_map_headers(rom.sym)
    hdr_name = {v: k[:-2] for k, v in rom.sym.items() if k.endswith("_h")}
    hp = _mo.flat(*rom.sym["MapHeaderPointers"])
    hb = _mo.flat(*rom.sym["MapHeaderBanks"])
    blob, offs = bytearray(), [NO_ENTRY] * MAP_EVENT_MAPS

    for mid in range(MAP_EVENT_MAPS):
        hdr = (rom.data[hb + mid],
               rom.data[hp + mid * 2] | (rom.data[hp + mid * 2 + 1] << 8))
        if hdr not in real:
            continue
        got = _mo.map_object_addr(rom.data, rom.sym, mid)
        if not got or got[1] >= len(rom.data):
            continue
        d = _mo.read_objects(rom.data, got[0], got[1])
        offs[mid] = len(blob)
        rec = bytearray([d["border"]])

        rec.append(len(d["warps"]))
        for w in d["warps"]:
            rec += struct.pack("<BBBB", w["x"] & 0xFF, w["y"] & 0xFF,
                               w["dest_map"], w["dest_warp"])

        rec.append(len(d["signs"]))
        for s in d["signs"]:
            t = _mo.object_text(rom.data, rom.sym, mid, s["text_id"],
                                lambda a: _tt.decode(rom.data, cm, a))
            rec += struct.pack("<BBI", s["x"] & 0xFF, s["y"] & 0xFF,
                               put(None if t == _mo.TX_ASM_MARKER else t))

        people = [o for o in d["objects"] if o["kind"] != "item"]
        items = [o for o in d["objects"] if o["kind"] == "item"]
        rec.append(len(people))
        for o in people:
            t = _mo.object_text(rom.data, rom.sym, mid, o["text_id"],
                                lambda a: _tt.decode(rom.data, cm, a))

            rec += struct.pack("<BBBBBI", o["x"] & 0xFF, o["y"] & 0xFF,
                               o["sprite"], o["movement"], o["text_id"],
                               put(None if t == _mo.TX_ASM_MARKER else t))

        rec.append(len(items))
        for o in items:
            rec += struct.pack("<BBB", o["x"] & 0xFF, o["y"] & 0xFF,
                               o.get("item_id", 0))

        trainers = [(i, o) for i, o in enumerate(people)
                    if o["kind"] == "trainer"]

        rname = hdr_name.get(hdr)
        heads = []
        if rname:
            try:
                heads = _tt.trainers_for(rom.data, rom.sym, cm, rname) or []
            except Exception:
                heads = []
        rec.append(len(trainers))
        for n_i, (idx, o) in enumerate(trainers):
            h = heads[n_i] if n_i < len(heads) else None
            rec += struct.pack("<BBBIII", idx, o.get("trainer_class", 0),
                               o.get("trainer_no", 0),
                               put(h["before"] if h else None),
                               put(h["after"] if h else None),
                               put(h["end"] if h else None))
        blob += rec

    _MAPEV["v"] = (bytes(blob), offs, bytes(texts))
    return _MAPEV["v"]

@asset("gMapEventBlob", verify=None, cut=True)
def map_event_blob(rom):
    return _map_events(rom)[0]

@asset("gMapEventOffsets", stride=2, verify=None, cut=True)
def map_event_offsets(rom):
    return b"".join(struct.pack("<H", o) for o in _map_events(rom)[1])

@asset("gMapEventText", verify=None, cut=True)
def map_event_text(rom):
    return _map_events(rom)[2]

NUM_INTERNAL_SPECIES = 190

@asset("gSpeciesToDex", stride=1, cut=True, bind=True,
       verify="src/data/base_stats.c", shape=(256,))
def species_to_dex(rom):
    base = rom.offset("PokedexOrder")
    out = bytearray(256)
    for i in range(NUM_INTERNAL_SPECIES):
        out[i + 1] = rom.data[base + i]
    return bytes(out)

@asset("gDexToSpecies", stride=1, cut=True, bind=True, verify=None)
def dex_to_species(rom):
    base = rom.offset("PokedexOrder")
    out = bytearray(152)
    for i in range(NUM_INTERNAL_SPECIES):
        dex = rom.data[base + i]
        if 1 <= dex < 152:
            out[dex] = i + 1
    return bytes(out)

@asset("kTypeChart", stride=3, cut=True, bind=True,
       ctype="type_entry_t", ctype_header="data/type_chart.h",
       verify=None)
def type_chart(rom):
    p = base = rom.offset("TypeEffects")
    while rom.data[p] != 0xFF:
        p += 3

    return bytes(rom.data[base:p]) + b"\xFF\xFF\x00"

sys.path.insert(0, str(REPO / "tools"))
import extract_sfx as _sfx

_SFX_CACHE = {}

def _sfx_table(rom):
    if "t" not in _SFX_CACHE:
        _SFX_CACHE["t"] = _sfx.build_table(rom.data, rom.sym)
    return _SFX_CACHE["t"]

def sfx_counts(rom):
    t = _sfx_table(rom)
    return len(t[0]), len(t[1])

def sfx_names(rom):
    return _sfx_table(rom)[3]

@asset("gSfxDefs", stride=4, verify=None, cut=True, bind=True,
       ctype="sfx_def_t", ctype_header="data/move_sfx_structs.h")
def sfx_defs(rom):
    defs = _sfx_table(rom)[0]
    return b"".join(struct.pack("<BBH", bank, n, first) for bank, n, first in defs)

@asset("gSfxChannels", stride=6, verify=None, cut=True, bind=True,
       ctype="sfx_channel_t", ctype_header="data/move_sfx_structs.h")
def sfx_channels(rom):
    chans = _sfx_table(rom)[1]
    return b"".join(struct.pack("<BxHH", hw, first, n) for hw, first, n in chans)

@asset("gSfxCmds", stride=10, verify=None, cut=True, bind=True,
       ctype="move_sfx_cmd_t", ctype_header="data/move_sfx_structs.h")
def sfx_cmds(rom):

    cmds = _sfx_table(rom)[2]
    return b"".join(struct.pack("<BxHHHH", t, *(v & 0xFFFF for v in rest))
                    for t, *rest in cmds)

@asset("gMoveSfxData", stride=4, verify=None, cut=True, bind=True,
       ctype="move_sfx_data_t", ctype_header="data/move_sfx_data.h")
def move_sfx_data(rom):
    slot_of = _sfx_table(rom)[5]
    base = rom.offset("MoveSoundTable")
    out = bytearray()
    for i in range(MOVE_SFX_DATA_COUNT):
        sid, pitch, tempo = rom.data[base + 3 * i: base + 3 * i + 3]
        out += struct.pack("<HBB", slot_of.get((2, sid), 0xFFFF), pitch, tempo)
    return bytes(out)

MOVE_SFX_DATA_COUNT = 166

MOVE_ANIM_NUM_FRAMEBLOCKS  = 122
MOVE_ANIM_NUM_SUBANIMS     = 86
MOVE_ANIM_NUM_ATTACK_ANIMS = 203
MOVE_ANIM_NUM_BASECOORDS   = 177

MOVE_ANIM_FIRST_SE_ID = 0xC0

def _anim_ptrs(rom, name, count):
    base = rom.offset(name)
    bank = rom.sym[name][0]
    return [bank * 0x4000 + ((rom.data[base + 2 * i]
                              | (rom.data[base + 2 * i + 1] << 8)) - 0x4000)
            for i in range(count)]

def _frameblock_entries(rom):
    out = []
    for p in _anim_ptrs(rom, "FrameBlockPointers", MOVE_ANIM_NUM_FRAMEBLOCKS):
        n = rom.data[p]
        rec = bytearray([n])
        for s in range(n):
            py, px, tile, attr = rom.data[p + 1 + s * 4: p + 5 + s * 4]
            rec += bytes([px // 8, py // 8, px % 8, py % 8, tile, attr])
        out.append(bytes(rec))
    return out

def _subanim_entries(rom):
    out = []
    for p in _anim_ptrs(rom, "SubanimationPointers", MOVE_ANIM_NUM_SUBANIMS):
        h = rom.data[p]
        typ, n = h >> 5, h & 0x1F
        out.append(bytes([typ, n]) + bytes(rom.data[p + 1: p + 1 + n * 3]))
    return out

def _attack_anim_entries(rom):
    out = []
    for p in _anim_ptrs(rom, "AttackAnimationPointers", MOVE_ANIM_NUM_ATTACK_ANIMS):
        q = p
        while rom.data[q] != 0xFF:
            q += 2 if rom.data[q] >= MOVE_ANIM_FIRST_SE_ID else 3
        out.append(bytes(rom.data[p:q + 1]))
    return out

@asset("gMoveAnimFrameBlockBlob", verify=None, cut=True)
def move_anim_frameblock_blob(rom):
    return _blob_and_offsets(_frameblock_entries(rom),
                             MOVE_ANIM_NUM_FRAMEBLOCKS, 0)[0]

@asset("gMoveAnimFrameBlockOffsets", stride=2, verify=None, cut=True)
def move_anim_frameblock_offsets(rom):
    offs = _blob_and_offsets(_frameblock_entries(rom),
                             MOVE_ANIM_NUM_FRAMEBLOCKS, 0)[1]
    return b"".join(struct.pack("<H", o) for o in offs)

@asset("gMoveAnimSubanimBlob", verify=None, cut=True)
def move_anim_subanim_blob(rom):
    return _blob_and_offsets(_subanim_entries(rom), MOVE_ANIM_NUM_SUBANIMS, 0)[0]

@asset("gMoveAnimSubanimOffsets", stride=2, verify=None, cut=True)
def move_anim_subanim_offsets(rom):
    offs = _blob_and_offsets(_subanim_entries(rom), MOVE_ANIM_NUM_SUBANIMS, 0)[1]
    return b"".join(struct.pack("<H", o) for o in offs)

@asset("gMoveAnimScriptBlob", verify=None, cut=True)
def move_anim_script_blob(rom):
    return _blob_and_offsets(_attack_anim_entries(rom),
                             MOVE_ANIM_NUM_ATTACK_ANIMS, 0)[0]

@asset("gMoveAnimScriptOffsets", stride=2, verify=None, cut=True)
def move_anim_script_offsets(rom):
    offs = _blob_and_offsets(_attack_anim_entries(rom),
                             MOVE_ANIM_NUM_ATTACK_ANIMS, 0)[1]
    return b"".join(struct.pack("<H", o) for o in offs)

@asset("gMoveAnimFrameBlockBaseCoords", stride=2, cut=True, bind=True,
       ctype="move_anim_basecoord_t",
       ctype_header="data/move_anim_basecoords.h",
       verify="src/data/move_anim_basecoords.c",
       shape=(MOVE_ANIM_NUM_BASECOORDS, 2))
def move_anim_basecoords(rom):
    base = rom.offset("FrameBlockBaseCoords")
    return bytes(rom.data[base: base + MOVE_ANIM_NUM_BASECOORDS * 2])

@asset("gTrainerBaseMoney", stride=2, verify=None, cut=True,
       bind=True, ctype="uint16_t")
def trainer_base_money(rom):
    base = rom.offset("TrainerPicAndMoneyPointers")
    out = bytearray()
    for i in range(NUM_TRAINERS):
        p = base + 5 * i + 3
        hi, lo = rom.data[p], rom.data[p + 1]
        val = (((hi >> 4) * 10 + (hi & 0xF)) * 100
               + ((lo >> 4) * 10 + (lo & 0xF)))
        out += struct.pack("<H", val)
    return bytes(out)

NUM_POKEMON_CRIES = 190
POKEMON_CRY_BYTES = 3

@asset("g_pokemon_cries", stride=POKEMON_CRY_BYTES,
       verify="src/data/cry_data.c",
       shape=(NUM_POKEMON_CRIES, POKEMON_CRY_BYTES),
       bind=True, ctype="pokemon_cry_t", ctype_header="data/cry_types.h")
def pokemon_cries(rom):
    return rom.read("CryData", NUM_POKEMON_CRIES * POKEMON_CRY_BYTES)

NUM_BASE_CRIES = 38
CRY_SQ_NOTE_BYTES = 5
CRY_NOISE_NOTE_BYTES = 4
CRY_META_BYTES = 13

CRY_NEGATIVE_FADE_DIFF = (
    "Three channels have one MORE note than the committed table -- "
    "kcry22_ch5, kcry22_ch6 and kcry1c_ch8 are each missing their FIRST note. "
    "tools/extract_cries.py matched the fade argument with (\\d+), which "
    "cannot match a negative number, so `square_note 2, 3, -5, 897` simply "
    "failed to parse and the note was silently dropped. A negative fade is not "
    "exotic: it is the envelope DIRECTION bit, encoded by the macro as "
    "%1000 | abs(fade), and audio.c writes (vol << 4) | fade straight into "
    "NR12/NR42 -- so the raw low nibble this exporter reads is exactly what "
    "the hardware register wants. The ROM wins."
)

def _decode_cry_channel(rom, sym, noise):
    import gen1_audio as GA
    tbl = GA.command_table()
    duty, rotate, notes = 0, 0, []
    p = rom.offset(sym)
    for _ in range(4000):
        b = rom.data[p]
        if 0x20 <= b <= 0x2F:
            ln = b & 0x0F
            dn = rom.data[p + 1]
            vol, fade = dn >> 4, dn & 0x0F
            if noise:
                notes.append((ln, vol, fade, rom.data[p + 2]))
                p += 3
            else:
                freq = rom.data[p + 2] | (rom.data[p + 3] << 8)
                notes.append((ln, vol, fade, freq))
                p += 4
            continue
        entry = tbl.get(b)
        if not entry:
            break
        name, width = entry
        if name == "sound_ret":
            break
        if name == "duty_cycle":
            d = rom.data[p + 1]
            duty, rotate = (d << 6) | (d << 4) | (d << 2) | d, 0
        elif name == "duty_cycle_pattern":
            duty, rotate = rom.data[p + 1], 1
        p += 1 + width
    return duty, rotate, notes

def _cry_tables(rom):
    sq, noise, meta = bytearray(), bytearray(), bytearray()
    for i in range(NUM_BASE_CRIES):
        row = []
        for suffix, is_noise in (("Ch5", False), ("Ch6", False), ("Ch8", True)):
            sym = f"SFX_Cry{i:02X}_1_{suffix}"
            if sym not in rom.sym:
                row.append((0, 0, 0, 0))
                continue
            duty, rotate, notes = _decode_cry_channel(rom, sym, is_noise)
            if is_noise:
                off = len(noise) // CRY_NOISE_NOTE_BYTES
                for ln, vol, fade, nr43 in notes:
                    noise += bytes((ln, vol, fade, nr43))
            else:
                off = len(sq) // CRY_SQ_NOTE_BYTES
                for ln, vol, fade, freq in notes:
                    sq += struct.pack("<BBBH", ln, vol, fade, freq)
            row.append((duty, rotate, len(notes), off))

        for k, (duty, rotate, n, off) in enumerate(row):
            if k < 2:
                meta += struct.pack("<BBBH", duty, rotate, n, off)
            else:
                meta += struct.pack("<BH", n, off)
    return bytes(sq), bytes(noise), bytes(meta)

@asset("g_cry_sq_notes", stride=CRY_SQ_NOTE_BYTES, verify=None, cut=True)
def cry_sq_notes(rom):
    return _cry_tables(rom)[0]

@asset("g_cry_noise_notes", stride=CRY_NOISE_NOTE_BYTES, verify=None, cut=True)
def cry_noise_notes(rom):
    return _cry_tables(rom)[1]

@asset("g_cry_defs_meta", stride=CRY_META_BYTES, verify=None, cut=True)
def cry_defs_meta(rom):
    return _cry_tables(rom)[2]

def main():
    def_rom, def_sym = default_paths(REPO)
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rom", default=str(def_rom), help="your own Gen 1 ROM")
    ap.add_argument("--sym", default=str(def_sym), help="matching rgblink .sym")
    ap.add_argument("--out", default=str(REPO / "assets.pak"),
                    help="single-file compat pak (the `red` package)")
    ap.add_argument("--packages-dir", default=None,
                    help="one <id>.pak + <id>.json per source game, plus "
                         "packages.txt giving the mount order")
    ap.add_argument("--verify", action="store_true",
                    help="diff ROM-derived bytes against the committed src/data C arrays")
    ap.add_argument("--allow-unknown-rom", action="store_true",
                    help="development only: skip the retail hash gate")
    args = ap.parse_args()

    try:
        rom = Gen1Rom(args.rom, args.sym, allow_unknown=args.allow_unknown_rom)
    except RomError as e:
        sys.exit(f"error: {e}")

    print(f"ROM   {rom.rom_path}")
    print(f"      {rom.sha1}  {rom.title}")
    print(f"sym   {rom.sym_path}  ({len(rom.sym)} symbols)")
    print()

    builders = {}

    base_pkg = ROM_PACKAGE_ID.get(rom.sha1, "red")
    if base_pkg != "red":

        for a in ASSETS:
            if a["package"] == "red":
                a["package"] = base_pkg
        print(f"package  {base_pkg}  (from {rom.title})")
        print()

    pkg_roms = {base_pkg: rom}

    def package_rom(pkg):
        if pkg not in pkg_roms:
            rel = PACKAGE_ROMS.get(pkg)
            if not rel:
                sys.exit(f"error: asset declares unknown package '{pkg}' "
                         f"(add it to PACKAGE_ROMS)")
            rp, sp = REPO / rel[0], REPO / rel[1]
            try:
                pkg_roms[pkg] = Gen1Rom(str(rp), str(sp), allow_unknown=True)
            except RomError as e:
                sys.exit(f"error: package '{pkg}' needs {rel[0]}: {e}")
        return pkg_roms[pkg]

    def builder_for(pkg):
        if pkg not in builders:
            builders[pkg] = PakBuilder(package_rom(pkg).sha1)
        return builders[pkg]

    failures = 0
    verified = 0
    elsewhere = []
    unverified = []

    for a in ASSETS:
        try:
            data = a["fn"](rom)
        except RomError as e:
            print(f"  FAIL  {a['name']}: {e}")
            failures += 1
            continue

        builder_for(a["package"]).add(a["name"], data, stride=a["stride"])

        if args.verify:
            if not a["verify"]:

                if a.get("cut"):
                    elsewhere.append(a["name"])
                else:
                    unverified.append(a["name"])
                continue
            path = REPO / a["verify"]

            cut = (not path.is_file()
                   or not _find_definition(_c_text(path), a["symbol"]))
            if cut:
                print(f"  CUT   {a['name']:<28} {len(data):>7} bytes  "
                      f"(src/data definition deleted)")
                verified += 1
                continue
            if a.get("fields"):
                want = parse_c_struct(path, a["symbol"], a["count"], a["fields"])
            else:
                want = parse_c_bytes(path, a["symbol"], a.get("shape"))
            if want == data:
                if a["expect_diff"]:
                    print(f"  ????  {a['name']:<28} expected a difference but matched;"
                          f" remove expect_diff")
                    failures += 1
                else:
                    print(f"  ok    {a['name']:<28} {len(data):>7} bytes  == {a['verify']}")
                    verified += 1
            elif a["expect_diff"]:
                n = sum(1 for x, y in zip(want, data) if x != y)
                print(f"  FIXED {a['name']:<28} {len(data):>7} bytes, "
                      f"{n} differ from committed")
                print(f"        {a['expect_diff']}")
                verified += 1
            else:
                failures += 1
                print(f"  DIFF  {a['name']:<28} ROM {len(data)} bytes vs "
                      f"committed {len(want)} bytes  ({a['verify']})")
                for k in range(min(len(want), len(data))):
                    if want[k] != data[k]:
                        print(f"        first difference at byte {k}: "
                              f"committed {want[k]:#04x}, ROM {data[k]:#04x}")
                        break

    if args.verify:
        print()
        print(f"verified {verified}/{len(ASSETS)}  failures {failures}")
        if elsewhere:
            music = [n for n in elsewhere if n not in TABLES_CHECKED_ELSEWHERE]
            tabl = [n for n in elsewhere if n in TABLES_CHECKED_ELSEWHERE]
            print(f"checked elsewhere ({len(music)}): the music assets, "
                  f"diffed event by event by tools/romimport/check_audio.py")
            if tabl:
                print(f"checked elsewhere ({len(tabl)}): the variable-length "
                      f"tables, by tools/assetpack/check_tables.py")
        if unverified:
            print(f"unverified (no committed counterpart): {', '.join(unverified)}")
        if failures:
            sys.exit(1)
        return

    out_dir = (Path(args.packages_dir) if args.packages_dir
               else REPO / "packages" / base_pkg)
    out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for pkg in sorted(builders):
        b = builders[pkg]
        pak_path = out_dir / f"{pkg}.pak"
        nbytes = b.write(str(pak_path))
        names = sorted(a["name"] for a in ASSETS if a["package"] == pkg)
        manifest = {
            "schemaVersion": PACKAGE_SCHEMA_VERSION,
            "id": pkg,
            "sourceRomSha1": package_rom(pkg).sha1,
            "sourceRomTitle": package_rom(pkg).title,
            "importerVersion": IMPORTER_VERSION,
            "assetCount": len(names),
            "bytes": nbytes,
            "sha1": hashlib.sha1(pak_path.read_bytes()).hexdigest(),
            "assets": names,
        }
        (out_dir / f"{pkg}.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        written.append((pkg, len(names), nbytes))
        print(f"wrote {pak_path}  ({len(names)} assets, {nbytes} bytes)")

    (out_dir / "packages.txt").write_text(
        "# mount order: first listed wins a contested key\n"
        + "".join(f"{p}\n" for p, _, _ in written), encoding="utf-8")

    gen_dir = REPO / "generated" / base_pkg
    gen_dir.mkdir(parents=True, exist_ok=True)
    music_h = (REPO / "src/game/music.h").read_text(encoding="utf-8")
    port_ids = {}
    for line in music_h.splitlines():
        m = re.match(r"#define\s+(MUSIC_\w+)\s+(\d+)", line)
        if m and int(m.group(2)):
            port_ids[m.group(1)] = int(m.group(2))

    ALIAS = {
        "MUSIC_TITLE":      "Music_TitleScreen",
        "MUSIC_JIGGLYPUFF": "Music_JigglypuffSong",

        "MUSIC_POKEFLUTE":  "SFX_Pokeflute",
    }
    hdr_base_m = {}
    for i in (1, 2, 3):
        b, a = rom.sym["SFX_Headers_%d" % i]
        hdr_base_m[b] = a
    ENGINE_OF_BANK = {0x02: 0, 0x08: 1, 0x1F: 2}

    by_key = {}
    for name, (b, a) in rom.sym.items():
        if not name.startswith("Music_") or "_Ch" in name or "_branch" in name:
            continue
        if b not in hdr_base_m:
            continue
        off = a - hdr_base_m[b]
        if off >= 0 and off % 3 == 0:
            by_key.setdefault(name[6:].upper().replace("_", ""), (name, b, off // 3))

    rows, missing = [], []
    for name, pid in sorted(port_ids.items(), key=lambda kv: kv[1]):
        symn = ALIAS.get(name)
        if symn is None:
            hit = by_key.get(name[6:].replace("_", ""))
            symn = hit[0] if hit else None
        if symn is None or symn not in rom.sym:
            missing.append(name)
            continue
        b, a = rom.sym[symn]
        rows.append((pid, name, symn, ENGINE_OF_BANK[b], (a - hdr_base_m[b]) // 3))

    ml = ["/* GENERATED by tools/assetpack/build_pak.py. DO NOT EDIT.",
          " *",
          " * Port music id (src/game/music.h) -> the ROM's own sound id and",
          " * engine. music.h's numbering is the PORT's, not the ROM's; its",
          " * comment claiming otherwise is wrong. Derived from the symbol",
          " * table: an id is (symbol - SFX_Headers_N) / 3 within its own bank.",
          " */", "#pragma once", "#include <stdint.h>", "",
          "typedef struct { uint8_t rom_id; uint8_t engine; } music_rom_t;", ""]
    top = max(r[0] for r in rows) + 1
    ml.append("/* Indexed by the port's MUSIC_* value; engine 0xFF = no mapping. */")
    ml.append("static const music_rom_t kMusicRom[%d] = {" % top)
    for pid, name, symn, eng, rid in rows:
        ml.append("    [%-3d] = { %3d, %d },   /* %-26s %s */" % (pid, rid, eng, name, symn))
    ml.append("};")
    if missing:
        ml.append("")
        ml.append("/* NO ROM SYMBOL, left unmapped -- needs a hand-written alias above:")
        for n in missing:
            ml.append(" *   %s" % n)
        ml.append(" */")
    (gen_dir / "music_ids.h").write_text("\n".join(ml) + "\n", encoding="utf-8")
    print("  music_ids.h: %d mapped, %d unmapped" % (len(rows), len(missing)))

    up2sym = {n.upper(): n for n in rom.sym if n.startswith("SFX_")}
    sfx_rows = []
    for name, dense in sorted(sfx_names(rom).items(), key=lambda kv: kv[1]):
        real = up2sym.get(name.upper())
        if real is None:
            continue
        b, a = rom.sym[real]
        if b not in hdr_base_m:
            continue
        off = a - hdr_base_m[b]
        if off < 0 or off % 3:
            continue
        sfx_rows.append((dense, real, ENGINE_OF_BANK[b], off // 3))

    sl = ["/* GENERATED by tools/assetpack/build_pak.py. DO NOT EDIT.",
          " *",
          " * Dense SFX index (generated/sfx_ids.h) -> the ROM's own sound id",
          " * and engine. The dense index runs past 255 across three banks and",
          " * is NOT what the sound engine branches on; it wants a per-bank id.",
          " * Passing the dense index to the engine would compile and be wrong.",
          " */", "#pragma once", "#include <stdint.h>", "",
          "typedef struct { uint8_t rom_id; uint8_t engine; } sfx_rom_t;", ""]
    stop = max(r[0] for r in sfx_rows) + 1
    sl.append("static const sfx_rom_t kSfxRom[%d] = {" % stop)
    for dense, name, eng, rid in sfx_rows:
        sl.append("    [%-3d] = { %3d, %d },   /* %s */" % (dense, rid, eng, name))
    sl.append("};")
    (gen_dir / "sfx_rom_ids.h").write_text(chr(10).join(sl) + chr(10), encoding="utf-8")
    print("  sfx_rom_ids.h: %d of %d sounds mapped" % (len(sfx_rows), stop))

    gen_dir = REPO / "generated" / base_pkg
    gen_dir.mkdir(parents=True, exist_ok=True)
    names = sfx_names(rom)
    lines = ["/* GENERATED by tools/assetpack/build_pak.py. DO NOT EDIT.",
             " *",
             " * Sound name -> index into gSfxDefs. Derived from the ROM's",
             " * symbol table: an id is (symbol - SFX_Headers_N) / 3.",
             " */", "#pragma once", ""]
    for n in sorted(names):
        lines.append(f"#define {n:<34} {names[n]}")

    lines += ["", "/* Range boundaries the engine branches on"
                  " (constants/music_constants.asm). */"]

    hdr_base = {}
    for i in (1, 2, 3):
        b, a = rom.sym[f"SFX_Headers_{i}"]
        hdr_base[b] = a

    def rom_sfx_id(macro, sym_name):
        if sym_name not in rom.sym:
            raise SystemExit(f"sfx_ids: {macro} needs {sym_name}, not in the .sym")
        bank, addr = rom.sym[sym_name]
        if bank not in hdr_base:
            raise SystemExit(f"sfx_ids: {sym_name} is in bank ${bank:02x}, "
                             f"which is not an audio bank")
        off = addr - hdr_base[bank]
        if off < 0 or off % 3:
            raise SystemExit(f"sfx_ids: {sym_name} is not a header entry")
        return off // 3

    for macro, sym_name, plus in (
        ("NOISE_INSTRUMENTS_END", "SFX_Noise_Instrument19_1", 1),
        ("CRY_SFX_START",         "SFX_Cry00_1",              0),
        ("CRY_SFX_END",           "SFX_Cry25_1",              3),
        ("BATTLE_SFX_START",      "SFX_Peck",                 0),
        ("BATTLE_SFX_END",        "SFX_Silph_Scope",          1),
        ("MAX_SFX_ID_1",          "SFX_Safari_Zone_PA",       0),
        ("MAX_SFX_ID_2",          "SFX_Silph_Scope",          0),
        ("MAX_SFX_ID_3",          "SFX_Shooting_Star",        0),
    ):
        v = rom_sfx_id(macro, sym_name) + plus
        if not 0 <= v <= 0xFF:
            raise SystemExit(f"sfx_ids: {macro} = {v} does not fit in a byte")
        lines.append(f"#define {macro:<34} {v}")
    lines.append(f"#define {'SFX_STOP_ALL_MUSIC':<34} 0xFF")
    lines.append("")
    write_text_lf(gen_dir / "sfx_ids.h", "\n".join(lines))
    print(f"wrote {gen_dir / 'sfx_ids.h'}  ({len(names)} sounds)")

    rev = [""] * (max(names.values()) + 1 if names else 0)
    for n, i in names.items():
        if not rev[i]:
            rev[i] = n
    lines = ["/* GENERATED by tools/assetpack/build_pak.py. DO NOT EDIT.",
             " *",
             " * Index -> sound name, the reverse of sfx_ids.h. FOR HARNESSES:",
             " * included only by tests/stubs.c so the strings stay out of the",
             " * game binary. Lets the anim harness keep emitting names, so the",
             " * golden traces it diffs against did not have to be rewritten",
             " * when sound lookup became an index.",
             " */", "#pragma once", ""]
    lines.append(f"#define SFX_NAME_COUNT {len(rev)}")
    lines.append("static const char *const kSfxNames[SFX_NAME_COUNT] = {")
    for i, n in enumerate(rev):
        lines.append(f'    /* {i:3d} */ "{n or "?"}",')
    lines.append("};")
    lines.append("")
    write_text_lf(gen_dir / "sfx_names.h", "\n".join(lines))
    print(f"wrote {gen_dir / 'sfx_names.h'}  ({len(rev)} indices)")

    if args.out:
        base = builders.get(base_pkg)
        if base:
            n = base.write(args.out)
            print(f"wrote {args.out}  ({len(base)} assets, {n} bytes)  [compat]")

if __name__ == "__main__":
    main()
