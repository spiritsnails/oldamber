# NTSC-CRT (vendored)

Integer-only NTSC composite video encoding/decoding emulation, **by EMMIR
(LMP88959)** â€” <https://github.com/LMP88959/NTSC-CRT>.

Used by this port to render the Super Game Boy frame the way an SNES actually
put it on a TV: composite chroma/luma crosstalk, dot crawl, colour bleed and
artifact colour. See `src/platform/ntsc_filter.c` for the integration and
`THIRD_PARTY.md` for the licence notice that has to ship with the binary.

## What is here, and why only this

Vendored rather than pulled at build time, so a clean checkout builds without
network access â€” same reason `shaders/` carries SameBoy's shaders directly.

| file | why |
|---|---|
| `crt_core.c/.h` | the encoder/decoder itself |
| `crt_snes.c/.h` | the **SNES** signal profile |
| `LICENSE` | upstream licence, unmodified |

The upstream repository also ships NES, NES-RGB, PV-1000, VHS and generic-NTSC
profiles, plus a demo front-end (`crt_main.c`, `bmp_rw.c`, `ppm_rw.c`). None of
those are used here and none are vendored.

`CRT_SYSTEM` must be defined as `CRT_SYSTEM_SNES` (3) for the build; `crt_core.h`
selects the profile header from it, and getting it wrong silently compiles a
different machine's line timing. The port sets it in `CMakeLists.txt`.

## Unmodified

These files are upstream, byte for byte. Keeping them that way is deliberate:
any change we need should go in `src/platform/ntsc_filter.c` instead, so this
directory stays trivially diffable against upstream.

## Why the SNES profile matters

It is not generic NTSC with a different name. `crt_snes.h` carries the real line
structure in PPU pixels â€” 9 front porch, 25 sync, 4 breezeway, 15 colour burst,
5 back porch, 15 left border, **256 active video**, 11 right border â€” with IRE
levels for each. The 256 active pixels line up exactly with the SGB frame's
width, which is why the artifacts land on the right pixel boundaries.

Measured on this port's own 256x224 SGB frame, `-O2`, single thread:
**~3.1 ms/frame**, about 19% of a 16.7 ms budget. `sizeof(struct CRT)` is
~476 KB (two signal planes), so an instance must be static or heap-allocated,
never a local.
