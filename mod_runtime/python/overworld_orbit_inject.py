
from __future__ import annotations

import sys

def main() -> int:
    if len(sys.argv) < 5:
        return 2

    map_id = int(sys.argv[1], 0)
    px = int(sys.argv[2], 0)
    py = int(sys.argv[3], 0)
    pdir = int(sys.argv[4], 0)

    clockwise = ((px + py + map_id) % 2) == 0
    loops = 1 + (map_id % 2)
    step_wait = 10 + ((px ^ py) & 7)
    sprite = "ROCKET" if (map_id % 3 == 0) else "COOLTRAINER_M"

    if clockwise:
        seq = [("down", "right"), ("right", "up"), ("up", "left"), ("left", "down")]
    else:
        seq = [("down", "left"), ("left", "up"), ("up", "right"), ("right", "down")]

    print(f'spawn orbiter {sprite} player+1 player+0')
    print("lock_input on")
    print('say "Python injected overworld behavior!@"')
    for _ in range(loops):
        for face_dir, move_dir in seq:
            print(f"face orbiter {face_dir}")
            print(f"move orbiter {move_dir} 1")
            print(f"wait {step_wait}")
    if pdir in (0, 4, 8, 12):
        print("face orbiter player")
    print("wait 16")
    print("lock_input off")
    print("despawn orbiter")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
