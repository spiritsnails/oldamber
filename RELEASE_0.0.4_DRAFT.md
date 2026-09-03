# OldAmber 0.0.4

## Changes

- Added optional debug tooling for beta testers and advanced users. Enable it from the launcher to add a **DEBUG TOOLS** page to the suspend menu with teleport, noclip, automatic battle wins, AmberScript tracing, and march tracing.
- The launcher remembers whether debug tooling was enabled. It remains disabled by default for new installations.
- Improved the save editor's location controls. Current locations are now shown by name instead of a fragile numeric map ID, and locations can be selected from the maps available in the installed game data.
- Added coordinate validation and safer virtual-map handling when changing a save's location.
- Added dedicated GitHub forms for reporting bugs and requesting features, with direct links in the README.

## Updating from OldAmber 0.0.3

Select **UPDATE GAME** in the OldAmber 0.0.3 launcher. It will download and install 0.0.4 while preserving your saves, save states, settings, and imported game data.

You can also update manually by downloading the normal package for your platform, unpacking it, and running the new version. User data is stored separately from the application on current releases.

### Windows users still on 0.0.1

If you are updating directly from 0.0.1, extract the new release over your existing OldAmber folder and replace the old files. Launch the game once so your save, save states, and settings can be copied to:

`%APPDATA%\spiritsnails\OldAmber\`

The original files will remain untouched as a backup. After the migration succeeds, the old application folder can be removed.

Fresh installations require no additional steps.

## Which file should I download?

- **Windows:** `OldAmber-0.0.4-windows-x64.zip`
- **macOS:** `OldAmber-0.0.4-macos-universal.zip`
- **Linux:** `OldAmber-0.0.4-x86_64.AppImage` is recommended for most users. Flatpak and `.tar.gz` alternatives are also available below.

Files containing **`-update`** and the file named **`oldamber-update.txt`** are used automatically by OldAmber's built-in updater. You do not need to download them manually.

## You supply your own legally acquired ROM

No game data ships here. Graphics, music, text, maps, and stat tables are read from a cartridge dump you already own the first time you run the game. Your ROM is opened read-only and stays where it is.

Pokémon Red and Pokémon Blue are both supported. Each has its own imported game data and save file, and the launcher lists whichever versions are installed.

## Downloads

| Platform | File |
|---|---|
| Windows 10/11, 64-bit | `OldAmber-0.0.4-windows-x64.zip` |
| macOS 10.15+, Apple Silicon and Intel | `OldAmber-0.0.4-macos-universal.zip` |
| Linux, self-contained | `OldAmber-0.0.4-x86_64.AppImage` |
| Linux, Flatpak | `OldAmber-0.0.4-linux-x86_64.flatpak` |
| Linux, plain archive | `OldAmber-0.0.4-linux-x64.tar.gz` |

Unpack it, run it, and point it at your ROM. OldAmber builds the game data once and afterwards opens straight to **PLAY**.

### Linux

#### Steam Deck quick setup

OldAmber can add itself to your Steam library without making you configure a non-Steam shortcut by hand:

1. Switch to **Desktop Mode** and download `OldAmber-0.0.4-linux-x64.tar.gz`.
2. Extract the archive to a permanent location, such as a Games folder. Do not move or delete that folder after adding it to Steam.
3. Make sure the Steam desktop client is running, then open the extracted `OldAmber-linux` folder and run `OldAmber.sh`.
4. Point OldAmber at your legally acquired Pokémon Red or Pokémon Blue ROM and let it build the game data.
5. Select **ADD TO STEAM** in the OldAmber launcher. After it confirms that OldAmber is in your Steam library, return to **Gaming Mode** and launch it normally from there.

The **ADD TO STEAM** option appears automatically when OldAmber detects a Steam Deck, Steam is running, and OldAmber has not already been added. If your file manager opens `OldAmber.sh` as text, mark it executable in its properties or right-click it and choose **Run in Konsole**.

The automatic shortcut flow uses the extracted `.tar.gz` release because it provides a permanent `OldAmber.sh` launch target. AppImage and Flatpak users can add OldAmber manually through the Steam desktop client. This functionality is coming to AppImage and Flatpak soon™.

#### Flatpak

The Flatpak pins its own runtime and is a native Linux option:

```sh
flatpak install --user OldAmber-0.0.4-linux-x86_64.flatpak
flatpak run com.spiritsnails.OldAmber
```

The AppImage carries its own libraries and requires glibc 2.38 or newer. Mark it executable and run it. On Arch and its derivatives, install `fuse2` if it reports that `libfuse.so.2` is missing, or run it with:

```sh
./OldAmber-0.0.4-x86_64.AppImage --appimage-extract-and-run
```

The `.tar.gz` archive is the smallest download but expects SDL2, Wayland or X11, and PulseAudio to already be installed.

## macOS: the first launch needs a right-click

The macOS build is ad-hoc signed rather than notarized. On its first launch:

> Right-click or Control-click `OldAmber.app`, choose **Open**, then choose **Open** again in the dialog.

If there is no Open button, open **System Settings**, select **Privacy & Security**, scroll down, and choose **Open Anyway**.

## Checksums

```text
808c5cdb1165d469b0b91e06574115d85bdd6a63a7fe38cd728d77c8b3549305  OldAmber-0.0.4-windows-x64.zip
f772373df6572cf5a07fb46c1df4a4b0ab7414cbe108dbc4881ded65454bdca1  OldAmber-0.0.4-macos-universal.zip
3cae428147fb84aede852187049c70a3209488112a34e05b29cfc71c14849d4e  OldAmber-0.0.4-linux-x64.tar.gz
3b4ecb78ac102aa46d82240c8b5f7bf98fa1150447a426f66341874e48568f1d  OldAmber-0.0.4-x86_64.AppImage
a107c9407e1c58255f170930d2eb09259b4e8413c16665429579463c9d2aae5b  OldAmber-0.0.4-linux-x86_64.flatpak
```

## Thanks

To [pret](https://github.com/pret), whose preservation work on the Game Boy games has taught a generation how this hardware and software work.

To [SameBoy](https://github.com/LIJI32/SameBoy) for its LCD, color-correction, and APU work, and to [NTSC-CRT](https://github.com/LMP88959/NTSC-CRT) for the composite-signal simulation.

This repository contains no ROM data or proprietary assets. All trademarks are the property of their respective owners.
