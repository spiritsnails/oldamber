#!/usr/bin/env bash
# launch_game.sh: Launch pokered.exe if not already running.
# Waits up to 15s for the CLI to become responsive.
#
# Run from anywhere inside the pokered repo.
# Exit codes: 0 = game ready, 1 = failed to start

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Two checkouts share this script: a normal dev checkout builds
# build/pokered.exe, while a RED_ONLY checkout (tools/setup_red.sh) builds
# build-red/oldamber.exe under the same CMake target. Same dual-candidate
# pattern as dashboard.py, lab.py and game_cli.py use.
BUILD_DIR="$REPO_ROOT/build"
GAME_EXE="$BUILD_DIR/pokered.exe"
PROC_NAME="pokered"
# GAME_VERSION picks the base game, same name and values as CMake's option, so
# `GAME_VERSION=blue bash tools/launch_game.sh` runs the Blue build. Its assets
# live in packages/blue, generated/blue and generatedmaps/blue, separate trees
# throughout, so the two installs never touch each other.
GAME_VERSION="${GAME_VERSION:-red}"
if [ -f "$REPO_ROOT/build-$GAME_VERSION/oldamber.exe" ]; then
    BUILD_DIR="$REPO_ROOT/build-$GAME_VERSION"
    GAME_EXE="$BUILD_DIR/oldamber.exe"
    PROC_NAME="oldamber"
elif [ ! -f "$GAME_EXE" ] && [ -f "$REPO_ROOT/build-red/oldamber.exe" ]; then
    BUILD_DIR="$REPO_ROOT/build-red"
    GAME_EXE="$BUILD_DIR/oldamber.exe"
    PROC_NAME="oldamber"
fi
TIMEOUT=15

# Already running? An absolute path to tasklist.exe, because bare `tasklist` is
# not on the PATH this MSYS2 environment resolves, the same problem as
# build.sh's bare `taskkill`.
#
# Not `ps`: MSYS2's ps only enumerates processes reachable from this bash's own
# session tree, so it reports "not running" for a game launched from another
# shell, and this script would then start a second game on top of the existing
# one. Both would race over the same bugs/cli_cmd.txt and bugs/cli_state.txt.
# tasklist.exe has a system-wide view, matching what build.sh's taskkill.exe
# actually kills.
if /c/Windows/System32/tasklist.exe //FI "IMAGENAME eq $PROC_NAME.exe" 2>/dev/null \
        | grep -qi "$PROC_NAME.exe"; then
    echo "[launch] $PROC_NAME.exe already running"
    exit 0
fi

# Launch from build/ so the game finds bugs/cli_state.txt relative to its CWD
# Extra args (e.g. --pson to start with amberscript pre-enabled, equivalent
# to running the "amberscript on" CLI command right after launch) pass
# through directly to pokered.exe.
#
# stdout and stderr go to a persistent log file rather than this script's own
# output. This script exits a few seconds after the game is up, so anything the
# game prints later, such as an amberscript parse error when a map streams in on
# a later warp, would otherwise be lost. The log file is readable at any time.
GAME_LOG="$BUILD_DIR/${PROC_NAME}_log.txt"
echo "[launch] starting $PROC_NAME.exe... $* (log: $GAME_LOG)"
(cd "$BUILD_DIR" && "$GAME_EXE" --skip "$@" > "$GAME_LOG" 2>&1) &

# NB: the debug suite dashboard is NOT started here. Launch it explicitly
# with tools\debugsuite\dsuite.ps1 (PowerShell), which starts the game +
# dashboard + browser together as a single opt-in.

# Poll game_cli.py for a response (it handles the bugs/ path correctly)
for i in $(seq 1 $TIMEOUT); do
    sleep 1
    if python "$REPO_ROOT/tools/game_cli.py" state 2>/dev/null | grep -q "==="; then
        echo "[launch] game ready (${i}s)"

        # Rebuild-progress handoff: build.sh quicksaves to bugs/qs_prebuild.state
        # right before killing the game (if it was running) so a rebuild mid-
        # playtest doesn't lose position/party/battle state the way a plain
        # relaunch-from-last-save would.
        #
        # Consumed once, whether the restore succeeds or fails. `quickload`
        # drops a whole state snapshot over the top of the save the game just
        # loaded, so a file left armed would rewind every later launch to that
        # one old snapshot, and the next in-game save would write the rewound
        # state back into pokered.sav. A handoff that did not land is moved
        # aside rather than deleted, so it is still recoverable by hand.
        PREBUILD_STATE="$BUILD_DIR/bugs/qs_prebuild.state"

        # A handoff must never override a save that is newer than it.
        #
        # If the player saved after the snapshot was taken, restoring it would
        # throw that save away and the next in-game save would write the rewound
        # state back to disk.
        #
        # Both files sit in the same directory, so a plain mtime comparison is
        # the whole test: the newer save wins and the snapshot is moved aside,
        # never deleted.
        if [ -f "$PREBUILD_STATE" ]; then
            for _sav in "$BUILD_DIR"/*.sav; do
                [ -f "$_sav" ] || continue
                if [ "$_sav" -nt "$PREBUILD_STATE" ]; then
                    mv -f "$PREBUILD_STATE" "$PREBUILD_STATE.stale"
                    echo "[launch] pre-rebuild state is OLDER than $(basename "$_sav") -- not restoring."
                    echo "[launch]   Your save wins. Snapshot kept at $(basename "$PREBUILD_STATE").stale"
                    break
                fi
            done
        fi

        if [ -f "$PREBUILD_STATE" ]; then
            echo "[launch] restoring pre-rebuild state..."
            python "$REPO_ROOT/tools/game_cli.py" "quickload prebuild" >/dev/null 2>&1 || true
            sleep 1   # let the game flush its stdout into GAME_LOG
            # Check the game's log, not the CLI's exit status. game_cli.py
            # printing "[cli] OK" only means the command was delivered; whether
            # the state actually loaded is only said in the game's own output.
            if grep -q "quickload prebuild <-" "$GAME_LOG" 2>/dev/null; then
                rm -f "$PREBUILD_STATE"
                echo "[launch] pre-rebuild state restored"
            else
                mv -f "$PREBUILD_STATE" "$PREBUILD_STATE.unrestored"
                echo "[launch] WARNING: pre-rebuild state did NOT restore (see $GAME_LOG)."
                echo "[launch]   Moved to $PREBUILD_STATE.unrestored so it cannot silently"
                echo "[launch]   rewind later launches. Recover by hand if you need it."
            fi
        fi

        exit 0
    fi
done

echo "[launch] ERROR: game did not respond within ${TIMEOUT}s"
exit 1
