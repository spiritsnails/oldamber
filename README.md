# OldAmber

A native C port of the Poke RBY Generation 1 engine, playable from the title
screen to the Hall of Fame.

**[Download the latest release](https://github.com/spiritsnails/oldamber/releases/latest)**
&nbsp;&middot;&nbsp;
**[Join the Discord](https://discord.gg/2pZNRp6t8)**

Behaviour follows the 1996 release closely. Damage and catch rates, status
effects, move dispatch, trainer decisions and text timing reproduce what the
original does, including the arithmetic quirks and overflow bugs that shipped
with it.

## You supply your own legally acquired ROM

This repository holds the engine and the extractors. The graphics, music, text,
maps and stat tables come out of a cartridge dump you already own, read once on
your own machine the first time you run the game. Your ROM is opened read-only
and stays where it is.

Poke Red and Blue are both supported. The launcher lists whichever versions are
present and you pick one.

## How to play

Run the game. The launcher opens, asks for a ROM, builds the game data in a few
seconds, and hands you a PLAY button. That happens once, and afterwards the
launcher opens straight to PLAY.

The launcher also holds RE-IMPORT A ROM for rebuilding the data after a bad
extraction or an update, and SWITCH SAVE FILE for keeping several games without
overwriting anything. It is drivable end to end with a game controller.

Windows, macOS and Linux are supported. macOS builds are universal, covering
Apple Silicon and Intel. Linux ships as an AppImage that carries its own
libraries, and as a Flatpak.

## Building from source

Common to every platform: a C11 compiler, CMake 3.16 or newer, SDL2, Python 3.

### Linux

```sh
sudo apt install build-essential cmake ninja-build libsdl2-dev python3
bash tools/setup_red.sh /path/to/your/pokered.gbc
bash tools/launch_game.sh
```

`setup_red.sh` takes a fresh clone and a ROM to a playable build in one step: it
extracts the assets, configures the tree, compiles, and verifies its own output.

### Windows

Build under MSYS2, which supplies the MinGW toolchain and SDL2:

```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2
bash tools/setup_red.sh /path/to/your/pokered.gbc
```

CMake also accepts an MSVC build. It needs SDL2's Visual C++ development
libraries, which are not carried in this repository: download
`SDL2-devel-<version>-VC.zip` from
[the SDL releases](https://github.com/libsdl-org/SDL/releases) and unpack it
into `deps/SDL2-VC/`. Any version works. CMake prints where to get them if it
does not find them, and a vcpkg or system SDL2 on `CMAKE_PREFIX_PATH` does just
as well.

### macOS

Release builds are universal, so they need a universal SDL2, and Homebrew
installs for the host architecture only. `setup_macos.sh` builds one into
`deps/` and configures against it:

```sh
bash tools/setup_macos.sh
```

### Rebuilding, and packaging

```sh
BUILD_DIR=build-red bash tools/build.sh
```

Release bundles come from `tools/dist/`: `make_release.sh` for Windows,
`make_release_linux.sh`, `make_release_macos.sh`, then `make_appimage.sh` and
`make_flatpak.sh` for the two Linux formats. Each produces an archive with a
checksum, checks its own output, and refuses to package a debug build.

## Architecture

```
src/game/         engine: overworld, battle, menus, scripting
src/platform/     SDL2 layer: display, audio, input, save, launcher
src/data/         table declarations, bound from the asset package at boot
tools/assetpack/  builds assets.pak and packages/<id>/ from a ROM
tools/romimport/  emits map geometry, tile art and the dialogue table
mod_runtime/      scenes, map properties, configuration
```

**Nothing ROM-derived is compiled in.** `src/data/` declares the shape of each
table; the bytes arrive at boot from `packages/<id>/`, produced on the player's
machine. A build with no imported ROM links, runs, and shows the launcher.

**Every map is virtual.** Maps stream through eight rotating slots, so the
current map id is a slot number and maps are referenced by name. That is what
lets the importer supply the geometry at runtime.

**Content lives in text, not in C.** The `.scene` files and map properties
under `mod_runtime/` carry dialogue, NPC behaviour, triggers and map rules, and
are read at startup. `src/game/` holds the engine that runs them.

**The importer runs out of process.** Three backends implement one interface:
embedded CPython for a development build, the extractor scripts through the
system python3, and a frozen `setup` binary in a release. The launcher drives
whichever is available, so the ROM picker and the progress screen are the same
everywhere.

## Modifying the game

Scenes are plain text, read at startup, so changing one needs no rebuild.

A scene is a small script. This is the Bike Shop clerk, shortened:

```
if EVENT_GOT_BICYCLE
    say rom:BikeShopClerkHowDoYouLikeYourBicycleText
    wait_text
    stop
end
if has_item(BIKE_VOUCHER)
    take_item BIKE_VOUCHER
    give_item BICYCLE
    set_event EVENT_GOT_BICYCLE
    sfx get_key_item
    wait_sfx
    stop
end
```

`say rom:<Label>` pulls the original line out of the text table the importer
built from your ROM, so dialogue stays where it came from. Plain strings work
for text you write yourself:

```
say "Hello! I am a\nnew character.\fNice to meet you!@"
```

Text ends at `@`, the terminator the original engine uses. The box is 18
columns by 2 lines: `\n` starts the second line, `\f` clears the box for a new
page, and `\c` waits for A then scrolls so the next line continues underneath.
Anything you leave unmarked is word-wrapped to fit, and a word longer than the
box is broken across lines, so short strings need no formatting at all.

`ask "...@"` puts a yes or no prompt up instead and branches on the answer.
Both accept the same escapes, and both take a `rom:` label in place of a
string.

Write your own lines here. Text copied out of the ROM belongs behind a `rom:`
label, where the importer supplies it from the player's own cartridge.

Scenes attach to map cells through `mod_runtime/scenes/bindings.txt`:

```
scene_npc  BikeShop    kanto/bikeshop/clerk      6 2
scene_tile BillsHouse  kanto/billshouse/bill_pc  1 4
```

`scene_npc` fires when the player talks to whoever stands there, `scene_tile`
when they interact with a tile, and `scene_trigger` when they step onto one.

Map properties sit in `mod_runtime/blocks/vmap_<Map>_properties.block` and
describe what the geometry alone does not:

```
block kanto_qac4f70fa
    surfable yes
end
```

Between them you can rewrite an NPC's dialogue, move a scene to another tile,
put a trigger on an empty cell, mark water as surfable, or add a character who
was never there, by editing text and restarting. The scene language covers
events, items, battles, movement and music, so most of what a map does is
reachable without touching C.

## Controls

| Key | Action |
|---|---|
| Arrow keys | Move, navigate menus |
| Z | A |
| X | B |
| Right Shift | Select |
| Enter | Start |
| Escape | Options and save states |
| Shift + Escape | Quit |

### Gamepad

Pads work the moment they are connected, no configuration. These are the
defaults, and they are what a Steam Deck picks up on its own.

| Button | Action |
|---|---|
| D-pad, or left stick | Move, navigate menus |
| A | A |
| B | B |
| Back / View | Select |
| Start / Menu | Start |
| L1 + R1 together | Options and save states |
| Back + Start, held two seconds | Restore default bindings |

A and B follow the Game Boy's physical layout rather than the printed letters:
A is the bottom face button, where your thumb rests, and B sits to its right,
exactly as on the handheld. On a Steam Deck or an Xbox pad the labels agree
with that. On a Nintendo-style pad the button printed B is the bottom one, so
it acts as A.

The launcher takes the same pad: the d-pad or stick moves, A confirms, B goes
back, L1 and R1 page through longer lists.

Options is L1 + R1 rather than a single button because the game itself needs
Start and Select, and a chord cannot be hit by accident while walking. The
reset is a two second hold for the same reason: a rescue that fires on a single
press is a rescue that fires by accident.

Every button is rebindable from the CONTROLS page of the options screen, with
keyboard and controller mapped independently, so remapping the pad leaves the
keyboard alone. Bindings are swapped rather than duplicated, so no two actions
can end up on one button and nothing gets stranded. The reset hold is there if
a remap leaves you unable to reach the menu.

## Presentation

The game can be played in a myriad of different presentation styles. You can
run it in a monochrome palette at 10:9 with ghosting and an LCD simulator on
top, or you can run it in 16:9 with no filter and no ghosting, for a clean
modern style. You can basically mix and match any presentation style, and come
up with new ones that did not really exist, like a 16:9 CRT TV, or the SGB
border with monochromatic palettes.

- **Widescreen**, an optional 16:9 frame showing more of the map either side at
  the same scale, with the player centred.
- **Shaders**, SameBoy's LCD filters, an emulated composite signal, and a CRT
  mode with curvature, beam and phosphor simulation, on an OpenGL renderer. An
  SDL renderer covers machines without it, switchable while running.
- **Super Game Boy border**, read from the cartridge like everything else.
- **Save states**, six slots with thumbnails, alongside the in-game save.
- Speed control, palette and colour-curve options, and a Game Boy Color style
  colour layer that can be switched off for the original monochrome.

## Where your data lives

Saves, the imported game data and the log sit beside the game for an ordinary
folder install. Where the install directory is read-only or should not be
written into, they move to the platform's own location: Application Support on
macOS, the app data directory inside a Flatpak. The game reports the directory
it chose in `pokered_log.txt` on the first line.

## Not implemented

Trading and link battles are absent for now. Online battles and trades are
planned. Everything else in the mainline single-player game is playable.

An expansion layer to the modding and authorship layer is planned for a later
date. The main goal currently is to get the base layer of the project faithful
and bug free. The engine is already built with modding and authorship in mind,
but my current focus has still been on getting this feeling exactly like you
remember. If you're a developer, take a look at the scene / block architecture
to see what I mean.

## Thanks

To [pret](https://github.com/pret), whose preservation work on the Game Boy Poke
games has taught a generation of people how this hardware and these games
actually work. This simply would not be possible without their work. I owe
everything here to them.

To [SameBoy](https://github.com/LIJI32/SameBoy) for its LCD and colour
correction work, and to [NTSC-CRT](https://github.com/LMP88959/NTSC-CRT) for the
composite signal simulation, both of which the presentation layer builds on.

## Legal

This repository contains no ROM data and no proprietary assets. All trademarks
are the property of their respective owners.

Third-party components and their licences are listed in
[THIRD_PARTY.md](THIRD_PARTY.md). The engine itself is MIT licensed.
