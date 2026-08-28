
from __future__ import annotations

import sys

def main() -> int:
    if len(sys.argv) < 11:
        return 2

    moves = [int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])]
    disabled_slot_1based = int(sys.argv[5])
    enemy_hp = int(sys.argv[6])
    enemy_max_hp = max(1, int(sys.argv[7]))
    player_hp = int(sys.argv[8])
    player_max_hp = max(1, int(sys.argv[9]))
    enemy_level = int(sys.argv[10])

    def allowed(slot: int) -> bool:
        if slot < 0 or slot > 3:
            return False
        if moves[slot] == 0:
            return False
        if disabled_slot_1based != 0 and disabled_slot_1based == slot + 1:
            return False
        return True

    available = [i for i in range(4) if allowed(i)]
    if not available:
        return 3

    enemy_ratio = enemy_hp / enemy_max_hp
    player_ratio = player_hp / player_max_hp

    if player_ratio <= 0.33:
        slot = max(available, key=lambda s: moves[s])
    elif enemy_ratio <= 0.25:
        slot = min(available, key=lambda s: moves[s])
    else:
        slot = available[enemy_level % len(available)]

    print(f"{slot} {moves[slot]}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
