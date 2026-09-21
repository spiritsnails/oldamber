# Scenario fixtures

Scenario fixtures start OldAmber in an isolated temporary directory with a
specific, reproducible game state. They never overwrite the normal save or
settings.

Run a fixture from the development repository:

```powershell
pwsh -NoProfile -File tools\py.ps1 -u tools\run_scenario.py scenarios\example.yaml
```

Use `--validate-only` to check a file without launching the game. Use `--keep`
to retain the temporary directory and its `scenario_applied.json` verification
report after the game exits.

Fixtures use a fresh game state unless `base` explicitly names a save:

```yaml
base:
  type: current_save
  path: C:\path\to\pokered.sav
```

The schema can configure:

- game version, player/rival identity, money, coins, and badges
- vmap, coordinates, facing, movement mode, and last-healed respawn point
- party, all PC boxes, and Day Care Pokémon
- species, level, moves, nickname, OT, HP, status, DVs, stat experience,
  experience, and PP/PP Ups
- bag and PC items
- Pokédex seen/caught state
- set and cleared event flags, including AmberScript-assigned numeric flags
- original-map item pickups, completed in-game trades, and visited-town flags
- RNG bytes, play time, saved text/battle/sound options, and rival starter

Odd but structurally valid states are intentional test cases. A low-level
fully evolved Pokémon, unusual moveset, or strange story-flag combination is
accepted. Invalid identifiers, impossible storage sizes, malformed values,
and contradictory event requests are rejected before launch.

Press F2 during a debug-enabled game to create the normal bug-report bundle.
The bundle now includes `scenario.yaml`, a standalone fresh-state fixture for
the exact live state at capture time. It can be validated or launched with the
same command above. Human-readable names are accompanied by their exact Gen I
encoded bytes so uncommon nickname glyphs survive a capture/load round trip.

After capture, OldAmber copies a GitHub-ready fenced YAML block to the system
clipboard and opens a controller-friendly confirmation panel. `REPORT A BUG`
opens the public bug form, `OPEN FOLDER` shows the complete local report bundle,
and B or Escape returns to the game. Paste the clipboard contents into the bug
report so a developer can recreate the state without receiving the player's
ordinary save file.
