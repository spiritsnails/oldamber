
from __future__ import annotations

import sys

def main() -> int:
    if len(sys.argv) < 9:
        return 2

    elapsed_sec = int(sys.argv[8], 0)

    phase = (elapsed_sec // 2) % 2
    direction = 0 if phase == 0 else 1

    print(f"npc_step {direction}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
