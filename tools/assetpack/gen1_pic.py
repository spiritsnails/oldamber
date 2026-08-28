
LENGTH_OFFSETS = [(1 << (n + 1)) - 1 for n in range(16)]

DECODE_0 = [0x0, 0x1, 0x3, 0x2, 0x7, 0x6, 0x4, 0x5,
            0xF, 0xE, 0xC, 0xD, 0x8, 0x9, 0xB, 0xA]
DECODE_1 = [0xF, 0xE, 0xC, 0xD, 0x8, 0x9, 0xB, 0xA,
            0x0, 0x1, 0x3, 0x2, 0x7, 0x6, 0x4, 0x5]

class _BitReader:

    def __init__(self, data, pos):
        self.data = data
        self.pos = pos

    def bit(self):
        byte = self.data[self.pos >> 3]
        b = (byte >> (7 - (self.pos & 7))) & 1
        self.pos += 1
        return b

    def bits(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | self.bit()
        return v

def _decompress_chunk(br, width, height):
    cols = width // 8
    buf = bytearray(cols * height)

    col = 0
    row = 0
    bit_off = 3

    def write_pair(v):

        buf[col * height + row] |= (v & 3) << (bit_off * 2)

    def advance():
        nonlocal col, row, bit_off
        row += 1
        if row != height:
            return True
        row = 0
        if bit_off != 0:
            bit_off -= 1
            return True
        bit_off = 3
        col += 1
        return col != cols

    mode_zeros = (br.bit() == 0)

    while True:
        if mode_zeros:

            ones = 0
            while br.bit():
                ones += 1
            value = br.bits(ones + 1)
            count = LENGTH_OFFSETS[ones] + value
            for _ in range(count):

                if not advance():
                    return buf, cols
            mode_zeros = False
            continue

        v = br.bits(2)
        if v == 0:
            mode_zeros = True
            continue
        write_pair(v)
        if not advance():
            return buf, cols

def _differential_decode(buf, cols, height, flipped=False):
    for row in range(height):
        last = 0
        for col in range(cols):
            i = col * height + row
            byte = buf[i]
            hi = _decode_nybble(byte >> 4, last, flipped)
            last = hi
            lo = _decode_nybble(byte & 0xF, last, flipped)
            last = lo
            buf[i] = (hi << 4) | lo
    return buf

def _decode_nybble(nybble, last, flipped):

    bit = (last >> 3) & 1 if flipped else last & 1
    table = DECODE_1 if bit else DECODE_0
    return table[nybble]

def _xor_chunks(dst, src):
    for i in range(len(dst)):
        dst[i] ^= src[i]
    return dst

def decompress(data, offset):
    br = _BitReader(data, offset * 8)

    size = br.bits(8)
    w_tiles = (size >> 4) & 0xF
    h_tiles = size & 0xF
    width = w_tiles * 8
    height = h_tiles * 8

    swap = br.bit() == 1

    first, cols = _decompress_chunk(br, width, height)

    if br.bit() == 0:
        mode = 0
    else:
        mode = 1 + br.bit()

    second, _ = _decompress_chunk(br, width, height)

    if mode == 0:
        _differential_decode(first, cols, height)
        _differential_decode(second, cols, height)
    elif mode == 1:

        _differential_decode(first, cols, height)
        _xor_chunks(second, first)
    else:

        _differential_decode(second, cols, height)
        _differential_decode(first, cols, height)
        _xor_chunks(second, first)

    if swap:
        buf1, buf2 = second, first
    else:
        buf1, buf2 = first, second

    out = bytearray()
    for i in range(len(buf1)):
        out.append(buf1[i])
        out.append(buf2[i])

    tiles = [bytes(out[i * 16:(i + 1) * 16]) for i in range(len(out) // 16)]
    row_major = b"".join(tiles[c * h_tiles + r]
                         for r in range(h_tiles)
                         for c in range(w_tiles))
    return row_major, w_tiles, h_tiles

def _tiles_to_pixels(data, w_tiles, h_tiles):
    w, h = w_tiles * 8, h_tiles * 8
    px = [[0] * w for _ in range(h)]
    for t in range(w_tiles * h_tiles):
        tx, ty = (t % w_tiles) * 8, (t // w_tiles) * 8
        for row in range(8):
            p0 = data[t * 16 + row * 2]
            p1 = data[t * 16 + row * 2 + 1]
            for bit in range(8):
                m = 0x80 >> bit
                px[ty + row][tx + bit] = ((1 if p0 & m else 0) |
                                          (2 if p1 & m else 0))
    return px

def _pixels_to_tiles(px, w_tiles, h_tiles):
    out = bytearray()
    for ty in range(h_tiles):
        for tx in range(w_tiles):
            for row in range(8):
                p0 = p1 = 0
                for bit in range(8):
                    v = px[ty * 8 + row][tx * 8 + bit]
                    m = 0x80 >> bit
                    if v & 1:
                        p0 |= m
                    if v & 2:
                        p1 |= m
                out.append(p0)
                out.append(p1)
    return bytes(out)

def scale_by_two(data, w_tiles=4, h_tiles=4):
    src = _tiles_to_pixels(data, w_tiles, h_tiles)
    cw, ch = w_tiles * 8 - 4, h_tiles * 8 - 4
    dst = [[0] * (cw * 2) for _ in range(ch * 2)]
    for y in range(ch):
        for x in range(cw):
            v = src[y][x]
            dst[y * 2][x * 2] = dst[y * 2][x * 2 + 1] = v
            dst[y * 2 + 1][x * 2] = dst[y * 2 + 1][x * 2 + 1] = v
    return _pixels_to_tiles(dst, cw * 2 // 8, ch * 2 // 8)

def to_canvas(tiles, w_tiles, h_tiles, canvas=7):
    if w_tiles > canvas or h_tiles > canvas:
        raise ValueError(f"pic {w_tiles}x{h_tiles} exceeds {canvas}x{canvas} canvas")
    blank = bytes(16)
    src = [tiles[i * 16:(i + 1) * 16] for i in range(len(tiles) // 16)]
    dy = canvas - h_tiles
    dx = (canvas - w_tiles + 1) // 2
    out = bytearray()
    for r in range(canvas):
        for c in range(canvas):
            if dy <= r < dy + h_tiles and dx <= c < dx + w_tiles:
                out += src[(r - dy) * w_tiles + (c - dx)]
            else:
                out += blank
    return bytes(out)
