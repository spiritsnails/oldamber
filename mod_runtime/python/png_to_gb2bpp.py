
from __future__ import annotations

import struct
import sys
import zlib

TILE_SIZE = 8
BLOCK_SIZE = 16

def decode_png_2bpp_grayscale(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError("not a PNG file")

    idat_chunks = []
    i = 8
    width = height = bit_depth = 0
    while i < len(data):
        length = struct.unpack('>I', data[i:i + 4])[0]
        chunk_type = data[i + 4:i + 8]
        chunk_data = data[i + 8:i + 8 + length]
        if chunk_type == b'IHDR':
            width, height = struct.unpack('>II', chunk_data[:8])
            bit_depth = chunk_data[8]
            color_type = chunk_data[9]
            if bit_depth != 2 or color_type != 0:
                raise ValueError(
                    f"expected 2bpp grayscale PNG, got bit_depth={bit_depth} color_type={color_type}")
        elif chunk_type == b'IDAT':
            idat_chunks.append(chunk_data)
        i += 12 + length

    raw = zlib.decompress(b''.join(idat_chunks))

    stride = (width * 2 + 7) // 8
    pixels = []
    prev_row = bytes(stride)
    pos = 0

    for _ in range(height):
        filt = raw[pos]; pos += 1
        row_raw = bytearray(raw[pos:pos + stride]); pos += stride

        if filt == 0:
            pass
        elif filt == 1:
            for j in range(1, stride):
                row_raw[j] = (row_raw[j] + row_raw[j - 1]) & 0xFF
        elif filt == 2:
            for j in range(stride):
                row_raw[j] = (row_raw[j] + prev_row[j]) & 0xFF
        elif filt == 3:
            for j in range(stride):
                a = row_raw[j - 1] if j > 0 else 0
                b = prev_row[j]
                row_raw[j] = (row_raw[j] + (a + b) // 2) & 0xFF
        elif filt == 4:
            for j in range(stride):
                a = row_raw[j - 1] if j > 0 else 0
                b = prev_row[j]
                c = prev_row[j - 1] if j > 0 else 0
                pa = abs(b - c); pb = abs(a - c); pc = abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                row_raw[j] = (row_raw[j] + pr) & 0xFF
        else:
            raise ValueError(f"unknown PNG filter type {filt}")

        prev_row = bytes(row_raw)

        for byte in row_raw:
            for shift in (6, 4, 2, 0):
                pixels.append((byte >> shift) & 3)

    return width, height, pixels[:width * height]

def pixels_to_gb2bpp(width, height, pixels, invert=True):
    tiles_x = width // 8
    tiles_y = height // 8
    out = bytearray()

    for ty in range(tiles_y):
        for tx in range(tiles_x):
            for row in range(8):
                lo = 0; hi = 0
                for col in range(8):
                    pv = pixels[(ty * 8 + row) * width + (tx * 8 + col)]
                    if invert:
                        pv = 3 - pv
                    bit = 7 - col
                    if pv & 1: lo |= (1 << bit)
                    if pv & 2: hi |= (1 << bit)
                out.append(lo)
                out.append(hi)
    return bytes(out)

def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    in_path, out_path = sys.argv[1], sys.argv[2]

    try:
        width, height, pixels = decode_png_2bpp_grayscale(in_path)
    except Exception as exc:
        print(f"[png_to_gb2bpp] failed to decode '{in_path}': {exc}", file=sys.stderr)
        return 3

    if (width, height) not in ((TILE_SIZE, TILE_SIZE), (BLOCK_SIZE, BLOCK_SIZE)):
        print(f"[png_to_gb2bpp] expected {TILE_SIZE}x{TILE_SIZE}px (single tile) or "
              f"{BLOCK_SIZE}x{BLOCK_SIZE}px (4-tile asset), got {width}x{height}px ('{in_path}')",
              file=sys.stderr)
        return 3

    blob = pixels_to_gb2bpp(width, height, pixels)
    with open(out_path, "wb") as f:
        f.write(blob)

    print(f"[png_to_gb2bpp] {in_path} -> {out_path} ({len(blob)} bytes)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
