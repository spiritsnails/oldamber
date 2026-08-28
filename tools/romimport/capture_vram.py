
import argparse
import os
import sys
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import crystal_maps as M
from crystal_rom import Rom

PC_DIR = os.path.join(ROOT, "pokecrystal-master")

WMAPTILESET = 0xD199
WYCOORD, WXCOORD = 0xDCB7, 0xDCB8
WMAPGROUP, WMAPNUMBER = 0xDCB5, 0xDCB6

def build_graph(rom, id_to_name, name_to_id):
    graph, tileset_of = {}, {}
    for (g, m), name in id_to_name.items():
        try:
            hdr = M.read_map_header(rom, g, m)
            attr = M.read_attributes(rom, *hdr["attributes"], id_to_name)
            ev = M.read_events(rom, *attr["events"])
        except Exception:
            continue
        tileset_of[(g, m)] = hdr["tileset"]
        graph[(g, m)] = [(w["x"], w["y"], w["group"], w["map"])
                         for w in ev["warps"]]
    return graph, tileset_of

def route_to(graph, tileset_of, start, want_tileset):
    if tileset_of.get(start) == want_tileset:
        return []
    prev = {start: None}
    q = deque([start])
    while q:
        cur = q.popleft()
        for (wx, wy, dg, dm) in graph.get(cur, []):
            dest = (dg, dm)
            if dest in prev or dest not in graph:
                continue
            prev[dest] = (cur, wx, wy)
            if tileset_of.get(dest) == want_tileset:
                steps, node = [], dest
                while prev[node] is not None:
                    p, wx2, wy2 = prev[node]
                    steps.append((p, wx2, wy2, node))
                    node = p
                return list(reversed(steps))
            q.append(dest)
    return None

LAND_TILE, WATER_TILE = 0x00, 0x01

def walkable_grid(rom, group, mapno, id_to_name):
    hdr = M.read_map_header(rom, group, mapno)
    attr = M.read_attributes(rom, *hdr["attributes"], id_to_name)
    ts = M.read_tileset_entry(rom, hdr["tileset"])
    n = M.metatile_count(rom, ts)
    coll = M.read_collision(rom, *ts["coll"], count=n)
    blocks = M.read_blocks(rom, *attr["blocks"], attr["width"], attr["height"])
    bank, addr = rom.addr_of("CollisionPermissionTable")
    end = rom.next_symbol_addr(bank, addr)
    perms = list(rom.read(bank, addr, (end - addr) if end else 256))
    w, h = attr["width"] * 2, attr["height"] * 2
    grid = [[False] * w for _ in range(h)]
    for cy in range(h):
        for cx in range(w):
            cid = M.cell_collision(blocks, coll, attr["width"], cx, cy)
            p = perms[cid] if cid < len(perms) else 0xFF
            grid[cy][cx] = p in (LAND_TILE, WATER_TILE)
    return grid

def bfs_path(grid, start, goal):
    if not grid:
        return None
    h, w = len(grid), len(grid[0])
    gx, gy = goal
    if not (0 <= gx < w and 0 <= gy < h):
        return None
    prev = {start: None}
    q = deque([start])
    while q:
        cur = q.popleft()
        if cur == goal:
            steps = []
            while prev[cur] is not None:
                p = prev[cur]
                steps.append((cur[0] - p[0], cur[1] - p[1]))
                cur = p
            return list(reversed(steps))
        cx, cy = cur
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = cx + dx, cy + dy
            if not (0 <= nx < w and 0 <= ny < h) or (nx, ny) in prev:
                continue

            if grid[ny][nx] or (nx, ny) == goal:
                prev[(nx, ny)] = cur
                q.append((nx, ny))
    return None

class Runner:
    def __init__(self, rom_path):
        from pyboy import PyBoy
        self.pb = PyBoy(rom_path, window="null", cgb=True)

    def mem(self, a):
        return self.pb.memory[a]

    def here(self):
        return (self.mem(WMAPGROUP), self.mem(WMAPNUMBER))

    def pos(self):
        return (self.mem(WXCOORD), self.mem(WYCOORD))

    def boot(self):
        seq = ["a", "a", "a", "start", "a", "a", "down", "a", "a", "b"]
        for i in range(4000):
            self.pb.button(seq[i % len(seq)], delay=2)
            self.pb.tick(10, False)
            if self.mem(WMAPGROUP):
                self.pb.tick(60, False)
                return True
        return False

    def step(self, direction, hold=12, settle=18):
        self.pb.button(direction, delay=hold)
        self.pb.tick(settle, False)

    def walk_to(self, tx, ty, walkable=None, budget=300):
        start_map = self.here()
        for _attempt in range(6):
            if self.here() != start_map:
                self.pb.tick(60, False)
                return True
            x, y = self.pos()
            if (x, y) == (tx, ty):

                self.step("up"); self.step("down")
                return self.here() != start_map
            path = bfs_path(walkable, (x, y), (tx, ty)) if walkable else None
            if not path:

                for _ in range(40):
                    x, y = self.pos()
                    if self.here() != start_map:
                        return True
                    if (x, y) == (tx, ty):
                        break
                    self.step("right" if tx > x else "left" if tx < x else
                              "down" if ty > y else "up")
                continue
            for (dx, dy) in path[:budget]:
                self.step({(1, 0): "right", (-1, 0): "left",
                           (0, 1): "down", (0, -1): "up"}[(dx, dy)])
                if self.here() != start_map:
                    self.pb.tick(60, False)
                    return True
        return self.here() != start_map

    def vram(self):
        return {b: bytes(self.pb.memory[b, 0x8000:0x9FFF]) for b in (0, 1)}

    def stop(self):
        self.pb.stop()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tileset", type=int)
    ap.add_argument("--map")
    ap.add_argument("--rom", default=os.path.join(PC_DIR, "pokecrystal.gbc"))
    ap.add_argument("--sym", default=os.path.join(PC_DIR, "pokecrystal.sym"))
    ap.add_argument("--out", default=os.path.join(ROOT, "generated", "romvram"))
    args = ap.parse_args()

    rom = Rom(args.rom, args.sym)
    id_to_name, name_to_id = M.build_map_table(rom)
    if args.map:
        if args.map not in name_to_id:
            sys.exit(f"unknown map {args.map!r}")
        g, m = name_to_id[args.map]
        want = M.read_map_header(rom, g, m)["tileset"]
    elif args.tileset is not None:
        want = args.tileset
    else:
        sys.exit("give --tileset N or --map Name")

    print("building the warp graph...")
    graph, tileset_of = build_graph(rom, id_to_name, name_to_id)
    print(f"  {len(graph)} maps, {sum(len(v) for v in graph.values())} warps")

    r = Runner(args.rom)
    if not r.boot():
        r.stop()
        sys.exit("never reached the overworld")
    start = r.here()
    print(f"start: {id_to_name.get(start, start)} tileset {r.mem(WMAPTILESET)}")

    steps = route_to(graph, tileset_of, start, want)
    if steps is None:
        r.stop()
        sys.exit(f"no warp route from {start} to a map with tileset {want}")
    print(f"route: {len(steps)} warp(s)")
    for (src, wx, wy, dest) in steps:
        print(f"  {id_to_name.get(src, src)} -> {id_to_name.get(dest, dest)} "
              f"via ({wx},{wy})")
        if not r.walk_to(wx, wy):
            print(f"    !! could not reach ({wx},{wy}) from {r.pos()}")
            break
        print(f"    now on {id_to_name.get(r.here(), r.here())} "
              f"tileset {r.mem(WMAPTILESET)}")

    got = r.mem(WMAPTILESET)
    if got != want:
        r.stop()
        sys.exit(f"ended on tileset {got}, wanted {want}")
    r.pb.tick(120, False)
    vram = r.vram()
    r.stop()

    os.makedirs(args.out, exist_ok=True)
    for b in (0, 1):
        p = os.path.join(args.out, f"tileset{want}_vram{b}.bin")
        open(p, "wb").write(vram[b])
        print(f"wrote {p}")

    ts = M.read_tileset_entry(rom, want)
    gfx = [bytes(x) for x in M.read_tileset_gfx(rom, *ts["gfx"])]
    slots = {}
    for b in (0, 1):
        for s in range(0x1FF):
            slots.setdefault(vram[b][s * 16:(s + 1) * 16], []).append((b, s))
    prev = None
    print(f"\ntileset {want}: {len(gfx)} decompressed tiles")
    for i, tile in enumerate(gfx):
        w = slots.get(tile, [])
        cur = w[0] if w else None
        if prev is None or (cur is None) != (prev is None) or (
                cur and prev and (cur[0] != prev[0] or cur[1] != prev[1] + 1)):
            d = "NOT IN VRAM" if cur is None else \
                f"bank {cur[0]} slot ${cur[1]:03X} (${0x8000 + cur[1] * 16:04X})"
            print(f"  rom tile {i:3d}: {d}")
        prev = cur
    return 0

if __name__ == "__main__":
    sys.exit(main())
