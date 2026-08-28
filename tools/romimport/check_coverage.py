
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

PARSER_SRC = [
    os.path.join(ROOT, "src", "game", "amberscript_tilemod.c"),
    os.path.join(ROOT, "src", "game", "amberscript_mapbank.c"),
]
GENBLOCKS = os.path.join(ROOT, "mod_runtime", "generatedmaps", "kanto", "blocks")
AUTHORED = os.path.join(ROOT, "mod_runtime", "blocks")
SCENES = os.path.join(ROOT, "mod_runtime", "scenes")
EMITTER = os.path.join(ROOT, "tools", "romimport", "emit_kanto.py")
ASM = os.path.join(ROOT, "pokered-master", "scripts")

EXPECTED_ABSENT = {

    "block": "structural keyword",
    "end": "structural keyword",
    "source": "structural keyword (source quad)",
    "add": "sub-keyword, not a directive",
    "tileset": "Johto `tileset ... end` stanza; Kanto uses gbc_tileset",

    "subtile": "emitted",
    "passable": "emitted (ROM collision)",
    "cuttable": "emitted since 2026-08-07",
    "cut_replacement": "emitted since 2026-08-07",
    "counter": "emitted since 2026-08-09 (tileset header's talking-over tiles)",

    "surfable": "authored in vmap_<Map>_properties.block",
    "border_side": "authored per-side border override -- which overhang looks wrong "
                   "under the ROM's single background block is a judgement about "
                   "the art, not something the ROM states",
    "tile_sign": "authored per-placement sign text",
    "tile_if": "authored flag-gated art",
    "tile_if_not": "authored flag-gated art, inverted test -- the alt block "
                   "applies while the event is CLEAR. The form every card-key "
                   "door needs: the ROM gate callbacks write the shut door only "
                   "on the clear branch, so the map's own data is the OPEN state "
                   "and the door is the overlay. Validated by tools/check_tile_if.py.",
    "tile_ledge": "authored properties layer (747 lines) - ledge direction "
                  "varies by placement, so it is not block-derivable",
    "pair_block": "authored tile-pair collision group",
    "ledge": "authored ledge direction",
    "no_face_until": "authored per-npc override (SS Anne captain)",
    "no_door_step": "authored per-map override of the door heuristic",
    "zone_latch": "authored latched-coordinate zone",

    "scene_npc": "injected from mod_runtime/scenes/bindings.txt",
    "scene_tile": "injected from bindings.txt",
    "scene_trigger": "injected from bindings.txt",

    "include": "scene-file directive",
    "def": "scene-file macro keyword",
    "enddef": "scene-file macro keyword",

    "crystal_anim": "Crystal only",
    "crystal_env": "Crystal only",
    "grass_rustle": "Crystal only",
    "johto_trainer": "Johto only",
    "cut_span": "Crystal only (block-span cut; Gen 1 is single-cell)",
    "wild_grass_rate": "Johto naming; Kanto emits `wild_rate` (58 lines)",
    "wild_grass_slot": "Johto naming; Kanto emits `wild_encounter` (580)",
    "wild_water_rate": "Johto naming; Kanto emits `wild_rate`",
    "wild_water_slot": "Johto naming; Kanto emits `wild_encounter`",
}

ROM_SIGNALS = [
    {
        "name": "text_random (weighted random NPC dialogue)",
        "directive": "text_random",
        "asm_pattern": r"ldh a, \[hRandomAdd\]",
        "note": "not decoded -- hand-listed in emit_kanto.py's EXTRA_GATES. "
                "NB: GameCorner's hRandomAdd is the lucky-slot index, not "
                "text, so expect one false positive there.",
    },
    {
        "name": "after_battle (post-battle scene on a trainer)",
        "directive": "after_battle",
        "asm_pattern": None,
        "note": "engine parses it (amberscript_mapbank.c:2507); emitter never "
                "writes it. 4 scenes are orphaned by this.",
    },
    {
        "name": "dark (Gen-1 dark cave needing FLASH)",
        "directive": "dark",

        "asm_pattern": None,
        "note": "engine parses `dark <MapName>` (amberscript_tilemod.c:3775, "
                "AmberScript_MapSetDark) and the real-map-id checks that gate "
                "gMapPalOffset elsewhere cannot see a vmap id -- so with "
                "nothing emitting it, ROCK TUNNEL IS NOT DARK. Expect "
                "RockTunnel1F + RockTunnelB1F.",
    },
]

def read(p):
    try:
        with io.open(p, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""

def parser_directives():
    out = set()
    for src in PARSER_SRC:
        s = read(src)

        out.update(re.findall(r'strncmp\(\s*p\s*,\s*"([a-z_0-9]+) ?"', s))

        out.update(re.findall(r'strcmp\(\s*t0\s*,\s*"([a-z_0-9]+)"', s))
    return {d for d in out if len(d) > 2}

def tokens_in(dirpath, exts):
    out = {}
    if not os.path.isdir(dirpath):
        return out
    for dp, _dn, fns in os.walk(dirpath):
        for n in fns:
            if not n.endswith(exts):
                continue
            for line in read(os.path.join(dp, n)).splitlines():
                s = line.strip()
                if not s or s.startswith("#"):
                    continue
                t = s.split()[0]
                out[t] = out.get(t, 0) + 1
    return out

def main():
    accepted = parser_directives()
    emitted = tokens_in(GENBLOCKS, (".block",))
    authored = tokens_in(AUTHORED, (".block",))
    scenes = tokens_in(SCENES, (".scene",))
    emitter_src = read(EMITTER)

    findings = 0

    print("=" * 72)
    print("A. Engine parses it, but generated maps never contain it")
    print("=" * 72)
    if not emitted:
        print("  generated blocks not found -- run emit_kanto.py --all first.\n")
    suspects = []
    for d in sorted(accepted):
        if d in emitted or d in EXPECTED_ABSENT:
            continue
        where = []
        if d in authored:
            where.append("authored x%d" % authored[d])
        if d in scenes:
            where.append("scenes x%d" % scenes[d])

        knows = ('"%s' % d) in emitter_src or ("'%s" % d) in emitter_src
        suspects.append((d, where, knows))
    if not suspects:
        print("  none\n")
    for d, where, knows in suspects:
        findings += 1
        print("  %-22s emitted:0  %s%s"
              % (d,
                 (", ".join(where) if where else "used nowhere"),
                 "" if knows else "   [emitter never mentions it]"))
    print()

    print("=" * 72)
    print("B. ROM has N of these -- did we emit N?")
    print("=" * 72)
    for sig in ROM_SIGNALS:
        got = emitted.get(sig["directive"], 0)
        if sig["asm_pattern"] and os.path.isdir(ASM):
            want = 0
            hits = []
            for n in sorted(os.listdir(ASM)):
                if not n.endswith(".asm"):
                    continue
                c = len(re.findall(sig["asm_pattern"], read(os.path.join(ASM, n))))
                if c:
                    want += c
                    hits.append("%s x%d" % (n[:-4], c))
            print("  %s" % sig["name"])
            print("     ROM sites : %d  (%s)" % (want, ", ".join(hits)))
            print("     emitted   : %d line(s) of `%s`" % (got, sig["directive"]))
            if want and not got:
                findings += 1
                print("     >> NOTHING EMITTED")
            elif want:
                print("     >> check each ROM site is represented")
        else:
            print("  %s" % sig["name"])
            print("     emitted   : %d line(s) of `%s`" % (got, sig["directive"]))
            if not got:
                findings += 1
                print("     >> NOTHING EMITTED")
        print("     note      : %s" % sig["note"])
        print()

    print("=" * 72)
    print("How to read this")
    print("=" * 72)
    print("""  A directive with `emitted:0` that the emitter never even mentions is
  the classic dead feature: the parser accepts it, nothing produces it.
  Confirm by grepping the emitter for the word, zero hits while the
  parser accepts it means that feature is off game-wide.

  Coverage that comes from a hand-written table in the emitter (EXTRA_GATES,
  SUPPRESS_STATIC_NPC, port_overrides.py) is inherently incomplete: it only
  covers what somebody typed. Prefer decoding the ROM.""")

    return 1 if findings else 0

if __name__ == "__main__":
    sys.exit(main())
