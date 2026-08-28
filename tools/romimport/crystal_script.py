
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

PC = os.path.join(ROOT, "pokecrystal-master")

_TABLE = None

def command_table():
    global _TABLE
    if _TABLE is not None:
        return _TABLE
    path = os.path.join(PC, "macros", "scripts", "events.asm")
    text = open(path, encoding="utf-8").read()
    order = [m.group(1) for m in
             re.finditer(r"^\tconst (\w+)_command\b", text, re.M)]
    bodies = {m.group(1): m.group(2) for m in
              re.finditer(r"^MACRO (\w+)\n(.*?)^ENDM", text, re.M | re.S)}
    widths = {"db": 1, "dw": 2, "dba": 3, "dbw": 3, "dwb": 3}
    table = {}
    for op, name in enumerate(order):
        body = bodies.get(name)
        if body is None:
            table[op] = (name, None)
            continue

        lines = [ln.split(";")[0].strip() for ln in body.splitlines()]
        start = next((i for i, ln in enumerate(lines)
                      if re.match(rf"^db\s+{name}_command\b", ln)), None)
        if start is None:
            table[op] = (name, None)
            continue
        kinds, ok = [], True
        for ln in lines[start + 1:]:
            if not ln:
                continue
            if ln.startswith(("if", "elif", "else")):
                ok = False
                break
            if ln.startswith(("endc", "endr")):
                continue
            mm = re.match(r"^(db|dw|dba|dn|dbw|dwb)\s+(.*)$", ln)
            if not mm:
                continue
            kind, args = mm.group(1), mm.group(2)
            if kind == "dn":
                kinds.append(1)
                continue
            nargs = len([a for a in args.split(",") if a.strip()])
            kinds.extend([widths[kind]] * nargs)
        table[op] = (name, kinds if ok else None)
    _TABLE = table
    return table

_CHARMAP = None

def charmap():
    global _CHARMAP
    if _CHARMAP is not None:
        return _CHARMAP
    out = {}
    path = os.path.join(PC, "constants", "charmap.asm")
    for line in open(path, encoding="utf-8"):
        if line.strip().startswith("pushc"):
            break
        m = re.match(r'\s*charmap "(.*)",\s*\$([0-9A-Fa-f]+)', line)
        if m:
            out.setdefault(int(m.group(2), 16), m.group(1))
    _CHARMAP = out
    return out

TX_START, TX_FAR, TERM, DONE, PROMPT = 0x00, 0x16, 0x50, 0x57, 0x58

DSL_TOKENS = {
    "<LINE>": "\\n",
    "<NEXT>": "\\n",
    "<CONT>": "\\c",
    "<PARA>": "\\f",
    "<PLAYER>": "{PLAYER}",
    "<PLAY_G>": "{PLAYER}",
    "<RIVAL>": "{RIVAL}",
    "#": "#",
    "<POKE>": "#",
    "<BSP>": " ",
    "<PC>": "PC",
    "<TM>": "TM",
    "<TRAINER>": "TRAINER",
    "<ROCKET>": "ROCKET",

    "\u2026": "...",
    "<\u2026\u2026>": "......",
    "<NULL>": "",
}

NO_EQUIVALENT = {"<MOM>", "<RED>", "<GREEN>", "<ENEMY>", "<TARGET>", "<USER>",
                 "<PKMN>", "<SCROLL>", "<_CONT>", "<DEXEND>", "<LF>", "<WBR>"}

def read_text(rom, bank, addr, depth=0, budget=1200):
    out = []
    try:
        for _ in range(budget):
            b = rom.u8(bank, addr)
            addr += 1
            if b in (TERM, DONE, PROMPT):
                return out
            if b == TX_FAR:
                if depth >= 4:
                    return None
                lo, hi = rom.u8(bank, addr), rom.u8(bank, addr + 1)
                nxt = read_text(rom, rom.u8(bank, addr + 2), (hi << 8) | lo,
                                depth + 1, budget)
                return None if nxt is None else out + nxt
            out.append(b)
    except IndexError:
        return None
    return None

def is_text(raw):
    if raw is None or not raw:
        return False
    cm = charmap()
    for b in raw:
        t = cm.get(b)
        if t is None:
            return False
        if t in DSL_TOKENS or t in NO_EQUIVALENT:
            continue

        if all(0x20 <= ord(ch) <= 0x7E for ch in t):
            continue
        return False
    return True

def to_dsl(raw):
    cm = charmap()
    out, missing = [], set()
    for b in raw:
        t = cm.get(b, f"<${b:02X}>")
        if t in NO_EQUIVALENT:
            missing.add(t)
            continue
        if t in DSL_TOKENS:
            out.append(DSL_TOKENS[t])
            continue

        if t == '"':
            out.append('\\"')
        elif t == "\\":
            out.append("\\\\")
        else:
            out.append(t)
    return "".join(out), sorted(missing)

TEXT_OPS = {"writetext": "near", "jumptext": "near",
            "jumptextfaceplayer": "near",
            "farwritetext": "far", "farjumptext": "far"}

UNCONDITIONAL_JUMP = {"jump": "near", "sjump": "near", "farjump": "far",
                      "jumpstd": "std"}

TERMINAL = {"end", "endall", "endcallback", "jumptext", "jumptextfaceplayer",
            "farjumptext", "returnafterbattle"}

BRANCHY = {"iffalse", "iftrue", "ifequal", "ifnotequal", "ifgreater", "ifless",
           "checkevent", "checkflag", "checkcode", "checkitem", "random",
           "yesorno", "loadmenu", "verticalmenu", "scall",
           "farscall", "memcall", "priorityjump",
           "checkmorn", "checkday", "checknite", "readvar", "special",
           "checkcellnum", "checkphonecall", "checktime"}

def walk(rom, bank, addr, budget=400):
    table = command_table()
    cmds, texts = [], []
    seen = set()
    try:
        for _ in range(budget):
            if (bank, addr) in seen:
                break
            seen.add((bank, addr))
            op = rom.u8(bank, addr)
            name, kinds = table.get(op, (None, None))
            if name is None or kinds is None:
                return {"cmds": cmds, "texts": texts, "unknown": op}
            cmds.append(name)
            p = addr + 1
            if name in TEXT_OPS:
                if TEXT_OPS[name] == "near":
                    texts.append((bank, rom.u16(bank, p)))
                else:

                    texts.append(rom.dba(bank, p))
            if name in UNCONDITIONAL_JUMP:
                kind = UNCONDITIONAL_JUMP[name]
                lo, hi = rom.u8(bank, p), rom.u8(bank, p + 1)
                if kind == "std":

                    sb, sa = rom.sym["StdScripts"]
                    idx = (hi << 8) | lo
                    bank, addr = rom.dba(sb, sa + idx * 3)
                elif kind == "far":
                    bank, addr = rom.dba(bank, p)
                else:
                    addr = (hi << 8) | lo
                continue
            addr = p + sum(kinds)
            if name in TERMINAL:
                break
    except (IndexError, KeyError):
        return {"cmds": cmds, "texts": texts, "unknown": -1}
    return {"cmds": cmds, "texts": texts, "unknown": None}

MAX_DSL_TEXT = 511

def _decode_at(rom, bank, addr):
    raw = read_text(rom, bank, addr)
    if not is_text(raw):
        return None, "did not decode as text"
    dsl, missing = to_dsl(raw)
    if missing:
        return None, f"uses {', '.join(missing)}, which the DSL cannot say"
    if len(dsl) > MAX_DSL_TEXT:
        return None, f"too long ({len(dsl)} chars, DSL limit {MAX_DSL_TEXT})"
    return dsl, ""

def npc_dialogue(rom, script_bank, script_addr):

    obj_event = rom.sym.get("ObjectEvent")

    if obj_event is not None and script_addr == obj_event[1]:
        return {"text": None, "shape": "ObjectEvent", "missing": [],
                "why": "points at the shared ObjectEvent handler, whose text "
                       "is the engine placeholder \"Object event\""}
    w = walk(rom, script_bank, script_addr)
    shape = " ".join(w["cmds"])
    if w["unknown"] is not None:
        table = command_table()
        nm = table.get(w["unknown"], ("?", None))[0] if w["unknown"] >= 0 else "ran off the ROM"
        return {"text": None, "shape": shape, "missing": [],
                "why": f"walk stopped at ${max(w['unknown'], 0):02X} ({nm})"}
    branchy = [c for c in w["cmds"] if c in BRANCHY]
    if branchy:
        return {"text": None, "shape": shape, "missing": [],
                "why": f"conditional ({', '.join(sorted(set(branchy)))})"}
    if len(w["texts"]) != 1:
        return {"text": None, "shape": shape, "missing": [],
                "why": f"{len(w['texts'])} texts, not 1"}
    text, why = _decode_at(rom, *w["texts"][0])
    return {"text": text, "shape": shape, "missing": [], "why": why}

def trainer_dialogue(rom, script_bank, struct_addr):
    out = {"seen": None, "beaten": None, "after": None, "why": {}}
    try:
        seen_ptr = rom.u16(script_bank, struct_addr + 4)
        beat_ptr = rom.u16(script_bank, struct_addr + 6)
        after_ptr = rom.u16(script_bank, struct_addr + 10)
    except IndexError:
        out["why"]["struct"] = "struct address is outside the ROM"
        return out

    for slot, ptr in (("seen", seen_ptr), ("beaten", beat_ptr)):
        if ptr < 0x4000:
            out["why"][slot] = f"pointer ${ptr:04X} is not in a banked region"
            continue
        text, why = _decode_at(rom, script_bank, ptr)
        if text is None:
            out["why"][slot] = why
        else:
            out[slot] = text

    if after_ptr >= 0x4000:
        w = walk(rom, script_bank, after_ptr)

        first = None
        for i, c in enumerate(w["cmds"]):
            if c in BRANCHY:
                break
            if c in TEXT_OPS:
                first = w["texts"][0]
                break
        if first is None:
            out["why"]["after"] = "after-script branches before its first text"
        else:
            text, why = _decode_at(rom, *first)
            if text is None:
                out["why"]["after"] = why
            else:
                out["after"] = text
    else:
        out["why"]["after"] = "no after-battle script"
    return out

def _report():
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import crystal_maps as M
    from crystal_rom import Rom

    rom = Rom(os.path.join(PC, "pokecrystal.gbc"),
              os.path.join(PC, "pokecrystal.sym"))
    table = M.build_map_table(rom)
    consts = M.const_table(os.path.join(PC, "constants",
                                        "script_constants.asm"))
    ot_script, ot_trainer = consts["OBJECTTYPE_SCRIPT"], consts["OBJECTTYPE_TRAINER"]

    blocks = os.path.join(ROOT, "mod_runtime", "generatedmaps", "johto", "blocks")
    names = sorted({os.path.splitext(f)[0] for f in os.listdir(blocks)
                    if f.endswith(".block")} & set(table[1]))

    got = collections.Counter()
    why = collections.Counter()
    lens = []
    samples = []
    for name in names:
        g, m = table[1][name]
        hdr = M.read_map_header(rom, g, m)
        attr = M.read_attributes(rom, *hdr["attributes"], table[0])
        sbank = attr["scripts"][0]
        ev = M.read_events(rom, *attr["events"])
        for o in ev["object_events"]:
            if o["type"] == ot_script:
                r = npc_dialogue(rom, sbank, o["script"])
                if r["text"]:
                    got["npc"] += 1
                    lens.append(len(r["text"]))
                    if len(samples) < 3:
                        samples.append((name, r["text"]))
                else:
                    why[r["why"].split("(")[0].strip()] += 1
            elif o["type"] == ot_trainer and o["script"] >= 0x4000:
                r = trainer_dialogue(rom, sbank, o["script"])
                for slot in ("seen", "beaten", "after"):
                    if r[slot]:
                        got[f"trainer {slot}"] += 1
                        lens.append(len(r[slot]))
                    else:
                        why[r["why"].get(slot, "?")] += 1

    print(f"maps: {len(names)}")
    print("\nstrings extracted:")
    for k, v in sorted(got.items()):
        print(f"  {v:5d}  {k}")
    print(f"  {sum(got.values()):5d}  TOTAL")
    print("\nnot extracted, by reason:")
    for k, v in why.most_common(15):
        print(f"  {v:5d}  {k}")
    if lens:
        over = sum(1 for n in lens if n >= 512)
        print(f"\nlength: max {max(lens)}, mean {sum(lens)//len(lens)}, "
              f"{over} at/over PKS_MAX_TEXT (512)")
    print("\nsamples:")
    for name, t in samples:
        print(f"  [{name}] {t!r}")

def verify_texts(limit=0):
    from crystal_rom import Rom

    rom = Rom(os.path.join(PC, "pokecrystal.gbc"),
              os.path.join(PC, "pokecrystal.sym"))
    simple = {"text": "", "line": "<LINE>", "next": "<NEXT>",
              "para": "<PARA>", "cont": "<CONT>"}
    blocks = {}
    roots = [os.path.join(PC, "data", "text"), os.path.join(PC, "maps"),
             os.path.join(PC, "engine")]
    for root in roots:
        for dirpath, _d, files in os.walk(root):
            for fn in sorted(files):
                if not fn.endswith(".asm"):
                    continue
                label, toks, ok = None, [], True
                for line in open(os.path.join(dirpath, fn), encoding="utf-8"):
                    line = line.rstrip("\n")
                    m = re.match(r"^(\w+)::?$", line)
                    if m:
                        if label and ok and toks:
                            blocks.setdefault(label, toks)
                        label, toks, ok = m.group(1), [], True
                        continue
                    if label is None:
                        continue
                    s = line.split(";")[0].strip()
                    if not s:
                        continue
                    mm = re.match(r'^(\w+)\s+"(.*)"$', s)
                    if mm and mm.group(1) in simple:
                        if simple[mm.group(1)]:
                            toks.append(simple[mm.group(1)])
                        toks.append(mm.group(2))
                        continue
                    if s in ("done", "prompt"):
                        if label and ok and toks:
                            blocks.setdefault(label, toks)
                        label = None
                        continue

                    ok = False
    checked = passed = skipped = 0
    fails = []
    for label, toks in sorted(blocks.items()):
        if label not in rom.sym:
            continue
        raw = read_text(rom, *rom.sym[label])
        if not is_text(raw):
            continue
        got, missing = to_dsl(raw)
        if missing:
            continue
        want, m2 = to_dsl_from_tokens(toks)
        if m2:
            skipped += 1
            continue
        checked += 1
        if got == want:
            passed += 1
        elif len(fails) < 10:
            fails.append((label, want, got))
        if limit and checked >= limit:
            break
    print(f"verify: {passed} of {checked} text symbols match the disassembly "
          f"({skipped} skipped as not comparable)")
    for label, want, got in fails:
        print(f"  !! {label}\n     asm: {want!r}\n     rom: {got!r}")
    return not fails

def to_dsl_from_tokens(toks):
    out, missing = [], set()
    for t in toks:
        if t in DSL_TOKENS:
            out.append(DSL_TOKENS[t])
            continue

        if "{" in t and "}" in t:
            missing.add("rgbasm interpolation")
            continue

        i = 0
        while i < len(t):
            if t[i] == "<":
                j = t.find(">", i)
                if j > 0:
                    tok = t[i:j + 1]
                    if tok in DSL_TOKENS:
                        out.append(DSL_TOKENS[tok])
                    elif tok in NO_EQUIVALENT:
                        missing.add(tok)
                    else:
                        missing.add(tok)
                    i = j + 1
                    continue
            ch = t[i]
            if ch == "#":
                out.append("#")
            elif ch == '"':
                out.append('\\"')
            elif ch == "\\":
                out.append("\\\\")
            elif ord(ch) > 0x7E:

                out.append(DSL_TOKENS.get(ch, ch))
                if ch not in DSL_TOKENS:
                    missing.add(ch)
            else:
                out.append(ch)
            i += 1
    return "".join(out), sorted(missing)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", action="store_true",
                    help="coverage across every imported map")
    ap.add_argument("--verify", action="store_true",
                    help="compare every decoded text against the disassembly")
    args = ap.parse_args()
    if args.verify:
        sys.exit(0 if verify_texts() else 1)
    if args.report:
        _report()
    else:
        t = command_table()
        print(f"")
        print(f"  with a known operand length   : "
              f"{sum(1 for _n, k in t.values() if k is not None)}")
        print(f"charmap entries                 : {len(charmap())}")
        print("run with --report for dialogue coverage")
