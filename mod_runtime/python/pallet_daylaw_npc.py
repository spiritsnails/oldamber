
import sys

if len(sys.argv) < 11:
    raise SystemExit(0)

cur_map = int(sys.argv[1])
npc_x = int(sys.argv[6])
npc_y = int(sys.argv[7])
elapsed = int(sys.argv[8])
band = int(sys.argv[10])

PALLET_MAP_IDS = {0, 194}
if cur_map not in PALLET_MAP_IDS:
    raise SystemExit(0)

if band == 0:
    if npc_y < 9:
        print("npc_step 0")
        raise SystemExit(0)

    if npc_x <= 4:
        print("npc_step 3")
    elif npc_x >= 16:
        print("npc_step 2")
    else:
        print("npc_step 3" if (elapsed % 6) < 3 else "npc_step 2")
else:
    if npc_y > 8:
        print("npc_step 1")
        raise SystemExit(0)

    if npc_x <= 4:
        print("npc_step 3")
    elif npc_x >= 16:
        print("npc_step 2")
    else:
        print("npc_step 2" if (elapsed % 6) < 3 else "npc_step 3")
