# gen1color, the battle presentation layer

Full reference: **`docs/battle/gen1color-and-rom-assets.md`**
Working guide: the **`gen1color-presentation`** skill.

This file is the quarantine contract and the current state. It used to say "No
code yet"; that was true when the module was scoped and is kept in mind as a
warning about stale READMEs, not repeated.

## What this is

The same Gen 1 battle engine wearing different **presentation**: an EXP bar, a
different HUD layout, Gen 2 pics, font and text box. Hence `gen1color`.

This was settled by a ROM-vs-ROM diff rather than assumed, identical WRAM
layout, identical RNG sequence, identical RNG call sites versus `pokered-master`.

**Consequence: do not re-derive battle logic here.** The engine is already
verified against the ROM. This is the presentation on top of it.

## Quarantine, as it actually stands

The original rule was "nothing here may be `#include`d by, or may modify,
`src/game/battle/*`". The first half still holds absolutely:

- **Nothing in `src/game/battle/` includes anything from this directory.**
  battle_ui does not know gen1color exists.
- **This module draws into the tilemap and tile slots only.** Turning it off
  restores the original battle UI exactly, and `color off` is the escape hatch
  that does so.

The second half was qualified by the user, and the current rule is:

> Additive or hook-based only. Never replace an architecture, either diversify
> routing so it knows what to choose based on presentation mode, or add a
> separate mode.

What that permitted, and all it has permitted: **four write-only statics and five
accessors** in `battle_ui.c`, so this module can observe state instead of
guessing at it. Nothing inside battle_ui reads any of them. Every assignment is a
one-liner beside code that was already there.

`BattleUI_EnemyPicKind` · `BattleUI_EnemyMonOnField` · `BattleUI_HpBarAnim` ·
`BattleUI_HudOverlayActive` · `BattleUI_PoofRomTile`

Adding a whole extra art set once needed **none** of these, it was a new branch
in `g1c_load_mon_pics` plus one additive function in `gbc_color.c`
(`GbcColor_SetBattleSuperPalettes`, NULL-defaulted so every existing caller is
byte-for-byte unaffected). That is the shape to aim for.

Adding a *sixth* accessor is a decision to make deliberately, not a habit. The
last attempt to add a behavioural hook (`BattleUI_RedrawHuds`, to force a
repaint) was the wrong shape and was reverted, see "no half-ownership" in the
docs.

## Files

| File | Role |
|---|---|
| `gen1color_battle.c/h` | per-frame tick: tile loads, mon pics, palettes, EXP bar, mode switches |
| `gen1color_scene.c/h` | composes the battle region, `G1CScene_Draw` (Gen 2) and `G1CScene_DrawGen1` (Gen 1) |
| `crystal_pic_anim.c/h` | Crystal front-pic animation player (`PokeAnim_DoAnimScript`) |

## Modes

```
color mono on|off              colour layer  (default: colour)
ui gen1|gen2                   which HUD     (default: gen1)
sprite gen1|crystal            which art     (default: gen1)
palette enhanced|yellow        which mon colours, GEN 1 art only (default: enhanced)
```

cries gen1|crystal (audio, KANTO mon only, default gen1) is the same family but
not a presentation setting, so it has no row on the options screen below.

The four presentation rows above are also on an in-game options screen: **ESC**
(`src/game/presentation_menu.c`; SHIFT+ESC quits, which is what ESC used to do).
Its COLOR row is the colour *layer*, not `color on|off`, see the docs for why
the master switch is the wrong thing to put on a settings page.

All switchable mid-battle. `color off` is distinct from `mono on`: it disables
the overlay entirely and returns the original game rather than this module
reproducing it.

### Two art styles were removed

A Gold/Silver set and a Space World 1997 prototype set once sat beside these,
both sourced from third-party romhacks. They are gone. A package asks its user
to supply the ROM it names, which is only a reasonable thing to ask when that
ROM is a game they can legally own a copy of.

Nothing replaced them, because nothing needed to: both were alternative **art**
and the port ships its own. The Gen 2 HUD, font and EXP bar, which were also
coming from a romhack, now come from Crystal, the game they are actually from
(`tools/extract_crystal_battle_gfx.py`).

## Two rules for assets

**1. Derive from the ROM, not from ASM text.** The built ROM plus its `.sym` is
the source of truth. Reading ASM means reimplementing macro expansion and
conditional assembly by hand, and getting it subtly wrong is invisible until it
renders.

**2. Assets are extracted, never committed**, and never taken from a
disassembly's own asset files. Ship-of-Harkinian model. Output goes to
`generated/` (gitignored); the reference repos are gitignored too.

```bash
cd pokecrystal-master && make    # f4cd194b…  sha1-checked   (rgbds 1.0.1)
cd pokered-master     && make    # ea9bcae6…                 (rgbds 1.0.1)
pwsh tools/py.ps1 tools/extract_crystal_mon_pics.py --rom pokecrystal-master/pokecrystal.gbc --verify
pwsh tools/py.ps1 tools/extract_crystal_battle_gfx.py --rom pokecrystal-master/pokecrystal.gbc
```

## Status

Live and in use. Gen 2 and Gen 1 HUDs; Crystal + Gen 1 art; Crystal pic
animations; colour and DMG. Remaining known gaps are listed at the end of the
docs page.
