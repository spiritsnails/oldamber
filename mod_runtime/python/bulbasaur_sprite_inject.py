
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except Exception as e:
    print(f"say \"Pillow missing: {e}\"@")
    raise SystemExit(0)

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "mod_runtime" / "generated" / "bulbasaur_front_2bpp.bin"

NATIVE_W = 40
NATIVE_H = 40
CANVAS_W = 56
CANVAS_H = 56

def gray_to_idx(v: int) -> int:

    if v >= 192:
        return 0
    if v >= 128:
        return 1
    if v >= 64:
        return 2
    return 3

def make_synthetic_frame() -> Image.Image:
    g = Image.new("L", (NATIVE_W, NATIVE_H), 255)
    d = ImageDraw.Draw(g)
    for i in range(0, NATIVE_W, 8):
        d.line((i, 0, i, NATIVE_H), fill=192)
        d.line((0, i, NATIVE_W, i), fill=192)
    d.polygon([(20, 4), (36, 20), (20, 36), (4, 20)], fill=96)

    canvas = Image.new("L", (CANVAS_W, CANVAS_H), 255)
    ox = (CANVAS_W - NATIVE_W) // 2
    oy = (CANVAS_H - NATIVE_H) // 2
    canvas.paste(g, (ox, oy))
    return canvas

def main() -> int:
    im = make_synthetic_frame()
    px = im.load()
    out = bytearray()

    for ty in range(7):
        for tx in range(7):
            for row in range(8):
                b0 = 0
                b1 = 0
                y = ty * 8 + row
                for col in range(8):
                    x = tx * 8 + col
                    idx = gray_to_idx(px[x, y])
                    bit = 7 - col
                    b0 |= (idx & 1) << bit
                    b1 |= ((idx >> 1) & 1) << bit
                out.append(b0)
                out.append(b1)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(out)
    rel = OUT.relative_to(ROOT).as_posix()
    print(f"sprite_front_load BULBASAUR {rel}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
