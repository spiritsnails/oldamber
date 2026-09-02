# OldAmber update format

Release installs have a stable bootstrap and immutable game payloads. The
bootstrap reads `bundled-version` beside itself, then prefers `current-version`
from OldAmber's per-user data directory when that version exists there. Game
data, saves, settings, imported ROM-derived data, and downloaded versions stay
outside the release folder.

The launcher checks this release asset:

`https://github.com/spiritsnails/oldamber/releases/latest/download/oldamber-update.txt`

The manifest is UTF-8 `key=value` text. It declares `schema`, `version`,
`minimum-bootstrap`, and `url`, `sha256`, and `size` for `windows-x64`,
`linux-x64`, and `macos-universal`. Flatpak updates remain managed by Flatpak.

Each update archive contains one version payload directly at its root. The
game downloads to a `.part` file, checks the exact byte count and SHA-256,
extracts into a staging directory, confirms the platform game binary exists,
renames the directory into an encoded path such as `versions/v0_0_3`, and atomically replaces
`current-version`. Exit code 75 asks the bootstrap to read the pointer again
and launch the new version.

The three platform packaging scripts create their update archives. Once all
three are present in `build/`, run:

```sh
python tools/dist/make_update_manifest.py
```

Upload `build/oldamber-update.txt` and all three update archives as assets on
the matching GitHub release. Full ZIP, tarball, AppImage, Flatpak, and macOS
downloads are still built and published for clean installs.
