
import struct

MAGIC = b"PKRDPAK\0"
VERSION = 1

HEADER_FMT = "<8sII20sIIIIIII"
ENTRY_FMT = "<QIIIIII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
ENTRY_SIZE = struct.calcsize(ENTRY_FMT)

assert HEADER_SIZE == 64, HEADER_SIZE
assert ENTRY_SIZE == 32, ENTRY_SIZE

def fnv1a64(s):
    h = 1469598103934665603
    for b in s.encode("ascii"):
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h

class PakBuilder:
    def __init__(self, rom_sha1_hex):
        self.rom_sha1 = bytes.fromhex(rom_sha1_hex)
        if len(self.rom_sha1) != 20:
            raise ValueError("rom_sha1 must be 20 bytes")
        self._entries = {}

    def add(self, name, data, count=None, stride=None):
        data = bytes(data)
        if name in self._entries:
            raise ValueError(f"duplicate asset name: {name}")
        if stride is None:
            stride = 1
        if count is None:
            if len(data) % stride:
                raise ValueError(
                    f"{name}: size {len(data)} is not a multiple of stride {stride}")
            count = len(data) // stride
        if stride != 0 and count * stride != len(data):
            raise ValueError(
                f"{name}: count {count} * stride {stride} != size {len(data)}")
        self._entries[name] = (data, count, stride)

    def __len__(self):
        return len(self._entries)

    def build(self):

        names = sorted(self._entries, key=lambda n: (fnv1a64(n), n))

        name_blob = bytearray()
        name_offs = {}
        for n in names:
            name_offs[n] = len(name_blob)
            name_blob += n.encode("ascii") + b"\0"

        data_blob = bytearray()
        data_offs = {}
        for n in names:
            blob = self._entries[n][0]

            while len(data_blob) % 4:
                data_blob.append(0)
            data_offs[n] = len(data_blob)
            data_blob += blob

        index_off = HEADER_SIZE
        names_off = index_off + ENTRY_SIZE * len(names)
        data_off = names_off + len(name_blob)
        while data_off % 4:
            name_blob.append(0)
            data_off += 1

        header = struct.pack(
            HEADER_FMT, MAGIC, VERSION, len(names), self.rom_sha1,
            index_off, names_off, len(name_blob), data_off, len(data_blob), 0, 0)

        index = bytearray()
        for n in names:
            blob, count, stride = self._entries[n]
            index += struct.pack(ENTRY_FMT, fnv1a64(n), name_offs[n],
                                 data_offs[n], len(blob), count, stride, 0)

        out = bytearray()
        out += header
        out += index
        out += name_blob
        out += data_blob
        assert len(out) == data_off + len(data_blob)
        return bytes(out)

    def write(self, path):
        blob = self.build()
        with open(path, "wb") as fh:
            fh.write(blob)
        return len(blob)
