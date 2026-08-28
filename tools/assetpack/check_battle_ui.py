
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_pak
from gen1_rom import default_paths

REPO = Path(__file__).resolve().parent.parent.parent

TILE = 16

HUD_FIRST_CHAR = 0x62
HUD_TILES = 23
BOX_FIRST_CHAR = 0x79
BOX_TILES = 7

SYMBOLS = [
    "FontGraphics", "FontGraphicsEnd",
    "TextBoxGraphics", "TextBoxGraphicsEnd",
    "HpBarAndStatusGraphics", "HpBarAndStatusGraphicsEnd",
    "BattleHudTiles1", "BattleHudTiles1End",
    "BattleHudTiles2", "BattleHudTiles3", "BattleHudTiles3End",
]

SYM_RE = re.compile(r"^([0-9A-Fa-f]{2,4}):([0-9A-Fa-f]{4})\s+(\S+)")

class Rom:

    def __init__(self, path, sym_path):
        self.data = Path(path).read_bytes()
        self.sym = {}
        for line in Path(sym_path).read_text(encoding="utf-8",
                                             errors="replace").splitlines():
            m = SYM_RE.match(line)
            if m:
                self.sym.setdefault(m.group(3),
                                    (int(m.group(1), 16), int(m.group(2), 16)))
        missing = [s for s in SYMBOLS if s not in self.sym]
        if missing:
            sys.exit(f"{sym_path} is missing symbols: {', '.join(missing)}\n"
                     f"  is this really a pokered .sym?")

    def off(self, name):
        bank, addr = self.sym[name]
        return addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)

    def span(self, start, end):
        a, b = self.off(start), self.off(end)
        if b <= a:
            sys.exit(f"{start}..{end} is empty or inverted -- wrong ROM?")
        return self.data[a:b]

def double(src):
    out = bytearray()
    for b in src:
        out.append(b)
        out.append(b)
    return bytes(out)

def tiles(data):
    return [data[i:i + TILE] for i in range(0, len(data), TILE)]

def extract(rom):
    font = tiles(double(rom.span("FontGraphics", "FontGraphicsEnd")))
    textbox = tiles(rom.span("TextBoxGraphics", "TextBoxGraphicsEnd"))

    span = {}
    base = tiles(rom.span("HpBarAndStatusGraphics", "HpBarAndStatusGraphicsEnd"))
    for i, t in enumerate(base):
        span[HUD_FIRST_CHAR + i] = t
    hud1 = tiles(double(rom.span("BattleHudTiles1", "BattleHudTiles1End")))
    for i, t in enumerate(hud1):
        span[0x6D + i] = t
    hud23 = tiles(double(rom.span("BattleHudTiles2", "BattleHudTiles3End")))
    for i, t in enumerate(hud23):
        span[0x73 + i] = t

    blank = b"\x00" * TILE
    hud = [span.get(HUD_FIRST_CHAR + i, blank) for i in range(HUD_TILES)]
    box = [textbox[BOX_FIRST_CHAR - 0x60 + i] for i in range(BOX_TILES)]

    clash = [c for c in range(BOX_FIRST_CHAR, 0x80)
             if c in span and span[c] != textbox[c - 0x60]]
    return font, box, hud, clash

def main():
    ap = argparse.ArgumentParser()
    rom_default, sym_default = default_paths(REPO)
    ap.add_argument("--rom", default=str(rom_default),
                    help="your own Pokemon Red ROM (.gbc). "
                         "`make` in pokered-master builds one, sha1-verified.")
    ap.add_argument("--sym", default=str(sym_default))
    args = ap.parse_args()

    for p in (args.rom, args.sym):
        if not Path(p).is_file():
            sys.exit(f"no such file: {p}\n  cd pokered-master && make")

    rom = Rom(args.rom, args.sym)
    font, box, hud, clash = extract(rom)
    print(f"{len(font)} font, {len(box)} box, {len(hud)} HUD tiles read direct")
    if clash:
        print(f"note: the battle's HP-bar write would also change chars "
              f"{', '.join(f'${c:02X}' for c in clash)}, which this port keeps "
              f"as text-box tiles throughout")
    else:
        print("note: the battle's HP-bar write covers $79-$7F too, but those "
              "tiles are identical to the text-box ones -- no divergence")

    from gen1_rom import Gen1Rom
    packrom = Gen1Rom(args.rom, args.sym)
    by_name = {a["name"]: a for a in build_pak.ASSETS}

    bad = 0
    for name, want in (("gFontTiles", font), ("gBoxTiles", box),
                       ("gHudTiles", hud)):
        if name not in by_name:
            print(f"FAIL: {name} has no provider in build_pak.py")
            bad += 1
            continue
        got = by_name[name]["fn"](packrom)
        expect = b"".join(bytes(t) for t in want)
        if got == expect:
            print(f"  {name}: matches the provider ({len(want)} tiles)")
        else:
            n = sum(1 for i in range(0, min(len(got), len(expect)), TILE)
                    if got[i:i + TILE] != expect[i:i + TILE])
            print(f"  {name}: DIFFERS -- {len(got)} B from the provider vs "
                  f"{len(expect)} B read direct, {n} tiles differ")
            bad += 1

    if bad:
        sys.exit(f"{bad} asset(s) disagree with an independent read of the ROM")
    print("all three agree")

if __name__ == "__main__":
    main()
