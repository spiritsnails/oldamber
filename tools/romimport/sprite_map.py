
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

def port_sprites():
    p = os.path.join(ROOT, "src", "data", "sprite_names_gen.h")
    src = open(p, encoding="utf-8", errors="replace").read()
    return {m.group(1) for m in re.finditer(r'\{\s*"([A-Z0-9_]+)"\s*,', src)}

def _norm(camel):
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", camel).upper()
    return re.sub(r"_+", "_", s)

CURATED = {

    "Chris": "RED", "ChrisBike": "RED", "Kris": "RED", "KrisBike": "RED",
    "Rival": "BLUE",
    "RedsMom": "MOM",

    "BugCatcher": "YOUNGSTER",
    "StandingYoungster": "YOUNGSTER",
    "UnusedGuy": "YOUNGSTER",
    "Lass": "GIRL",
    "Twin": "LITTLE_GIRL",
    "Teacher": "BRUNETTE_GIRL",
    "KimonoGirl": "BEAUTY",
    "PokefanM": "MIDDLE_AGED_MAN",
    "PokefanF": "MIDDLE_AGED_WOMAN",
    "SwimmerGuy": "SWIMMER",
    "SwimmerGirl": "SWIMMER",
    "BlackBelt": "ROCKER",
    "Sage": "CHANNELER",
    "Elder": "GRAMPS",
    "Officer": "GUARD",
    "RocketGirl": "ROCKET",
    "Pharmacist": "CLERK",
    "Receptionist": "NURSE",
    "OldLinkReceptionist": "LINK_RECEPTIONIST",
    "Cal": "COOLTRAINER_M",

    "Elm": "OAK",
    "Bill": "SUPER_NERD",
    "Kurt": "GRAMPS", "KurtOutside": "GRAMPS",
    "Falkner": "COOLTRAINER_M", "Bugsy": "YOUNGSTER",
    "Whitney": "GIRL", "Morty": "CHANNELER", "Chuck": "ROCKER",
    "Jasmine": "BEAUTY", "Pryce": "GRAMPS", "Clair": "COOLTRAINER_F",
    "Brock": "COOLTRAINER_M", "Misty": "COOLTRAINER_F", "Surge": "SAILOR",
    "Erika": "BEAUTY", "Janine": "COOLTRAINER_F", "Sabrina": "CHANNELER",
    "Blaine": "SUPER_NERD", "Will": "GENTLEMAN", "Karen": "BEAUTY",

    "Rock": "BOULDER", "FruitTree": "BOULDER",
    "BigSnorlax": "SNORLAX",
    "BigOnix": "MONSTER", "BigLapras": "MONSTER", "Slowpoke": "MONSTER",
    "Sudowoodo": "MONSTER", "Dragon": "MONSTER",
    "Entei": "MONSTER", "Raikou": "MONSTER", "Suicune": "MONSTER",
    "Surf": "MONSTER", "SurfingPikachu": "MONSTER",
    "GoldTrophy": "POKEDEX", "SilverTrophy": "POKEDEX",
    "N64": "CLIPBOARD", "Snes": "CLIPBOARD", "Famicom": "CLIPBOARD",
    "VirtualBoy": "CLIPBOARD",
}

SPRITE_VARS = 0xF0
POKEMON_SPRITE_FIRST = 0x80

POKEMON_SPECIFIC = {
    "SPRITE_SNORLAX": "SNORLAX",
    "SPRITE_MOLTRES": "BIRD", "SPRITE_ZAPDOS": "BIRD",
    "SPRITE_ARTICUNO": "BIRD", "SPRITE_PIDGEY": "BIRD",
    "SPRITE_PIDGEOT": "BIRD", "SPRITE_FEAROW": "BIRD",
    "SPRITE_DODUO": "BIRD", "SPRITE_SEEL": "SEEL",
    "SPRITE_CLEFAIRY": "FAIRY", "SPRITE_JIGGLYPUFF": "FAIRY",
}

VARIABLE_SPRITES = {
    "SPRITE_CONSOLE": "CLIPBOARD",
    "SPRITE_DOLL_1": "FAIRY", "SPRITE_DOLL_2": "FAIRY",
    "SPRITE_BIG_DOLL": "MONSTER",
    "SPRITE_WEIRD_TREE": "BOULDER",
    "SPRITE_OLIVINE_RIVAL": "BLUE",
    "SPRITE_AZALEA_ROCKET": "ROCKET",
    "SPRITE_FUCHSIA_GYM_1": "COOLTRAINER_M",
    "SPRITE_FUCHSIA_GYM_2": "COOLTRAINER_M",
    "SPRITE_FUCHSIA_GYM_3": "COOLTRAINER_M",
    "SPRITE_FUCHSIA_GYM_4": "COOLTRAINER_M",
    "SPRITE_COPYCAT": "GIRL",
    "SPRITE_JANINE_IMPERSONATOR": "COOLTRAINER_F",
}

def sprite_constants(pc_root):
    p = os.path.join(pc_root, "constants", "sprite_constants.asm")
    out, n = {}, 0
    for line in open(p, encoding="utf-8", errors="replace"):
        line = line.split(";")[0]
        m = re.match(r"\s*const_next\s+\$?([0-9A-Fa-f]+)", line)
        if m:
            n = int(m.group(1), 16)
            continue
        m = re.match(r"\s*const_def\s*\$?([0-9A-Fa-f]+)?", line)
        if m:
            n = int(m.group(1), 16) if m.group(1) else 0
            continue
        m = re.match(r"\s*const\s+(SPRITE_\w+)", line)
        if m:
            out[n] = m.group(1)
            n += 1
    return out

def crystal_token(sprite_id, consts):
    c = consts.get(sprite_id)
    if not c or not c.startswith("SPRITE_"):
        return None
    if sprite_id >= POKEMON_SPRITE_FIRST:
        return None
    return "crystal:" + c[len("SPRITE_"):]

def resolve(sprite_id, table_names, consts, port):
    nm = table_names.get(sprite_id)
    if nm:
        auto = _norm(nm)
        if auto in port:
            return auto
        if nm in CURATED and CURATED[nm] in port:
            return CURATED[nm]
    c = consts.get(sprite_id)
    if c:
        if c in VARIABLE_SPRITES and VARIABLE_SPRITES[c] in port:
            return VARIABLE_SPRITES[c]
        if c in POKEMON_SPECIFIC and POKEMON_SPECIFIC[c] in port:
            return POKEMON_SPECIFIC[c]
        if sprite_id >= POKEMON_SPRITE_FIRST and sprite_id < SPRITE_VARS:
            return "MONSTER" if "MONSTER" in port else None
    return None

def build(crystal_names):
    port = port_sprites()
    out, unmapped = {}, []
    for name in sorted(set(crystal_names)):
        auto = _norm(name)
        if auto in port:
            out[name] = auto
        elif name in CURATED and CURATED[name] in port:
            out[name] = CURATED[name]
        elif name in CURATED:
            unmapped.append(f"{name} -> {CURATED[name]} (not a port sprite)")
        else:
            unmapped.append(name)
    return out, unmapped

if __name__ == "__main__":
    import sys
    sys.path.insert(0, HERE)
    from crystal_rom import Rom
    import emit_map as E
    pc = os.path.join(ROOT, "pokecrystal-master")
    rom = Rom(os.path.join(pc, "pokecrystal.gbc"),
              os.path.join(pc, "pokecrystal.sym"))
    names = E.sprite_names(rom)
    m, un = build(names.values())
    auto = sum(1 for k, v in m.items() if _norm(k) == v)
    print(f"{len(m)}/{len(set(names.values()))} mapped "
          f"({auto} automatic, {len(m) - auto} curated), {len(un)} unmapped")
    for u in un:
        print(f"  !! {u}")
