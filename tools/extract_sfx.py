
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

SFX_NOTE_CMD = 0x20

T_DUTY_CYCLE, T_DUTY_PATTERN, T_PITCH_SWEEP = 0, 1, 2
T_SQUARE_NOTE, T_NOISE_NOTE, T_SOUND_LOOP, T_SOUND_RET = 3, 4, 5, 6

NOISE_CHANNEL = 8

def _music_only():
    sys.path.insert(0, str(REPO / "tools" / "romimport"))
    import gen1_audio
    handled = {0x10, 0xEC, 0xFC, 0xFE, 0xFF}
    return {op: nargs for op, (_name, nargs) in gen1_audio.command_table().items()
            if op not in handled}

_MUSIC_TABLE = _music_only()

def music_width(b):
    for base in (b, b & 0xF0, b & 0xF8):
        if base in _MUSIC_TABLE:
            return 1 + _MUSIC_TABLE[base]
    if b < 0xC0:
        return 1
    return None

def _signed_mag(v):
    return -(v & 0x7) if (v & 0x8) and (v & 0x7) else v

def load_symbols(path):
    syms, byaddr = {}, {}
    pat = re.compile(r"^([0-9A-Fa-f]{2,}):([0-9A-Fa-f]{4})\s+(\S+)")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = pat.match(line.split(";", 1)[0].strip())
        if m:
            bank, addr, name = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
            syms.setdefault(name, (bank, addr))
            byaddr.setdefault((bank, addr), name)
    return syms, byaddr

def flat(bank, addr):
    return addr if bank == 0 else bank * 0x4000 + (addr - 0x4000)

def decode_channel(rom, start, hw_channel, _inlining=()):
    out, p = [], start
    idx_at = {}
    loops = []
    while True:
        idx_at[p] = len(out)
        b = rom[p]
        if b == 0xFF:
            out.append((T_SOUND_RET, 0, 0, 0, 0))
            break
        if b == 0xFE:

            loops.append((len(out), rom[p + 2] | (rom[p + 3] << 8)))
            out.append((T_SOUND_LOOP, rom[p + 1], 0, 0, 0))
            p += 4
        elif b == 0xEC:
            out.append((T_DUTY_CYCLE, rom[p + 1], 0, 0, 0))
            p += 2
        elif b == 0xFC:

            n = rom[p + 1]
            out.append((T_DUTY_PATTERN, (n >> 6) & 3, (n >> 4) & 3,
                        (n >> 2) & 3, n & 3))
            p += 2
        elif b == 0x10:

            arg = rom[p + 1]
            out.append((T_PITCH_SWEEP, arg >> 4, _signed_mag(arg & 0x0F), 0, 0))
            p += 2
        elif SFX_NOTE_CMD <= b <= 0x2F:
            length = b & 0x0F
            vf = rom[p + 1]
            vol, f = vf >> 4, vf & 0x0F
            fade = -(f & 0x7) if (f & 0x8) else f
            if hw_channel == NOISE_CHANNEL:
                out.append((T_NOISE_NOTE, length, vol, fade, rom[p + 2]))
                p += 3
            else:
                out.append((T_SQUARE_NOTE, length, vol, fade,
                            rom[p + 2] | (rom[p + 3] << 8)))
                p += 4
        elif music_width(b) is not None:

            p += music_width(b)
        else:
            raise ValueError(f"unhandled sfx opcode {b:#04x} at {p:#08x} "
                             f"(channel {hw_channel})")

    bank_base = start & ~0x3FFF
    for i, target in loops:
        flat_target = bank_base + (target - 0x4000)
        if flat_target in idx_at:
            t, p0, _, p2, p3 = out[i]
            out[i] = (t, p0, idx_at[flat_target], p2, p3)
            continue

        if flat_target in _inlining:
            raise ValueError(f"cyclic cross-channel loop at {target:#06x}")
        tail, _ = decode_channel(rom, flat_target, hw_channel,
                                 tuple(_inlining) + (flat_target,))
        base = len(out)
        t, p0, _, p2, p3 = out[i]
        out[i] = (t, p0, base, p2, p3)

        out.extend((c[0], c[1], c[2] + base, c[3], c[4]) if c[0] == T_SOUND_LOOP
                   else c for c in tail)
    return out, p + 1

def sfx_at(rom, headers_flat, bank, sfx_id):
    e = headers_flat + 3 * sfx_id
    b0 = rom[e]
    count = ((b0 >> 4) >> 2) + 1
    chans = []
    for i in range(count):
        eb = rom[e + 3 * i]
        hw = (eb & 0x0F) + 1
        addr = rom[e + 3 * i + 1] | (rom[e + 3 * i + 2] << 8)
        cmds, _ = decode_channel(rom, flat(bank, addr), hw)
        chans.append((hw, addr, cmds))
    return chans

def header_symbols(syms, headers):
    named = {}
    for name, (bank, addr) in syms.items():

        if (not name.startswith("SFX_") or "_Ch" in name
                or name.startswith("SFX_Headers_")):
            continue
        for n, (hbank, hflat) in headers.items():
            if bank == hbank and flat(bank, addr) >= hflat:
                off = flat(bank, addr) - hflat
                if off % 3 == 0:
                    named[name] = (n, off // 3)
    return named

def build_table(rom, syms):
    headers = {}
    for n in (1, 2, 3):
        bank, addr = syms[f"SFX_Headers_{n}"]
        headers[n] = (bank, flat(bank, addr))

    defs, channels, cmds = [], [], []
    index_of, slot_of, skipped = {}, {}, []
    for name, (bank, sid) in sorted(header_symbols(syms, headers).items()):
        try:
            chans = sfx_at(rom, headers[bank][1], headers[bank][0], sid)
        except (ValueError, IndexError) as e:
            skipped.append((name, str(e)))
            continue
        index_of[name.upper()] = len(defs)
        slot_of[(bank, sid)] = len(defs)
        defs.append((bank, len(chans), len(channels)))
        for hw, _addr, cl in chans:
            channels.append((hw, len(cmds), len(cl)))
            cmds.extend(cl)
    return defs, channels, cmds, index_of, skipped, slot_of

def committed_streams(path):
    txt = re.sub(r"/\*.*?\*/", "", path.read_text(encoding="utf-8", errors="replace"),
                 flags=re.S)
    cmds = {}
    for m in re.finditer(r"(\w+_cmds)\[\]\s*=\s*\{(.*?)\};", txt, re.S):
        rows = re.findall(r"\{\s*(\w+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,"
                          r"\s*(-?\d+)\s*,\s*(-?\d+)\s*\}", m.group(2))
        names = {"MOVE_SFX_CMD_DUTY_CYCLE": T_DUTY_CYCLE,
                 "MOVE_SFX_CMD_DUTY_CYCLE_PATTERN": T_DUTY_PATTERN,
                 "MOVE_SFX_CMD_PITCH_SWEEP": T_PITCH_SWEEP,
                 "MOVE_SFX_CMD_SQUARE_NOTE": T_SQUARE_NOTE,
                 "MOVE_SFX_CMD_NOISE_NOTE": T_NOISE_NOTE,
                 "MOVE_SFX_CMD_SOUND_LOOP": T_SOUND_LOOP,
                 "MOVE_SFX_CMD_SOUND_RET": T_SOUND_RET}
        cmds[m.group(1)] = [(names[t], int(a), int(b), int(c), int(d))
                            for t, a, b, c, d in rows]
    out = {}
    for m in re.finditer(r"sfx_(\w+)_channels\[\]\s*=\s*\{(.*?)\};", txt, re.S):
        chans = re.findall(r"\{\s*(\d+)\s*,\s*\"([^\"]*)\"\s*,\s*(\w+)\s*,",
                           m.group(2))
        out[m.group(1)] = [(int(hw), cmds.get(arr, [])) for hw, _lbl, arr in chans]
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", type=Path,
                    default=REPO / "pokered-master" / "pokered.gbc")
    ap.add_argument("--sym", type=Path, default=None)
    ap.add_argument("--verify", action="store_true",
                    help="diff against the committed C this replaced")
    ap.add_argument("--oracle", type=Path, default=None,
                    help="the committed move_sfx_structs.c to diff against; "
                         "defaults to src/data, which the cutover removed")
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    syms, byaddr = load_symbols(args.sym or args.rom.with_suffix(".sym"))

    headers = {}
    for n in (1, 2, 3):
        name = f"SFX_Headers_{n}"
        if name not in syms:
            sys.exit(f"error: {name} not in the symbol file")
        bank, addr = syms[name]
        headers[n] = (bank, flat(bank, addr))

    named = {}
    for name, (bank, addr) in syms.items():
        for n, (hbank, hflat) in headers.items():
            if bank == hbank and flat(bank, addr) >= hflat:
                off = flat(bank, addr) - hflat
                if off % 3 == 0 and name.startswith("SFX_") and "_Ch" not in name:
                    named[name] = (n, off // 3)
    print(f"{len(named)} sfx header symbols across 3 banks")

    if not args.verify:
        return 0

    oracle = args.oracle or (REPO / "src" / "data" / "move_sfx_structs.c")
    if not oracle.exists():
        print(f"SKIPPED: no oracle at {oracle}.\n"
              f"  It was src/data/move_sfx_structs.c, deleted by the cutover.\n"
              f"  Re-run against history with --oracle (see the source).")
        return 0
    want = committed_streams(oracle)
    print(f"{len(want)} committed sounds to check\n")
    ok = bad = skip = 0
    for sym, chans in sorted(want.items()):

        cand = {k.upper(): k for k in named}
        key = cand.get(sym.upper())
        if key is None:
            skip += 1
            continue
        bank, sid = named[key]
        try:
            got = sfx_at(rom, headers[bank][1], headers[bank][0], sid)
        except ValueError as e:
            bad += 1
            if bad <= 8:
                print(f"  {sym} (bank {bank} id {sid:#04x}): {e}")
            continue
        g = [(hw, cmds) for hw, _a, cmds in got]
        if g == chans:
            ok += 1
        else:
            bad += 1
            if bad <= 5:
                print(f"  {sym} (bank {bank} id {sid:#04x})")
                print(f"    rom       {[(h, len(c)) for h, c in g]}")
                print(f"    committed {[(h, len(c)) for h, c in chans]}")
                for i, (a, b) in enumerate(zip(g, chans)):
                    if a != b:
                        print(f"    ch{i} rom      {a[1][:4]}")
                        print(f"    ch{i} committed{b[1][:4]}")
                        break
    print(f"\nsfx streams: {ok} match, {bad} differ, {skip} not found by name")
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
