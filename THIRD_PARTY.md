# Third-party code and licences

This file exists because the project ships code written by other people. Adding
something here is not paperwork — the licences below require the notice to
travel with the binary, so a component whose notice is missing cannot legally
ship.

---

## SameBoy — display shaders

`shaders/` contains the fragment shaders from **SameBoy**, used unmodified
(except where noted in each file's header) and run by the OpenGL display
backend (`src/platform/display_gl.c`).

* Upstream: https://github.com/LIJI32/SameBoy
* Licence: Expat (MIT)

SameBoy's own LICENSE states that all files and directories in its repository,
except the `iOS` and `HexFiend` directories, are under the Expat licence.
Nothing from either of those directories is used here.

```
Copyright (c) 2015-2024 Lior Halphon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## SameBoy — translated C

The Expat notice above covers these too; they are listed separately because
they are **not** shaders and would otherwise be easy to mistake for original
work when someone greps `shaders/` to find what is third-party.

The following are ports of SameBoy C into this codebase. They were translated
rather than copied verbatim (different types, different call sites), but they
are derived work and the notice applies:

* `src/platform/display.c`
  * `MONO_PALETTES` — the GREY / DMG / MGB / GBL shade sets, verbatim values
    from `Core/display.c`.
  * `temperature_tint` — the ambient-light model from `Core/display.c`,
    including the 0.375-power green falloff and the two cold-half quadratics.
  * the colour-correction curves — the 32-entry channel LUT, the gamma
    green/blue mixing, and the contrast-compression ranges behind the
    REDUCED / HARSH / BOOSTED modes, from `GB_convert_rgb15` and its
    `GB_COLOR_CORRECTION_*` branches.
  * the **Super Game Boy channel curve** -- `scale_channel_with_curve_sgb`'s
    32-entry LUT, verbatim, shipped as `GBC_CURVE_SAMEBOY_SGB`. Listed apart
    from the CGB curve above because it is a separate *finding* as much as a
    separate table: `GB_convert_rgb15` sends `GB_is_sgb(gb) || for_border` down
    a branch that uses this LUT **and nothing else**, applying neither the
    green/blue mixing nor the contrast compression. Those model a GBC LCD's
    colour crosstalk, and an SGB picture leaves over a television instead.
    Without reading SameBoy this port would have gone on putting SGB content
    through panel modelling -- roughly 40% too bright through the midtones.
* `src/platform/sgb_border.c`
  * `bgr555_to_rgba` takes the border palette through that same SGB curve,
    which is SameBoy's design rather than ours: its `for_border` case shares the
    SGB branch deliberately, so the border artwork and the picture inside it
    agree. This port previously scaled the border by 255/31 and matched neither.
* `src/platform/gb_apu.c`
  * the **square channels' sweep model**, ported from `Core/apu.c`. SameBoy's
    sweep is not instantaneous -- the 128 Hz tick only starts a calculation
    that lands a variable number of sweep-units later -- and this port
    reproduces that, down to a field named after SameBoy's own
    `channel_1_restart_hold`.
  * the **noise channel's counter model**, a free-running 14-bit counter with
    the LFSR stepped on its falling edges, from the same file.
* `src/data/gbc_palettes.c` / `.h`
  * the **CGB auto-colour palettes**. The Game Boy Color boot ROM hashes a
    monochrome cart's title and picks one of ~50 built-in colour combinations;
    the values here are Pokemon Red's entry, read from SameBoy's `cgb_boot.asm`
    (their own clean-room boot ROM) -- `PalettePerChecksum` entry 13,
    `palette_comb 3, 4, 4`.
* `src/platform/display_gl.c`
  * the letterboxing viewport maths, from `update_viewport` in `SDL/gui.c`,
    including all three scaling modes.
  * the `{filter}` splice into `MasterShader.fsh` and the frame-blending mode
    numbering, which are SameBoy's shader contract rather than ours.

---

## SameBoy — as a reference implementation

SameBoy is also used as a differential reference during development: the audio
work diffs this port's APU against it (see `docs/audio-harness.md`). That use
involves running SameBoy and comparing output, not copying code, and would need
no notice on its own — but the shaders and the C above do, and they ship.

---

## NTSC-CRT — composite video simulation

`third_party/ntsc-crt/` contains **NTSC-CRT** by **EMMIR (LMP88959)**, used
unmodified to render the Super Game Boy frame the way an SNES put it on a
television: composite chroma/luma crosstalk, dot crawl, colour bleed and
artifact colour. Only the core and the **SNES** signal profile are vendored;
see that directory's README for what was left out and why.

* Upstream: https://github.com/LMP88959/NTSC-CRT
* Licence: a bespoke permissive licence — reproduced in full below and in
  `third_party/ntsc-crt/LICENSE`

It is worth being precise about this one, because it is unusual: the licence
makes attribution **explicitly optional**. It is given here anyway. A notice
that only appears where it is legally compelled is not a notice, it is
paperwork, and this project ships other people's work either way.

```
Feel free to use the code in any way you would like, however, if you release
anything with it, a comment in your code/README saying where you got this code
would be a nice gesture but it’s not mandatory.

The software is provided "as is", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose and noninfringement. In no event shall the
authors or copyright holders be liable for any claim, damages or other
liability, whether in an action of contract, tort or otherwise, arising from,
out of or in connection with the software or the use or other dealings in the
software.
```

---

## OldAmber CRT renderer — design provenance

`src/platform/crt_renderer.[ch]`, `src/platform/crt_renderer_gl.[ch]`,
`src/platform/gl_api.[ch]` and everything under `shaders/crt/` are **original
OldAmber code**, licensed **MIT**
(SPDX-License-Identifier in every file). They are written from
`docs/crt-renderer-implementation-spec.md`, public display physics, and the
permissive references that spec's section 18 approves.

This entry exists because "we wrote it ourselves" is worth being able to
demonstrate rather than assert, and because the distinction below is easy to
lose later.

**Ideas studied — no code reused.** Concepts that are physics or common
practice, not anyone's expression: separating beam width from mask pitch;
measuring mask pitch in output pixels rather than source texels; a
luminance-dependent beam; working in linear light; bloom/halation as separate
radii; exponential phosphor persistence.

**Code reused: none.** No shader from any source was copied, translated,
mechanically ported or closely restructured into this renderer, and no
third-party parameter table or preset constant is reproduced. The profile values
in `crt_renderer.c` are conservative calibration seeds chosen for this project
and marked as such in the source; section 23 of the spec requires
reference-display measurement before any of them may be called final.

**Deliberately NOT consulted while writing it**, since their licences are
incompatible with this project's policy: CRT-Royale, CRT Guest Advanced,
CRT-Geom and CRT-Easymode (all GPL). They are named here so the exclusion is on
the record.

**A trap worth recording.** libretro's shader collections mix licences per file
with no repository-wide grant, and at least one file — `crt-mattias.glsl` —
carries no licence statement at all, only an author name and a Shadertoy link,
which makes its default terms CC BY-NC-SA. A file's own header is the only
reliable statement; the surrounding repository is not.

---

## SDL2 — window, input, audio device

`SDL2.dll` is **redistributed in the release bundle**
(`tools/dist/make_release.sh` copies it next to the executable), and the game
links against it for the window, the GL context, input and the audio device.

* Upstream: https://www.libsdl.org/
* Version shipped: **2.32.10**
* Licence: **zlib**

The zlib licence does not compel a notice in binary form the way Expat does.
It is given here because we ship the library itself, not merely link it, and
because a redistributed binary with no statement of where it came from is
unhelpful to anyone who receives it.

```
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

The text above is the complete notice, reproduced here rather than pointed at,
because the library's own archive is not carried in this tree. SDL2 comes from
your package manager, and the MSVC development libraries are downloaded from
upstream when you want them.

---

## CPython — embedded in setup.exe

`setup.exe` in the release bundle is built with PyInstaller from
`tools/dist/setup_assets.py`. PyInstaller works by embedding a **complete
CPython interpreter and standard library** into the executable, so the
bundle distributes CPython whether or not the player ever sees Python.

* Upstream: https://www.python.org/
* Version embedded: **CPython 3.11.9**
* Licence: **PSF License Agreement** (permissive, GPL-incompatible in the
  copyleft direction only — it imposes nothing on this project's own code)

The PSF licence requires that a copy of the agreement travel with any
distribution containing Python, and that modifications be noted. Nothing here
modifies CPython; it is embedded verbatim by the packager. The agreement ships
with the interpreter as `LICENSE.txt` in the Python installation and is
reproduced in the release bundle alongside this file.

---

## PyInstaller — the packager for setup.exe

Recorded separately from CPython because its licence has a condition worth
being precise about, and because it is the only GPL-lineage component anywhere
near this project.

* Upstream: https://pyinstaller.org/
* Version used: **6.21.0**
* Licence, verbatim from the installed package metadata
  (`pyinstaller-6.21.0.dist-info/METADATA`):

```
GPLv2-or-later with a special exception which allows to use PyInstaller
to build and distribute non-free programs (including commercial ones)
```

**That exception is the whole point.** PyInstaller's bootloader is linked into
every executable it produces, and without the exception a GPL bootloader would
reach the program it wraps. With it, `setup.exe` — and by extension this
project — carries no copyleft obligation. Nothing of PyInstaller is present in
`OldAmber.exe` itself; it touches only the asset-import helper.

Recorded so that "is anything here GPL?" has an answer on file rather than
being re-derived, and so that a future bump of the packager is checked against
this condition rather than assumed.

---

## What is NOT third-party

The Game Boy / Pokémon ROM data this project reads is **not** redistributed.
The repository ships extractors, and the user supplies their own ROM. Nothing
in this file grants any right to that data.
