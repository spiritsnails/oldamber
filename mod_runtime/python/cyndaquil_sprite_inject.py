
from pathlib import Path
import warnings

try:
    from PIL import Image, ImageDraw
except Exception as e:
    print(f'say "Pillow missing: {e}"@')
    raise SystemExit(0)

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "mod_runtime" / "generated"
FRONT_OUT = OUT_DIR / "cyndaquil_front_2bpp.bin"
BACK_OUT = OUT_DIR / "cyndaquil_back_2bpp.bin"

CANVAS = 56

def gray_to_idx(v: int) -> int:
    if v >= 192:
        return 0
    if v >= 128:
        return 1
    if v >= 64:
        return 2
    return 3

def make_synthetic_frame(seed: int) -> Image.Image:
    g = Image.new("L", (40, 40), 255)
    d = ImageDraw.Draw(g)
    for r in range(18, 0, -4):
        shade = 255 - ((r + seed * 7) % 4) * 64
        d.ellipse((20 - r, 20 - r, 20 + r, 20 + r), fill=shade)
    canvas = Image.new("L", (CANVAS, CANVAS), 255)
    canvas.paste(g, ((CANVAS - 40) // 2, (CANVAS - 40) // 2))
    return canvas

def encode_2bpp(im: Image.Image) -> bytes:
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
    return bytes(out)

def main() -> int:
    warnings.filterwarnings("ignore", category=UserWarning, module="PIL.Image")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    FRONT_OUT.write_bytes(encode_2bpp(make_synthetic_frame(seed=0)))
    BACK_OUT.write_bytes(encode_2bpp(make_synthetic_frame(seed=1)))
    print(f"sprite_front_load 0xF0 {FRONT_OUT.relative_to(ROOT).as_posix()}")
    print(f"sprite_back_load 0xF0 {BACK_OUT.relative_to(ROOT).as_posix()}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
