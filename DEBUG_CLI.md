# Debug CLI

OldAmber includes a command console for testing saves, story states, maps,
battles and AmberScript scenes without replaying the entire route to them.
These commands are powerful: several of them directly change the running game.
Make a save state first if you care about the current state.

The CLI is separate from the launcher's **Enable debug tooling** checkbox. That
checkbox exposes the controller-friendly debug pages in the suspend menu. The
CLI remains available whether the checkbox is on or off.

## Opening the console

While the game is running, press the backtick/tilde key (`` ` `` / `~`). Type
one command, then press Enter. Escape closes the console without running it.
The console accepts up to 64 characters per command.

For a larger console with command history, launch the executable with
`--debug-render`. In that layout the console stays open; press `` ` `` to enter
or leave typing mode, and Enter to submit.

When running from a source checkout, `tools/game_cli.py` provides an external
terminal interface to the same commands:

```sh
python tools/game_cli.py
python tools/game_cli.py "teleport pewter"
python tools/game_cli.py --script path/to/commands.txt
```

Run it from the repository root while the game is open. The downloadable game
bundles do not include the loose Python development tools, so use the in-game
console there.

Arguments shown in square brackets are optional. Most names are
case-insensitive; use underscores instead of spaces where a command takes a
single name. Numeric IDs accept decimal, and many commands also accept `0x`
hexadecimal notation.

## Movement and basic input

| Command | Effect |
|---|---|
| `up [tiles]` | Walk up; defaults to one tile. |
| `down [tiles]` | Walk down. |
| `left [tiles]` | Walk left. |
| `right [tiles]` | Walk right. |
| `a [presses]` or `interact [presses]` | Press A; useful for dialogue and confirmation. |
| `b` or `back` | Press B. |
| `start` or `menu` | Press Start. |
| `select` | Press Select. |
| `wait [frames]` | Send no input; defaults to 60 simulation frames. |
| `state` | Refresh the external CLI state report without acting. |
| `options [on|off]` | Open, close or toggle the suspend menu. `presmenu` is an alias. |

While dialogue or a yes/no prompt is open, normal gameplay commands are
locked. Use `a` or `b` to advance or close it before sending another command.

## Getting around the world

| Command | Effect |
|---|---|
| `teleport <place>` | Warp to a built-in named location. `tele` is an alias. |
| `teleport <map_id> [x y]` | Warp to a numeric map and coordinates. |
| `map_warp <vmap_name> [x y]` | Warp to an AmberScript virtual map by name. |
| `noclip [on|off]` | Toggle or explicitly set collision-free movement. |
| `unstuck` | Move to a safe nearby tile, preferring the nearest warp when necessary. |
| `nowilds on|off` | Suppress or restore random wild encounters. |
| `sprint_holdb on|off|status` | Make holding B enable or disable sprinting. |
| `mapname` | Print the current real map ID and virtual-map name, when applicable. |
| `tile_info` | Report the player position, facing tile and collision details. `tileinfo` is an alias. |
| `tile_prop_at <x> <y>` | Report the tile properties at map coordinates. `tileprop` is an alias. |
| `passable_at <x> <y>` | Trace why a map cell is or is not passable. |
| `collision on|off` | Show or hide the collision overlay. |
| `gridoverlay on|off` | Show or hide block IDs on the map. |
| `screenshot [path.bmp]` | Save the current framebuffer; defaults to `bugs/shot.bmp`. `shot` is an alias. |

Named `teleport` destinations are:

```text
pallet              viridian            pewter
cerulean            vermilion           lavender
celadon             fuchsia             cinnabar
indigo              saffron             route_1 through route_12
route_24            route_25            route_4_fly
route_10_fly        oaks_lab            viridian_forest
mt_moon             rock_tunnel         pokemon_tower
silph_co            safari_zone         viridian_gym
pewter_gym          cerulean_gym        vermilion_gym
celadon_gym         fuchsia_gym         saffron_gym
cinnabar_gym
```

Town and city names also accept their full form, such as `pewter_city`.

## Inventory, party and Pokédex

Party slots are numbered 1 through 6. Move slots are numbered 1 through 4.

| Command | Effect |
|---|---|
| `giveitem <name_or_id> [quantity]` | Add an item. Examples: `giveitem potion 5`, `giveitem silph_scope`. |
| `givetm <1-50>` | Add one TM. |
| `givehm <1-5>` | Add one HM. |
| `listbag` | Print the current bag contents. |
| `bagqty <index> <quantity>` | Change a zero-based bag entry's quantity; zero removes it. |
| `bagremove <index>` | Remove a zero-based bag entry. |
| `givemon <dex_number> [level]` | Add a Generation I Pokémon by Pokédex number; defaults to level 20. |
| `givemonx <species_name_or_id> [level]` | Add a Pokémon by species name or internal ID. |
| `giveteam` | Replace the party with six level-100 testing Pokémon. |
| `setlevel <party_slot> <level>` | Change a party member's level and recalculate its stats. |
| `sethealth <party_slot> <hp>` | Change current HP, clamped to the member's maximum. |
| `poison [party_slot]` | Poison a party member; defaults to slot 1. |
| `healparty` | Restore all party HP and clear status conditions. |
| `teach <party_slot> <move_name>` | Teach a move by name, using an empty slot or replacing slot 4. |
| `teachmove <party_slot> <move_id>` | Teach a move by numeric ID. |
| `setmove <party_slot> <move_slot> <move>` | Replace one specific move slot and restore its PP. |
| `addexp <party_slot> <amount>` | Add experience and process level-ups; does not trigger evolution. |
| `exprate <percent>` | Set battle EXP gain; `100` is normal and `200` is double. |
| `pc_send <party_slot>` | Deposit a party member into the current PC box. |
| `boxdump` | Write the current box contents to `bugs/box.json`. |
| `boxswitch <box_index>` | Select a zero-based PC box and dump its contents. |
| `boxwithdraw <slot_index>` | Move a zero-based box slot into the party. |
| `boxrelease <slot_index>` | Permanently remove a zero-based box slot. |
| `dex_fill [seen|owned|all]` | Fill one or both Pokédex tables; defaults to both. |
| `setmoney <0-999999>` | Set carried money. |
| `setcoins <0-9999>` | Set the Coin Case balance. |

`givemon` and `givemonx` fill the next free party slot. If the party is already
full, they replace slot 6. `boxrelease` is destructive and has no confirmation.

Convenience teams for animation testing are available as `movetestteam1` and
`movetestteam2`. `bulba15`, `squirtle15` and `magneton50` create fixed test
Pokémon and movesets.

## Events, badges and story setup

| Command | Effect |
|---|---|
| `setevent <EVENT_NAME_or_number>` | Set an event using a readable `EVENT_*` name or number. |
| `clearevent <EVENT_NAME_or_number>` | Clear a named or numbered event. |
| `setflag <number>` | Set an event by numeric ID. |
| `clearflag <number>` | Clear an event by numeric ID. |
| `flagdump` | Write all currently set flags and their names to `bugs/flags.json`. |
| `givebadge <number>` | Grant a badge. Accepts 0-7 or 1-8 numbering. |
| `removebadge <number>` | Remove a badge. Accepts 0-7 or 1-8 numbering. |
| `clearguardchecks` | Clear the remembered Route 23 badge-check results. |
| `resetoak` | Reset the Oak intro, starter, parcel and early rival sequence. |
| `skipoak` | Mark the Oak intro through parcel pickup complete, leaving parcel delivery ready. |
| `trainer_reset` | Clear the defeated flags for trainers on the current map. |
| `gym_reset <leader|all>` | Reset a gym's leader and trainer state. |
| `gym_badges_clear [keep]` | Clear gym completion flags and optionally retain the first `keep` badges. |
| `e4_reset` | Reset the Elite Four sequence. `elite4_reset` and `elitefour_reset` are aliases. |
| `seafoam_reset` | Reset Seafoam Islands boulders and currents. |
| `victoryroad_reset` | Reset Victory Road boulders. `vr_reset` is an alias. |
| `safari_state [steps]` | Report Safari state, or set remaining steps from 0 through 502. |
| `eventdiff snapshot` | Remember selected story state for comparison. |
| `eventdiff show` | Show what changed since the snapshot. |

Prefer `setevent` and `clearevent` when a readable event name exists. The
numeric forms are useful for unnamed or newly added flags but are easier to
misapply.

### Story checkpoints

`checkpoint <name>` applies a coherent test setup, including the needed flags,
party state and location. It intentionally interrupts an active battle or
scene. Use `checkpoint list` for the current list or `checkpoint verify <name>`
to preview its changes and restore the original state afterward.

Current checkpoints include:

```text
parcel_ready          pokedex_ready         route22
brock                 mt_moon               cerulean
misty                 cerulean_rocket       ss_anne_hm
surge                 erika                 koga
sabrina               blaine                brock_post
misty_post            erika_post            koga_post
sabrina_post          blaine_post           post_giovanni_victory
route23_guard_reset   gym_badges1           gym_badges2
gym_badges3           gym_badges4           gym_badges5
liftkey_reset         giovanni_reset        giovanni_ready
silph_ready           silph_entry_locked    silph_entry_unlocked
silph_rival_ready     silph_giovanni_ready  silph_lapras_ready
```

`badge_guard_reset` is an alias for `route23_guard_reset`.

## Battles

| Command | Effect |
|---|---|
| `wild <dex_number> [level]` | Start a catchable wild battle; defaults to level 5. |
| `fight [move_slot]` | Choose FIGHT, optionally selecting move 1 through 4. |
| `pkmn` or `pokemon` | Choose PKMN from the battle menu. |
| `bag` or `item` | Choose ITEM from the battle menu. |
| `run` | Choose RUN from the battle menu. |
| `autowin on|off|status` | Make the first player-selected move automatically win each battle. |
| `battle_seed <0-255>` | Set deterministic battle randomness. |
| `rng_state` | Print the current battle random state. |
| `animlab start [level]` | Start automatic move-animation testing. |
| `animlab stop` | Stop the animation loop without ending the current battle. |
| `animlab status` | Report animation-lab state. |
| `hittrace on|off|reset|status` | Control move-hit diagnostic tracing. |

## Saving, replay and time control

Debug changes affect the running process immediately, but they do not become
normal saved progress until the game is saved.

| Command | Effect |
|---|---|
| `savemenu` | Drive the real Start → Save → Yes flow; refuses places where the player cannot normally save. |
| `gamesave` or `savegame` | Write progress directly, bypassing the normal menu restrictions. |
| `quicksave [name]` | Create a named debug save state; defaults to `1`. |
| `quickload [name]` | Load a named debug save state. |
| `csave create <name>` | Create a named debug save state. `csave save` is equivalent. |
| `csave load <name>` | Load a named debug save state. |
| `replay record <name>` | Save the starting state and begin recording per-frame input. |
| `replay stop` | Stop recording or playback. |
| `replay play <name>` | Load and play a recording. |
| `replay status` | Report replay state. |
| `pause` / `resume` | Freeze or resume simulation. |
| `step [frames]` | Advance a paused game by one or more frames. |
| `speed <percent>` | Set simulation speed; `100` is normal and `0` is unlimited. |
| `turbo on|off` | Switch between unlimited and normal simulation speed. |
| `rewind <frames>` | Move backward through the 30-second rewind ring; a negative value moves forward. |
| `bp_commit` | Persist the current rewind window as an automatically named breakpoint. |
| `bp_restore <name>` | Restore a persisted breakpoint and pause at its oldest frame. |

Save states and replays are debugging aids, not substitutes for the ordinary
`.sav` progression file. Loading either rewinds the whole captured runtime
state.

## AmberScript and map authoring

These commands are most useful when developing scenes and maps. Paths are
resolved from the game working directory.

| Command | Effect |
|---|---|
| `amberscript on|off|status` | Enable, disable or report AmberScript dispatch. |
| `scene_run <name>` | Run `mod_runtime/scenes/<name>.scene`. |
| `scene_stop` | Stop the active debug scene. |
| `script_trace on|off|status` | Control scene tracing to the console. |
| `script_trace file_on|file_off` | Control persistent scene trace output. |
| `scenedump [path]` | Disassemble the active scene; defaults to `bugs/scene_disasm.txt`. |
| `npcdump` | Dump current NPC and scene-binding information. |
| `npcshow <x> <y>` | Persistently show the NPC declared at those coordinates after re-entering the map. |
| `npchide <x> <y>` | Persistently hide the declared NPC after re-entering the map. |
| `map_list` | List registered virtual maps. |
| `map_save <name>` | Save the current map as a named virtual map. |
| `map_release <name>` | Release a named virtual map's live slot. |
| `map_export <name>` | Export the current map for AmberScript editing. |
| `block_def_load <path.block>` | Load block definitions from a `.block` file. |
| `map_edits_apply` | Reapply loaded map edits. |

The runtime also exposes low-level `tile_*`, `subtile_*`, `tileset_*`,
`block_*`, `scene_npc`, `scene_trigger`, `map_connect`, `dsl_bank` and NPC
puppet commands. They directly mutate authoring state and are easier to use
incorrectly than the commands above. Their accepted syntax is printed when a
command is submitted without its required arguments; the corresponding
implementations live in the `amberscript_*` modules under `src/game/`.

## Reports and diagnostics

| Command | Effect |
|---|---|
| `capture [message]` | Create a timestamped diagnostic report. |
| `probe player|npcs|hash|status` | Write a focused machine-readable runtime probe. |
| `mapdump` | Write the current map grid. |
| `statedump [path]` | Write a raw runtime-state snapshot; defaults to `bugs/state.bin`. |
| `suite` | Report debug-suite status. |
| `trace on|off <channels>` | Toggle `actor`, `ui`, `npc`, `warp` or `all` trace streams; channels may be comma-separated. |
| `hashtrace on|off` | Toggle per-frame deterministic state hashing. |
| `watch add flag <id>` | Pause and capture when an event becomes set. |
| `watch add flagclear <id>` | Pause and capture when an event becomes clear. |
| `watch add tile <map> <x> <y>` | Pause and capture on arrival at a position. |
| `watch add map <map>` | Pause and capture on entering a map. |
| `watch add hp <amount>` | Pause and capture when any party member reaches that HP or lower. |
| `watch add battle` | Pause and capture when a battle begins. |
| `watch remove <index>` | Remove one watchpoint. |
| `watch clear` | Remove all watchpoints. |

Most generated diagnostics are placed in the game's `bugs/` directory. In a
source build that is normally inside the active build directory. The external
Python client prints `cli_state.txt` after each command; machine-readable suite
responses are written to `suite_out.json`.

## A safe testing pattern

```text
quicksave before_test
checkpoint misty
giveitem super_potion 5
noclip on
```

Test the behavior, then return with:

```text
noclip off
quickload before_test
```

Use `savemenu` only when you deliberately want the changed state to become
normal saved progress.
