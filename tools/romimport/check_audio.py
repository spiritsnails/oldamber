
import argparse
import os
import re
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(_HERE), "assetpack"))
sys.path.insert(0, _HERE)

from gen1_rom import Gen1Rom, default_paths
from port_overrides import AUDIO_REFERENCE_DIVERGENCES
import gen1_audio as A

ORACLES = (os.path.join("generated", "music_data_gen.h"),
           os.path.join("src", "data", "music_data_gen.h"))

N_FIELDS = 10

def committed_loop_start(txt, stem, cid):
    m = re.search(r"static const ch_data_t k%s_Ch%d = \{ \w+, (\d+), (-?\d+) \};"
                  % (stem, cid + 1), txt)
    return int(m.group(2)) if m else None

def committed_events(txt, stem, cid):
    m = re.search(r"k%s_Ch%d_notes\[\]\s*=\s*\{(.*?)\n\};" % (stem, cid + 1),
                  txt, re.S)
    if not m:
        return None
    return [tuple([int(x, 0) for x in
                   re.findall(r"0[xX][0-9a-fA-F]+|-?\d+", row)][:N_FIELDS])
            for row in re.findall(r"\{([^{}]*)\}", m.group(1))]

def decoded_events(rom, cid, flat, tempo):
    ev = A.decode_channel(rom, flat, is_drum=(cid == 3), is_wave=(cid == 2),
                          tempo=tempo)
    out = []
    for e in ev:
        vib = e.get("vib", (0, 0, 0))
        slide = e.get("slide", (0, 0))
        out.append((e.get("freq", 0), e["frames"], e.get("duty", 0),
                    e.get("volume", 0),
                    (e.get("volume", 0) << 4) | e.get("env", 0),
                    vib[0], vib[1], vib[2], slide[0], slide[1]))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    ap.add_argument("--root", default=".")
    ap.add_argument("--oracle", help="music_data_gen.h to diff against")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    rom_path, sym_path = default_paths(args.root)
    rom = Gen1Rom(args.rom or rom_path, args.sym or sym_path)

    oracle = args.oracle or next(
        (p for p in (os.path.join(args.root, o) for o in ORACLES)
         if os.path.isfile(p)), None)
    if not oracle:
        print("SKIPPED: no oracle to check against.\n"
              "  Regenerate it from the disassembly with:\n"
              "    python tools/extract_audio.py > generated/music_data_gen.h\n"
              "  (gitignored -- it is ROM-derived and is not committed.)")
        return 0
    with open(oracle, "rb") as f:
        txt = f.read().decode("utf-8", "replace")
    print("oracle              : %s" % oracle)

    stems = sorted(set(re.findall(r"static const note_evt_t k(\w+?)_Ch\d_notes",
                                  txt)))
    tot = match = 0
    exact, declared, bad, unresolved = [], [], [], []
    loop_bad = []

    for stem in stems:
        chans = A.song_channels(rom, stem)
        if not chans:
            unresolved.append(stem)
            continue
        tempo = A.song_tempo(rom, [c[1] for c in chans])
        st_tot = st_match = 0
        diffs = []
        for cid, flat in chans:
            com = committed_events(txt, stem, cid)
            if com is None:
                continue
            got = decoded_events(rom, cid, flat, tempo)
            n = min(len(com), len(got))
            ok = sum(1 for k in range(n) if com[k] == got[k])
            span = max(len(com), len(got))
            if ok != span:
                first = next((k for k in range(n) if com[k] != got[k]), n)
                diffs.append((cid, ok, span, first))
            want_ls = committed_loop_start(txt, stem, cid)
            got_ls = A.decode_channel(rom, flat, is_drum=(cid == 3),
                                      is_wave=(cid == 2),
                                      tempo=tempo).loop_start
            if want_ls is not None and want_ls != got_ls:
                loop_bad.append((stem, cid + 1, want_ls, got_ls))
            st_tot += span
            st_match += ok
        tot += st_tot
        match += st_match
        if not st_tot:
            unresolved.append(stem)
        elif not diffs:
            exact.append(stem)
        elif all((stem, c) in AUDIO_REFERENCE_DIVERGENCES for c, _, _, _ in diffs):
            declared.append((stem, diffs))
        else:
            bad.append((stem, st_match, st_tot, diffs))

    print("songs               : %d" % len(stems))
    print("exact               : %d" % len(exact))
    print("declared divergence : %d" % len(declared))
    print("events matching     : %d/%d (%.2f%%)"
          % (match, tot, 100.0 * match / max(1, tot)))
    print("loop points         : all match" if not loop_bad
          else "loop points         : %d WRONG" % len(loop_bad))
    for stem, diffs in declared:
        for cid, ok, span, _ in diffs:
            print("  ~ %s ch%d %d/%d -- %s"
                  % (stem, cid + 1, ok, span,
                     AUDIO_REFERENCE_DIVERGENCES[(stem, cid)].split(".")[0]))

    if loop_bad:
        print("\nFAIL: %d channel(s) have the wrong LOOP POINT -- every event "
              "can be correct and the track still restart in the wrong "
              "place:" % len(loop_bad))
        for stem, ch, want, got in loop_bad:
            print("  x %-26s ch%d  committed %4d  decoded %4d"
                  % (stem, ch, want, got))

    if unresolved:
        print("\nFAIL: %d song(s) resolved to no ROM channel -- see the naming "
              "traps in this file's docstring:" % len(unresolved))
        for stem in unresolved:
            print("  ? %s" % stem)
    if bad:
        print("\nFAIL: %d song(s) differ with no declared reason:" % len(bad))
        for stem, ok, span, diffs in bad:
            print("  x %-26s %d/%d" % (stem, ok, span))
            for cid, cok, cspan, first in diffs:
                print("      ch%d %d/%d, first differing event %d"
                      % (cid + 1, cok, cspan, first))
        print("\nA divergence is only acceptable with a MECHANISM. Find why "
              "the walker and the reference disagree; if the ROM is right, "
              "record it in port_overrides.AUDIO_REFERENCE_DIVERGENCES.")

    return 1 if (bad or unresolved or loop_bad) else 0

if __name__ == "__main__":
    sys.exit(main())
