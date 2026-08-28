
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
PR = os.path.join(REPO, "pokered-master")

TX_START = 0x00
TX_FAR = 0x17
TERMINATOR = 0x50

CONTROL = {

    0x05: "\n",
    0x49: "\f",

    0x4B: "\x0b",
    0x4C: "\x0b",
    0x4E: "\n",
    0x4F: "\n",
    0x51: "\f",
    0x55: "\x0b",
    0x57: "",
    0x58: "",
}
ENDERS = {0x50, 0x57, 0x58}

SUBSTITUTIONS = {
    "<PLAYER>": "{PLAYER}",
    "<RIVAL>":  "{RIVAL}",
    "<TARGET>": "{TARGET}",
    "<USER>":   "{USER}",
    "<PKMN>":   "#MON",
}

_CHARMAP_TABLE = {
    0x49: '<PAGE>', 0x4A: '<PKMN>', 0x4B: '<_CONT>', 0x4C: '<SCROLL>',
    0x4E: '<NEXT>', 0x4F: '<LINE>', 0x50: '@', 0x51: '<PARA>',
    0x52: '<PLAYER>', 0x53: '<RIVAL>', 0x54: '#', 0x55: '<CONT>',
    0x56: '<\u2026\u2026>', 0x57: '<DONE>', 0x58: '<PROMPT>', 0x59: '<TARGET>',
    0x5A: '<USER>', 0x5B: '<PC>', 0x5C: '<TM>', 0x5D: '<TRAINER>',
    0x5E: '<ROCKET>', 0x5F: '<DEXEND>', 0x60: '\u2032', 0x61: '\u2033',
    0x62: '<BOLD_C>', 0x63: '<BOLD_D>', 0x64: '<BOLD_E>', 0x65: '<BOLD_F>',
    0x66: '<BOLD_G>', 0x67: '<BOLD_H>', 0x68: '<BOLD_I>', 0x69: '<BOLD_V>',
    0x6A: '<BOLD_S>', 0x6B: '<BOLD_L>', 0x6C: '<BOLD_M>', 0x6D: '<COLON>',
    0x6E: '<LV>', 0x6F: '\u3045', 0x70: '<to>', 0x71: '\u2019',
    0x72: '<BOLD_P>', 0x73: '<ID>', 0x74: '\u2116', 0x75: '\u2026',
    0x76: '\u3041', 0x77: '\u3047', 0x78: '\u3049', 0x79: '\u250c',
    0x7A: '\u2500', 0x7B: '\u2510', 0x7C: '\u2502', 0x7D: '\u2514',
    0x7E: '\u2518', 0x7F: ' ', 0x80: 'A', 0x81: 'B',
    0x82: 'C', 0x83: 'D', 0x84: 'E', 0x85: 'F',
    0x86: 'G', 0x87: 'H', 0x88: 'I', 0x89: 'J',
    0x8A: 'K', 0x8B: 'L', 0x8C: 'M', 0x8D: 'N',
    0x8E: 'O', 0x8F: 'P', 0x90: 'Q', 0x91: 'R',
    0x92: 'S', 0x93: 'T', 0x94: 'U', 0x95: 'V',
    0x96: 'W', 0x97: 'X', 0x98: 'Y', 0x99: 'Z',
    0x9A: '(', 0x9B: ')', 0x9C: ':', 0x9D: ';',
    0x9E: '[', 0x9F: ']', 0xA0: 'a', 0xA1: 'b',
    0xA2: 'c', 0xA3: 'd', 0xA4: 'e', 0xA5: 'f',
    0xA6: 'g', 0xA7: 'h', 0xA8: 'i', 0xA9: 'j',
    0xAA: 'k', 0xAB: 'l', 0xAC: 'm', 0xAD: 'n',
    0xAE: 'o', 0xAF: 'p', 0xB0: 'q', 0xB1: 'r',
    0xB2: 's', 0xB3: 't', 0xB4: 'u', 0xB5: 'v',
    0xB6: 'w', 0xB7: 'x', 0xB8: 'y', 0xB9: 'z',
    0xBA: '\u00e9', 0xBB: '\'d', 0xBC: '\'l', 0xBD: '\'s',
    0xBE: '\'t', 0xBF: '\'v', 0xE0: '\'', 0xE1: '<PK>',
    0xE2: '<MN>', 0xE3: '-', 0xE4: '\'r', 0xE5: '\'m',
    0xE6: '?', 0xE7: '!', 0xE8: '.', 0xE9: '\u30a1',
    0xEA: '\u30a5', 0xEB: '\u30a7', 0xEC: '\u25b7', 0xED: '\u25b6',
    0xEE: '\u25bc', 0xEF: '\u2642', 0xF0: '\u00a5', 0xF1: '\u00d7',
    0xF2: '<DOT>', 0xF3: '/', 0xF4: ',', 0xF5: '\u2640',
    0xF6: '0', 0xF7: '1', 0xF8: '2', 0xF9: '3',
    0xFA: '4', 0xFB: '5', 0xFC: '6', 0xFD: '7',
    0xFE: '8', 0xFF: '9',
}

def charmap():
    return _CHARMAP_TABLE

def decode_text(rom, off, max_len=1024):
    cm = charmap()
    out = []
    start = off
    steps = 0
    while steps < max_len:
        steps += 1
        b = rom.data[off]
        off += 1

        if b == TX_FAR:
            addr = rom.data[off] | (rom.data[off + 1] << 8)
            bank = rom.data[off + 2]
            off += 3
            far = addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)
            text, _ = decode_text(rom, far, max_len)
            return "".join(out) + text, off

        if b == TX_START and not out:
            continue

        if b in ENDERS:
            break
        if b in CONTROL:
            out.append(CONTROL[b])
            continue

        if b < 0x18:

            return None, off

        tok = cm.get(b)
        if tok is None:
            out.append("?")
        elif len(tok) == 1:
            out.append(tok)
        elif tok.startswith("<") and tok.endswith(">"):

            out.append(SUBSTITUTIONS.get(tok, tok))
        else:
            out.append(tok)

    return "".join(out).rstrip(" "), off

def text_pointer_table(rom, map_id, hdr_off, bank):
    addr = rom.data[hdr_off + 5] | (rom.data[hdr_off + 6] << 8)
    return addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)

def map_texts(rom, hdr_off, bank, count):
    tbl = text_pointer_table(rom, 0, hdr_off, bank)
    out = []
    for i in range(count):
        p = tbl + i * 2
        addr = rom.data[p] | (rom.data[p + 1] << 8)
        if addr == 0:
            out.append(None)
            continue
        off = addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)
        try:
            txt, _ = decode_text(rom, off)
        except (IndexError, RecursionError):
            txt = None
        out.append(txt)
    return out
