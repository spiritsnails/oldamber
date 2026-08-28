# tools/romimport, Johto maps, out of the ROM

The existing Johto importer (`tools/pokecrystal_import.py`) reads the pret
disassembly's own files: `maps/<Name>.blk`, `data/tilesets/*_metatiles.bin`,
`gfx/tilesets/*.png`. That is the provenance this project is moving away from.

This folder is the replacement path: **map data comes out of a ROM the user
built**, Ship-of-Harkinian style, the same rule the battle-asset extractors
already follow (`docs/battle/gen1color-and-rom-assets.md`).

```bash
cd pokecrystal-master && make          # f4cd194b…  sha1-checked, rgbds 1.0.1
python tools/romimport/dump_map.py NewBarkTown --verify
```

## What it reads, and from where

| | source |
|---|---|
| map data, events, tiles, collision | **`pokecrystal.gbc`** |
| addresses of the above | `pokecrystal.sym`, names and addresses, no content |
| struct layouts | the macros that define them, transcribed into comments |
| `--verify` oracle | the disassembly's own files, **read nowhere else** |

The separation is enforced by construction, not by discipline: `crystal_rom.py`
and `crystal_maps.py` open nothing but the `.gbc` and the `.sym`. Only
`dump_map.py`'s `--verify` half touches the disassembly, and everything it reads
there is compared against, never emitted.

Even the map *table* is ROM-derived. `build_map_table` walks
`MapGroupPointers`, follows each 9-byte header to its attributes pointer, and
matches that address back to a `<Name>_MapAttributes` symbol, so the
name ↔ (group, map) mapping comes from the ROM plus its own linker output, with
no constants file to drift from. That recovers **388 maps across 26 groups**.

## Files

| | |
|---|---|
| `crystal_rom.py` | banks, symbols, pointer forms, lz decompression |
| `crystal_maps.py` | the map structures, each transcribed from its macro |
| `dump_map.py` | read one map, print it, and `--verify` it |

## Verified

Eight maps across four tilesets, all six data classes byte-exact against the
disassembly:

```
NewBarkTown ElmsLab Route29 GoldenrodCity
SproutTower1F BurnedTower1F CherrygroveCity VioletCity
```

blockdata · metatiles · collision · warp events · bg events (signs) ·
object events (NPCs). New Bark Town reads back as 10×9 blocks, border `$05`,
WEST→Route29 / EAST→Route27, 4 warps, 4 signs, 3 NPCs, which is exactly what
`data/maps/attributes.asm` and `maps/NewBarkTown.asm` say.

## Three things that were nearly missed

Recorded because each one produces output that *looks* right.

**Metatile counts are not all 128.** Indoor tilesets have 64. Reading a fixed
128 walks into the collision table that follows, and an oracle comparison over
`min(len(a), len(b))` cannot see it, it only ever compares entries that exist
on both sides. `metatile_count()` derives the real count from the Meta→Coll
address gap.

**Several tileset labels alias one address.** `Tileset0` is a placeholder
pointing at `TilesetJohto`'s data, so the first matching symbol names the
tileset "0", which then finds no oracle file and *skips its own verification*.
Meta and Coll are tileset-specific (GFX is not, `TilesetBattleTowerOutsideGFX`
and `TilesetJohtoModernGFX` share an address), so intersect those two and
discard the numeric placeholder.

**`object_event` stores `y + 4, x + 4`.** The `+4` is in the macro, not in the
map, it is the 4-block border offset. Left in, every NPC lands four tiles down
and right of where the map says, which reads as a plausible map rather than a
bug.

## Stage 2, emission (`emit_map.py`)

```bash
python tools/romimport/emit_map.py NewBarkTown --verify
```

Writes to `generated/romimport/` (gitignored): one 8×8 PNG per tile the
tileset's metatiles use, a `.block` with the deduped quads, and the cell grid.

Geometry, measured against the existing vmap rather than assumed: a `subtile`
is one 8×8 tile, a block's `source quad` is 2×2 subtiles = a 16×16 cell, and a
Crystal metatile (4×4 tiles) is therefore **four** cells. `tilecoll TL, TR, BL,
BR` gives one collision id per quadrant, i.e. exactly one per emitted quad.

Verified against the old disassembly-sourced import (still on disk, untracked):

| | New Bark | Route 29 | Cherrygrove | Violet |
|---|---|---|---|---|
| tiles byte-identical | 152/152 | 152/152 | 152/152 | 152/152 |
| grid coords | match | match | match | match |
| cells identical | 350/360 | 991/1080 | 687/720 | 1349/1440 |

**The art is byte-exact everywhere, and every differing cell differs only in
PASSABILITY, zero art differences.** Connections came out identical too
(`connect NewBarkTown west Route29 118 0`), and NPC sprites resolve by name
(Teacher, Fisher, Rival).

### Settled: the passability disagreement, the ROM is right

Every differing cell was the ROM saying **not walkable** where the old import
said walkable. User verdict, by eye, on New Bark:

> the passability is exactly as i suspected, the small trees that some johto
> vmaps didnt naturally mark as collision, properly marked here.

So the ROM path fixes a real bug in the old import: Johto's small decorative
trees were walk-through. That is what reading the game's own
`CollisionPermissionTable` buys, `passable = permission is LAND_TILE or
WATER_TILE`, with no heuristic in between.

Consequence for `--verify`: **art and geometry are hard failures; passability
is informational.** The old import is an oracle only for what it can be trusted
on. Leaving passability as a failure would have meant a permanently red check,
which is worse than not checking, a red you learn to ignore.

Note the quad COUNTS differ legitimately (31 here vs 34 there on New Bark): the
two pipelines dedupe on different keys, this one folds passability in, the old
one splits a warp into its own block even when the art is identical. Comparing
partitions therefore proves nothing, which is why the check compares each
cell's four tile images and its walkability instead.

## Not done yet

- **Indoor tilesets do not emit.** `ElmsLab` stops with *"metatiles reference
  tile 192 but the tileset decompressed to only 192 tiles"*. Gen 2 tilesets
  are not one contiguous graphics block, ids past the compressed run come
  from a second source this reader does not follow yet. It fails loudly rather
  than emitting a map with garbage tiles.
- **NPCs and signs are emitted as COMMENTS**, not live `npc`/`sign` lines. The
  sprite names and coordinates are right; what is missing is the mapping from
  Crystal's movement/type constants to this port's DSL, and the scripts.
- **Palettes are half done.** The per-metatile-quadrant palette *map* is read;
  the actual RGB sets (time-of-day tileset palettes, roof palettes per map
  group) are not.
- **Sprite ids are raw numbers.** Turning `sprite 41` into a name needs the
  `OverworldSprites` table.
- **Event flags are read but unused**, which is the bounded scope asked for, NPCs come in unconditionally for now.

When emission lands, its output must go to `generated/` (gitignored), **not**
alongside the current importer's `mod_runtime/custom_art/pokecrystal/johto/*.png`, those are committed ROM-derived assets and are part of what this replaces.
