
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCKS = os.path.join(ROOT, "mod_runtime", "generatedmaps", "kanto", "blocks")
BINDINGS = os.path.join(ROOT, "mod_runtime", "scenes", "bindings.txt")

ACKNOWLEDGED = {

    ("CeladonPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("CeruleanPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("CinnabarPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("FuchsiaPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("LavenderPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("MtMoonPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("PewterPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("RockTunnelPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("SaffronPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("VermilionPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("ViridianPokecenter", "LINK_RECEPTIONIST", 11, 2): "no link cable support",
    ("IndigoPlateauLobby", "LINK_RECEPTIONIST", 13, 6): "no link cable support",

    ("MtMoonPokecenter", "CLIPBOARD", 7, 2): "empty string in the ROM itself",
}

NPC_RE = re.compile(r'^npc\s+(\S+)\s+(\S+)\s+(-?\d+)\s+(-?\d+)\s+(\S+)\s+""\s*$')

def silent_npcs():
    out = []
    if not os.path.isdir(BLOCKS):
        sys.stderr.write(
            "no generated blocks at %s\n"
            "run: tools/romimport/emit_kanto.py --all\n" % BLOCKS)
        sys.exit(2)
    for fn in sorted(os.listdir(BLOCKS)):
        if not fn.endswith(".block"):
            continue
        path = os.path.join(BLOCKS, fn)
        for line in io.open(path, encoding="utf-8", errors="replace"):
            m = NPC_RE.match(line.strip())
            if m:
                out.append((m.group(1), m.group(2), int(m.group(3)),
                            int(m.group(4)), fn))
    return out

def bound_coords():
    out = set()
    if not os.path.isfile(BINDINGS):
        return out
    for line in io.open(BINDINGS, encoding="utf-8", errors="replace"):
        parts = line.split()
        if len(parts) >= 5 and parts[0] == "scene_npc":
            try:
                out.add((parts[1], int(parts[3]), int(parts[4])))
            except ValueError:
                pass
    return out

def main():
    show_all = "--all" in sys.argv[1:]
    bound = bound_coords()

    unbound, waived = [], []
    for mp, sprite, x, y, fn in silent_npcs():
        if (mp, x, y) in bound:
            continue
        key = (mp, sprite, x, y)
        (waived if key in ACKNOWLEDGED else unbound).append((key, fn))

    if show_all and waived:
        print("acknowledged silent NPCs (%d):" % len(waived))
        for key, _fn in waived:
            print("    %-24s %-18s (%2d,%2d)  -- %s"
                  % (key[0], key[1], key[2], key[3], ACKNOWLEDGED[key]))
        print("")

    if not unbound:
        print("check_npc_bindings: OK -- every silent NPC is bound or "
              "acknowledged (%d waived)" % len(waived))
        return 0

    print("check_npc_bindings: %d NPC(s) do NOTHING when talked to\n"
          % len(unbound))
    for key, fn in unbound:
        mp, sprite, x, y = key
        print("  %s: %s at (%d,%d)" % (mp, sprite, x, y))
        print("      generated in %s with empty dialogue, and nothing binds it."
              % fn)
        print("      Fix by ONE of:")
        print("        - scene:   add to mod_runtime/scenes/bindings.txt")
        print("                     scene_npc %s <scene> %d %d" % (mp, x, y))
        print("        - service: add to emit_kanto.py's SCRIPT_NPC_SERVICE")
        print('                     ("%s", "%s", %d, %d): "service:<name>",'
              % (mp, sprite, x, y))
        print("                   (the service must exist in "
              "pks_resolve_npc_service)")
        print("        - waive:   add to ACKNOWLEDGED in this file, WITH the")
        print("                   reason it is legitimately silent")
        print("")
    return 1

if __name__ == "__main__":
    sys.exit(main())
