OLDAMBER
========

WHAT TO DO

1. Run OldAmber.
2. Choose your Poke Red ROM when the launcher asks for it.
3. Press PLAY.

Step 2 happens once. After that the launcher opens straight to PLAY.

Building the game data takes a few seconds. If you ever need to do it
again, RE-IMPORT A ROM is on the launcher next to the PLAY button.


WHY DO I NEED TO SUPPLY A ROM?

This build contains none of the original game's data. No graphics, no
music, no Poke stats. All of it is read out of your own copy of the
game the first time you run it.

Your ROM is only read. It is never modified, copied, or sent anywhere.

This release reads Poke Red. Yellow will not work, and neither will a
modified or incomplete copy. The launcher says so plainly if the file
you picked is not right.


A NOTE ON HOW IT LOOKS

This build starts as plain Game Boy black and white, with the original
Gen 1 battle screen and sprites. That is the default on purpose and does
not need reporting.

There is a colour layer, and shader options including LCD and CRT
simulation, in the options screen. Press Escape to reach them. A few
presentation options are held back in this release because they are not
finished.


CONTROLS

  Arrow keys      move
  Z               A button   (confirm, talk, interact)
  X               B button   (cancel, back)
  Enter           Start      (menu)
  Right Shift     Select
  Escape          options and save states
  Shift + Escape  quit

A game controller works on connection, and the launcher can be driven
entirely with one. On a pad, A and B are the two lower face buttons, Back
and Start are Select and Start, and L1 and R1 together open the options.
Everything is rebindable from the CONTROLS page of the options screen. If
a binding ever leaves you stuck, hold F5, or Back and Start together on a
pad, for two seconds to put the defaults back.


WHERE YOUR SAVES GO

On Windows, your save, save-state slots, backups and settings live in:

  %APPDATA%\spiritsnails\OldAmber\

The first version that uses this location copies any older save and settings
from beside OldAmber.exe automatically. It does not delete or overwrite either
copy. You can replace the game folder during an update without moving your
save by hand.

The imported game data and log still sit beside the game in an ordinary folder
install. Re-import your ROM if that imported data is ever removed.

Where the game cannot write beside itself, which is the case for the
macOS app and the Flatpak, the data uses the usual place for that platform
instead. The first line of pokered_log.txt names the content directory the
game chose, whichever it was.


IF SOMETHING GOES WRONG

The launcher refuses the ROM
  It is checking that the file is an unmodified Poke Red dump. If you are
  sure it is, include exactly what the error said.

The game will not start, or the maps look like garbage
  The import did not finish. Run RE-IMPORT A ROM from the launcher and
  wait for it to say IMPORT COMPLETE.

Anything else
  Say what you were doing and where you were standing. A screenshot or a
  short video clip is ideal. There is no need to reproduce it or work out
  why. Just describe what happened.

pokered_log.txt records what happened on the last run, and attaching it
helps. Do not attach your ROM, your save, or anything the import produced.

  Bugs:    https://github.com/spiritsnails/oldamber/issues
  Discord: https://discord.gg/2pZNRp6t8


CREDITS AND LICENCES

This build includes work by other people, used under permissive licences:
SameBoy (Lior Halphon), NTSC-CRT (EMMIR / LMP88959), SDL2 (Sam Lantinga)
and CPython. See THIRD_PARTY.md next to this file for the full notices,
and LICENSE-Python.txt for the PSF agreement covering the interpreter
that builds the game data.

The game itself is MIT licensed. No ROM data is redistributed. The game
reads the ROM you supply from your own computer.
