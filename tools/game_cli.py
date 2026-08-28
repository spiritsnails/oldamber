
import sys
import os
import time
import argparse
from dataclasses import dataclass

TIMEOUT    = 10.0

def _find_bugs_dir():
    import os
    candidates = [
        ("bugs",              "pokered.exe"),
        ("bugs",              "oldamber.exe"),
        ("build/bugs",        "pokered.exe"),
        ("build-red/bugs",    "oldamber.exe"),
        ("../build/bugs",     "pokered.exe"),
        ("../build-red/bugs", "oldamber.exe"),
    ]
    for c, exe in candidates:
        parent = os.path.normpath(os.path.join(c, ".."))
        if os.path.exists(os.path.join(parent, exe)):
            return c
    return "build/bugs"

_BUGS = _find_bugs_dir()
CMD_FILE   = f"{_BUGS}/cli_cmd.txt"
STATE_FILE = f"{_BUGS}/cli_state.txt"
TELE_FILE  = f"{_BUGS}/teleport.txt"

@dataclass
class CommandResult:
    ok: bool
    consumed: bool
    state_updated: bool
    message: str

def ensure_bugs_dir():
    os.makedirs(_BUGS, exist_ok=True)

def send_command(cmd: str) -> CommandResult:
    ensure_bugs_dir()

    try:
        before = os.path.getmtime(STATE_FILE)
    except FileNotFoundError:
        before = 0.0

    with open(CMD_FILE, "w") as f:
        f.write(cmd.strip() + "\n")

    deadline = time.time() + TIMEOUT
    while time.time() < deadline:

        if not os.path.exists(CMD_FILE):

            inner = time.time() + TIMEOUT
            while time.time() < inner:
                try:
                    after = os.path.getmtime(STATE_FILE)
                    if after > before:
                        return CommandResult(
                            ok=True,
                            consumed=True,
                            state_updated=True,
                            message="command consumed; state updated",
                        )
                except FileNotFoundError:
                    pass
                time.sleep(0.05)
            return CommandResult(
                ok=True,
                consumed=True,
                state_updated=False,
                message="command consumed; state update not observed before timeout",
            )
        time.sleep(0.05)

    try:
        os.remove(CMD_FILE)
    except FileNotFoundError:
        pass
    return CommandResult(
        ok=False,
        consumed=False,
        state_updated=False,
        message="timed out waiting for game to consume command",
    )

NAMED_LOCATIONS = {

    "pallet":           (0,  10, 18),
    "pallet_town":      (0,  10, 18),
    "viridian":         (1,  24, 40),
    "viridian_city":    (1,  24, 40),
    "pewter":           (2,  16, 18),
    "pewter_city":      (2,  16, 18),
    "cerulean":         (3,  28, 18),
    "cerulean_city":    (3,  28, 18),
    "vermilion":        (5,  28, 24),
    "vermilion_city":   (5,  28, 24),
    "lavender":         (4,  14, 18),
    "lavender_town":    (4,  14, 18),
    "celadon":          (6,  44, 24),
    "celadon_city":     (6,  44, 24),
    "fuchsia":          (7,  28, 24),
    "fuchsia_city":     (7,  28, 24),
    "cinnabar":         (8,  14, 18),
    "cinnabar_island":  (8,  14, 18),
    "saffron":          (10, 28, 28),
    "saffron_city":     (10, 28, 28),

    "viridian_gym":     (52,  8,  9),
    "pewter_gym":       (54,  8,  9),
    "cerulean_gym":     (65,  8,  9),
    "vermilion_gym":    (92,  8,  9),
    "celadon_gym":      (135, 8,  9),
    "fuchsia_gym":      (166, 8,  9),
    "saffron_gym":      (178, 8,  9),
    "cinnabar_gym":     (234, 8,  9),

    "oaks_lab":         (37,  12, 11),
    "viridian_forest":  (51,  14, 40),
    "mt_moon":          (59,  14, 10),
    "rock_tunnel":      (155, 14, 10),
    "pokemon_tower":    (142, 8,  9),
    "silph_co":         (200, 8,  9),
    "safari_zone":      (217, 28, 20),

    "route_1":  (12, 14, 70), "route_2":  (13, 14, 10),
    "route_3":  (14, 14, 10), "route_4":  (15, 14, 10),
    "route_5":  (16, 14, 10), "route_6":  (17, 14, 70),
    "route_7":  (18, 14, 10), "route_8":  (19, 14, 70),
    "route_9":  (20, 14, 10), "route_10": (21, 14, 10),
    "route_11": (22, 14, 10), "route_12": (23, 14, 10),
    "route_24": (33, 14, 10), "route_25": (34, 14, 10),
}

def send_teleport(args_str: str):
    ensure_bugs_dir()
    args = args_str.strip()

    key = args.split()[0].lower().replace("-", "_") if args else ""
    if key and not key[0].isdigit():
        if key not in NAMED_LOCATIONS:
            print(f"[cli] Unknown location: {key}")
            print(f"[cli] Known: {', '.join(sorted(NAMED_LOCATIONS))}")
            return
        m, x, y = NAMED_LOCATIONS[key]
        args = f"{m} {x} {y}"

    parts = args.split()
    parts = [str(int(p, 0)) if p.startswith("0x") or p.startswith("0X") else p for p in parts]
    cmd = "teleport " + " ".join(parts)

    print(f"[cli] Teleport: {cmd}")
    res = send_command(cmd)
    if res.ok:
        print(f"[cli] OK: {res.message}")
    else:
        print(f"[cli] ERROR: {res.message}")
    time.sleep(0.3)
    show_state()

def show_state():
    try:
        with open(STATE_FILE) as f:
            print(f.read())
    except FileNotFoundError:
        print("[cli] No state file yet — send a command first (or type 'state').")

def run_command(raw: str) -> bool:
    cmd = raw.strip()
    if not cmd or cmd.startswith("#"):
        return True
    if cmd in ("quit", "exit", "q"):
        return False

    verb = cmd.split()[0].lower()

    if verb == "teleport":
        rest = cmd[len("teleport"):].strip()
        send_teleport(rest)
        return True

    res = send_command(cmd)
    if res.ok:
        print(f"[cli] OK: {res.message}")
    else:
        print(f"[cli] ERROR: {res.message}. Is the game running?")
    show_state()
    return True

def interactive():
    print("pokered game CLI  (type 'help' for commands, 'quit' to exit)")
    print("Game must be running. Commands go to bugs/cli_cmd.txt.\n")
    show_state()
    while True:
        try:
            raw = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if raw == "help":
            print(__doc__)
            continue
        if not run_command(raw):
            break

def run_script(path: str):
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            print(f">>> {line}")
            if not run_command(line):
                break

def main():
    parser = argparse.ArgumentParser(description="pokered game CLI")
    parser.add_argument("command", nargs="?", help="single command to run")
    parser.add_argument("--script", help="run commands from a file")
    args = parser.parse_args()

    if args.script:
        run_script(args.script)
    elif args.command:
        run_command(args.command)
        show_state()
    else:
        interactive()

if __name__ == "__main__":
    main()
