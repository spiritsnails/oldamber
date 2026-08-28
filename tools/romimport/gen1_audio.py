
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

REPO = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.dirname(HERE))
PR = os.path.join(REPO, "pokered-master")
AUDIO_MACROS = os.path.join(PR, "macros", "scripts", "audio.asm")

_TABLE = None

COMMAND_TABLE = {
    0x10: ("pitch_sweep", 1),
    0xB0: ("drum_note", 1),
    0xC0: ("rest", 0),
    0xD0: ("note_type", 1),
    0xE0: ("octave", 0),
    0xE8: ("toggle_perfect_pitch", 0),
    0xEA: ("vibrato", 2),
    0xEB: ("pitch_slide", 2),
    0xEC: ("duty_cycle", 1),
    0xED: ("tempo", 2),
    0xEE: ("stereo_panning", 1),
    0xEF: ("unknownmusic0xef", 1),
    0xF0: ("volume", 1),
    0xF8: ("execute_music", 0),
    0xFC: ("duty_cycle_pattern", 1),
    0xFD: ("sound_call", 2),
    0xFE: ("sound_loop", 3),
    0xFF: ("sound_ret", 0),
}

def command_table():
    return COMMAND_TABLE

def parse_command_table():
    global _TABLE
    if _TABLE is not None:
        return _TABLE

    text = open(AUDIO_MACROS, encoding="utf-8").read()
    lines = text.splitlines()

    val = 0
    opcodes = {}
    for ln in lines:
        s = ln.strip()
        m = re.match(r"const_next\s+\$([0-9A-Fa-f]+)", s)
        if m:
            val = int(m.group(1), 16)
            continue
        if s.startswith("const_skip"):
            val += 1
            continue
        m = re.match(r"const\s+(\w+)", s)
        if m:
            opcodes[m.group(1)] = val
            val += 1
            continue
        m = re.match(r"const_def\s+\$?([0-9A-Fa-f]+)?", s)
        if m and m.group(1):
            val = int(m.group(1), 16)

    widths = {}
    cur = None
    skip_branch = False
    for ln in lines:
        s = ln.strip()
        m = re.match(r"MACRO\s+(\w+)", s)
        if m:
            cur = m.group(1)
            widths[cur] = -1
            skip_branch = False
            continue
        if s == "ENDM":
            cur = None
            continue
        if cur is None:
            continue
        if s.startswith("IF ") or s.startswith("IF\t"):
            skip_branch = False
            continue
        if s.startswith("ELSE"):
            skip_branch = True
            continue
        if s.startswith("ENDC"):
            skip_branch = False
            continue
        if skip_branch:
            continue
        if s.startswith("db "):
            widths[cur] += len(s[3:].split(","))
        elif s.startswith("dn "):
            widths[cur] += 1
        elif s.startswith("dw "):
            widths[cur] += 2 * len(s[3:].split(","))

    out = {}
    for name, op in opcodes.items():
        base = name[:-4] if name.endswith("_cmd") else name
        if base in widths and widths[base] >= 0:
            out[op] = (base, widths[base])
    _TABLE = out
    return out

import sys
sys.path.insert(0, os.path.join(REPO, "tools"))
import extract_audio as EA

NOTE_MAX = 0xBF
DRUM_NOTE = 0xB0
REST = 0xC0

SONG_ALIASES = {
    "Pokeflute": "SFX_Pokeflute",
}

SONG_VARIANTS = {
    "Cities1AltTempo": ("Music_Cities1", {
        0: "Music_Cities1_Ch1_AlternateTempo"}),
    "MeetRivalAltStart": ("Music_MeetRival", {
        0: "Music_MeetRival_Ch1_AlternateStart",
        1: "Music_MeetRival_Ch2_AlternateStart",
        2: "Music_MeetRival_Ch3_AlternateStart"}),
    "MeetRivalAltTempo": ("Music_MeetRival", {
        0: "Music_MeetRival_Ch1_AlternateTempo"}),

    "MeetRivalAltStartTempo": ("Music_MeetRival", {
        0: "Music_MeetRival_Ch1_AlternateStartAndTempo",
        1: "Music_MeetRival_Ch2_AlternateStart",
        2: "Music_MeetRival_Ch3_AlternateStart"}),
}

def song_symbol(rom, stem):
    if stem in SONG_VARIANTS:
        return SONG_VARIANTS[stem][0]
    if stem in SONG_ALIASES:
        s = SONG_ALIASES[stem]
        return s if s in rom.sym else None
    s = "Music_" + stem
    return s if s in rom.sym else None

def song_channels(rom, stem):
    sym = song_symbol(rom, stem)
    if sym is None or sym not in rom.sym:
        return []
    off = rom.offset(sym)
    bank = rom.sym[sym][0]
    nch = (rom.data[off] >> 6) + 1
    out, p = [], off
    for i in range(nch):
        cid = rom.data[p] & 0x0F
        addr = rom.data[p + 1] | (rom.data[p + 2] << 8)
        out.append((cid, bank * 0x4000 + (addr - 0x4000)))
        p += 3
    for idx, repl in SONG_VARIANTS.get(stem, (None, {}))[1].items():
        if idx < len(out) and repl in rom.sym:
            out[idx] = (out[idx][0], rom.offset(repl))
    return out

def song_tempo(rom, chan_offsets):
    if not chan_offsets:
        return 256
    off = chan_offsets[0]
    for _ in range(64):
        b = rom.data[off]
        if b == 0xED:
            return (rom.data[off + 1] << 8) | rom.data[off + 2]
        if b < 0xC0:
            break
        entry = command_table().get(b)
        if 0xD0 <= b <= 0xE7:
            off += 1 + (0 if b >= 0xE0 else 1)
        elif entry:
            off += 1 + entry[1]
        else:
            break
    return 256

class Events(list):
    loop_start = -1

def decode_channel(rom, off, is_drum=False, is_wave=False, tempo=None,
                   max_cmds=20000):
    tbl = command_table()
    st = EA.State()
    loop_target = None
    if tempo is not None:
        st.tempo = tempo
    if is_drum:

        st.speed = 12
    out = Events()
    stack = []
    loop_counts = {}
    frac_at = {}
    idx_at = {}

    for _ in range(max_cmds):

        frac_at.setdefault(off, st.frac)
        idx_at.setdefault(off, len(out))
        start = off
        b = rom.data[off]
        off += 1

        if b < REST:
            if is_drum and b >= DRUM_NOTE:
                length = (b & 0x0F) + 1
                inst = rom.data[off]
                off += 1
                delay, st.frac = EA.calc_delay(length, st.speed, st.tempo, st.frac)

                out.append({"kind": "drum", "at": start, "inst": inst,
                            "frames": delay,
                            "freq": 0, "duty": inst, "volume": 0, "env": 0})
                continue

            pitch, length = b >> 4, (b & 0x0F) + 1
            delay, st.frac = EA.calc_delay(length, st.speed, st.tempo, st.frac)
            freq = EA.calc_freq(pitch, st.octave, st.perfect_pitch)

            slide_target, slide_frames = 0, 0
            if st.slide_pending:
                slide_target = st.slide_target
                slide_frames = max(1, delay - st.slide_len_mod)
                st.slide_pending = False
                st.slide_len_mod = 0
                st.slide_target = 0
            out.append({"kind": "note", "at": off - 1, "freq": freq,
                        "frames": delay,
                        "duty": st.duty, "volume": st.volume,
                        "env": st.env_nibble, "vib": (st.vib_delay,
                                                      st.vib_rate,
                                                      st.vib_depth),
                        "slide": (slide_target, slide_frames)})
            continue

        if REST <= b < 0xD0:
            length = (b & 0x0F) + 1
            delay, st.frac = EA.calc_delay(length, st.speed, st.tempo, st.frac)

            out.append({"kind": "rest", "at": off - 1, "frames": delay,
                        "freq": 0, "duty": st.duty, "volume": 0, "env": 0})
            continue

        if 0xD0 <= b <= 0xDF:
            name, width = ("drum_speed", 0) if is_drum else ("note_type", 1)
        elif 0xE0 <= b <= 0xE7:
            name, width = "octave", 0
        else:
            entry = tbl.get(b)
            if entry is None:
                break
            name, width = entry

        if name == "note_type" and is_drum:
            width = 0
        args = rom.data[off:off + width]
        off += width

        if name == "drum_speed":

            st.speed = b & 0x0F
        elif name == "note_type":

            st.speed = b & 0x0F
            if args:

                st.volume = args[0] >> 4
                if is_wave:
                    st.wave_inst = args[0] & 0x0F
                    st.duty = st.wave_inst
                    st.env_nibble = 0
                else:
                    st.env_nibble = args[0] & 0x0F
        elif name == "octave":
            st.octave = 8 - (b & 0x0F)
        elif name == "tempo" and len(args) >= 2:
            st.tempo = (args[0] << 8) | args[1]
        elif name == "duty_cycle" and args:
            st.duty = args[0]
        elif name == "vibrato" and len(args) >= 2:
            st.vib_delay = args[0]
            st.vib_rate = args[1] >> 4
            st.vib_depth = args[1] & 0x0F
        elif name == "toggle_perfect_pitch":
            st.perfect_pitch = not st.perfect_pitch
        elif name == "pitch_slide" and len(args) >= 2:

            st.slide_len_mod = args[0] + 1
            st.slide_target = EA.calc_freq(args[1] & 0x0F,
                                           8 - ((args[1] >> 4) & 0x0F),
                                           st.perfect_pitch)
            st.slide_pending = True
        elif name == "sound_call" and len(args) >= 2:
            stack.append(off)
            off = _local(rom, off, args)
            continue
        elif name == "sound_loop" and len(args) >= 3:
            target = _local(rom, off, args[1:])
            n = args[0]
            if n == 0:

                if target not in frac_at:
                    off = target
                    continue
                loop_target = target
                break

            k = loop_counts.get(off, 0)
            if k < n:
                loop_counts[off] = k + 1
                if target in frac_at:
                    st.frac = frac_at[target]
                off = target
                continue
            loop_counts.pop(off, None)
            continue

            loop_counts.pop(off, None)
            continue
        elif name == "sound_ret":
            if stack:
                off = stack.pop()
                continue
            break

    out.loop_start = idx_at.get(loop_target, -1) if loop_target is not None else -1
    return out

def _local(rom, off, args):
    addr = args[0] | (args[1] << 8)
    bank = off // 0x4000
    return addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)

def dump_table():
    for op, (name, w) in sorted(command_table().items()):
        print("  $%02x  %-24s %d operand byte(s)" % (op, name, w))

def check_command_table():
    if not os.path.exists(AUDIO_MACROS):
        print(f"cannot check: {AUDIO_MACROS} is not here.")
        print("  a checkout is needed to re-derive the table:  "
              "cd pokered-master && make")
        return 2
    parsed = parse_command_table()
    if parsed == COMMAND_TABLE:
        print(f"")
        return 0
    print("")
    for op in sorted(set(parsed) | set(COMMAND_TABLE)):
        a, b = COMMAND_TABLE.get(op), parsed.get(op)
        if a != b:
            print(f"")
    return 1

if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check_command_table())
    dump_table()
