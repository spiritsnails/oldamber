
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

TRAINER_ENTRY = 12
TEXT_END = 0x50

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

_CHARMAP_TABLE = {
    0x00: '<NULL>', 0x05: '\u30ac', 0x06: '\u30ae', 0x07: '\u30b0',
    0x08: '\u30b2', 0x09: '\u30b4', 0x0A: '\u30b6', 0x0B: '\u30b8',
    0x0C: '\u30ba', 0x0D: '\u30bc', 0x0E: '\u30be', 0x0F: '\u30c0',
    0x10: '\u30c2', 0x11: '\u30c5', 0x12: '\u30c7', 0x13: '\u30c9',
    0x19: '\u30d0', 0x1A: '\u30d3', 0x1B: '\u30d6', 0x1C: '\u30dc',
    0x26: '\u304c', 0x27: '\u304e', 0x28: '\u3050', 0x29: '\u3052',
    0x2A: '\u3054', 0x2B: '\u3056', 0x2C: '\u3058', 0x2D: '\u305a',
    0x2E: '\u305c', 0x2F: '\u305e', 0x30: '\u3060', 0x31: '\u3062',
    0x32: '\u3065', 0x33: '\u3067', 0x34: '\u3069', 0x3A: '\u3070',
    0x3B: '\u3073', 0x3C: '\u3076', 0x3D: '\u3079', 0x3E: '\u307c',
    0x40: '\u30d1', 0x41: '\u30d4', 0x42: '\u30d7', 0x43: '\u30dd',
    0x44: '\u3071', 0x45: '\u3074', 0x46: '\u3077', 0x47: '\u307a',
    0x48: '\u307d', 0x49: '<PAGE>', 0x4A: '<PKMN>', 0x4B: '<_CONT>',
    0x4C: '<SCROLL>', 0x4E: '<NEXT>', 0x4F: '<LINE>', 0x50: '@',
    0x51: '<PARA>', 0x52: '<PLAYER>', 0x53: '<RIVAL>', 0x54: '#',
    0x55: '<CONT>', 0x56: '<\u2026\u2026>', 0x57: '<DONE>', 0x58: '<PROMPT>',
    0x59: '<TARGET>', 0x5A: '<USER>', 0x5B: '<PC>', 0x5C: '<TM>',
    0x5D: '<TRAINER>', 0x5E: '<ROCKET>', 0x5F: '<DEXEND>', 0x60: '<BOLD_A>',
    0x61: '<BOLD_B>', 0x62: '<BOLD_C>', 0x63: '<BOLD_D>', 0x64: '<BOLD_E>',
    0x65: '<BOLD_F>', 0x66: '<BOLD_G>', 0x67: '<BOLD_H>', 0x68: '<BOLD_I>',
    0x69: '<BOLD_V>', 0x6A: '<BOLD_S>', 0x6B: '<BOLD_L>', 0x6C: '<BOLD_M>',
    0x6D: '<COLON>', 0x6E: '<LV>', 0x6F: '\u3045', 0x70: '<to>',
    0x71: '\u2019', 0x72: '<BOLD_P>', 0x73: '<ID>', 0x74: '\u00b7',
    0x75: '\u2026', 0x76: '\u3041', 0x77: '\u3047', 0x78: '\u3049',
    0x79: '\u250c', 0x7A: '\u2500', 0x7B: '\u2510', 0x7C: '\u2502',
    0x7D: '\u2514', 0x7E: '\u2518', 0x7F: ' ', 0x80: 'A',
    0x81: 'B', 0x82: 'C', 0x83: 'D', 0x84: 'E',
    0x85: 'F', 0x86: 'G', 0x87: 'H', 0x88: 'I',
    0x89: 'J', 0x8A: 'K', 0x8B: 'L', 0x8C: 'M',
    0x8D: 'N', 0x8E: 'O', 0x8F: 'P', 0x90: 'Q',
    0x91: 'R', 0x92: 'S', 0x93: 'T', 0x94: 'U',
    0x95: 'V', 0x96: 'W', 0x97: 'X', 0x98: 'Y',
    0x99: 'Z', 0x9A: '(', 0x9B: ')', 0x9C: ':',
    0x9D: ';', 0x9E: '[', 0x9F: ']', 0xA0: 'a',
    0xA1: 'b', 0xA2: 'c', 0xA3: 'd', 0xA4: 'e',
    0xA5: 'f', 0xA6: 'g', 0xA7: 'h', 0xA8: 'i',
    0xA9: 'j', 0xAA: 'k', 0xAB: 'l', 0xAC: 'm',
    0xAD: 'n', 0xAE: 'o', 0xAF: 'p', 0xB0: 'q',
    0xB1: 'r', 0xB2: 's', 0xB3: 't', 0xB4: 'u',
    0xB5: 'v', 0xB6: 'w', 0xB7: 'x', 0xB8: 'y',
    0xB9: 'z', 0xBA: '\u00e9', 0xBB: '\'d', 0xBC: '\'l',
    0xBD: '\'s', 0xBE: '\'t', 0xBF: '\'v', 0xC0: '\u305f',
    0xC1: '\u3061', 0xC2: '\u3064', 0xC3: '\u3066', 0xC4: '\u3068',
    0xC5: '\u306a', 0xC6: '\u306b', 0xC7: '\u306c', 0xC8: '\u306d',
    0xC9: '\u306e', 0xCA: '\u306f', 0xCB: '\u3072', 0xCC: '\u3075',
    0xCD: '\u3078', 0xCE: '\u307b', 0xCF: '\u307e', 0xD0: '\u307f',
    0xD1: '\u3080', 0xD2: '\u3081', 0xD3: '\u3082', 0xD4: '\u3084',
    0xD5: '\u3086', 0xD6: '\u3088', 0xD7: '\u3089', 0xD8: '\u308a',
    0xD9: '\u308b', 0xDA: '\u308c', 0xDB: '\u308d', 0xDC: '\u308f',
    0xDD: '\u3092', 0xDE: '\u3093', 0xDF: '\u3063', 0xE0: '\'',
    0xE1: '<PK>', 0xE2: '<MN>', 0xE3: '-', 0xE4: '\'r',
    0xE5: '\'m', 0xE6: '?', 0xE7: '!', 0xE8: '.',
    0xE9: '\u30a1', 0xEA: '\u30a5', 0xEB: '\u30a7', 0xEC: '\u25b7',
    0xED: '\u25b2', 0xEE: '\u25bc', 0xEF: '\u2642', 0xF0: '<ED>',
    0xF1: '\u00d7', 0xF2: '<DOT>', 0xF3: '/', 0xF4: ',',
    0xF5: '\u2640', 0xF6: '0', 0xF7: '1', 0xF8: '2',
    0xF9: '3', 0xFA: '4', 0xFB: '5', 0xFC: '6',
    0xFD: '7', 0xFE: '8', 0xFF: '9',
}

def load_charmap():
    return dict(_CHARMAP_TABLE)

CONTROL = {
    0x4F: "\n",
    0x51: "\n\n",
    0x55: "\n\n",
    0x4E: "\n",
}

TERMINATORS = {0x50, 0x57, 0x58, 0x5F}

TX_START = 0x00

TX_FAR = 0x17

def decode(rom, cm, addr, limit=600, _hops=0):
    if _hops < 4 and addr + 3 < len(rom) and rom[addr] == TX_FAR:
        far = rom[addr + 1] | (rom[addr + 2] << 8)
        bank = rom[addr + 3]
        return decode(rom, cm, flat(bank, far), limit, _hops + 1)

    out, p = [], addr
    if p < len(rom) and rom[p] == TX_START:
        p += 1
    while p < len(rom) and p - addr < limit:
        b = rom[p]
        if b in TERMINATORS:
            break
        if b in CONTROL:
            out.append(CONTROL[b])
        elif b in cm:
            out.append(cm[b])
        else:
            out.append(f"<{b:02X}>")
        p += 1
    return "".join(out)

def trainers_for(rom, syms, cm, map_name):
    sym = f"{map_name}TrainerHeader0"
    if sym not in syms:
        return None
    bank, addr = syms[sym]
    base = flat(bank, addr)
    out = []
    p = base
    while p + TRAINER_ENTRY <= len(rom) and rom[p] != 0xFF:
        ev_bit = rom[p]
        tno = rom[p + 1] >> 4
        rd = lambda o: rom[p + o] | (rom[p + o + 1] << 8)
        before, after, end = rd(4), rd(6), rd(8)
        out.append({
            "event_bit": ev_bit,
            "trainer_no": tno,
            "before": decode(rom, cm, flat(bank, before)),
            "after":  decode(rom, cm, flat(bank, after)),
            "end":    decode(rom, cm, flat(bank, end)),
        })
        p += TRAINER_ENTRY
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", type=Path,
                    default=REPO / "pokered-master" / "pokered.gbc")
    ap.add_argument("--sym", type=Path, default=None)
    ap.add_argument("--map", help="one map, e.g. MtMoonB2F")
    ap.add_argument("--diff-blocks", action="store_true",
                    help="diff against the before/after/defeat lines in "
                         "mod_runtime/blocks/*.block")
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    syms = load_symbols(args.sym or args.rom.with_suffix(".sym"))
    cm = load_charmap()

    maps = sorted({m[:-len("TrainerHeader0")] for m in syms
                   if m.endswith("TrainerHeader0")})
    if args.map:
        maps = [args.map]

    total = 0
    for name in maps:
        t = trainers_for(rom, syms, cm, name)
        if not t:
            continue
        total += len(t)
        if args.map or not args.diff_blocks:
            print(f"\n=== {name}  ({len(t)} trainers)")
            for i, e in enumerate(t):
                print(f"  [{i}] no={e['trainer_no']} bit={e['event_bit']}")
                for k in ("before", "end", "after"):
                    s = e[k].replace("\n", " / ")
                    print(f"        {k:6} {s[:96]!r}")
    print(f"\n{len(maps)} maps, {total} trainers")

if __name__ == "__main__":
    main()
