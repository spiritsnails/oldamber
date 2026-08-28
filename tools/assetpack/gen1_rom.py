
import hashlib
import re
import sys
from pathlib import Path

KNOWN_ROMS = {
    "ea9bcae617fdf159b045185467ae58b2e4a48b9a": "Pokemon Red (UE) [S][!]",
    "d7037c83e1ae5b39bde3c30787637ba1d4c48ce2": "Pokemon Blue (UE) [S][!]",
}

ROM_SYMS = {
    "ea9bcae617fdf159b045185467ae58b2e4a48b9a": "pokered.sym",
    "d7037c83e1ae5b39bde3c30787637ba1d4c48ce2": "pokeblue.sym",
}

SYM_RE = re.compile(r"^\s*([0-9A-Fa-f]{2,4}):([0-9A-Fa-f]{4})\s+(\S+)")

class RomError(Exception):
    pass

class Gen1Rom:

    def __init__(self, rom_path, sym_path, allow_unknown=False):
        self.rom_path = Path(rom_path)
        self.sym_path = Path(sym_path)

        if not self.rom_path.is_file():
            raise RomError(f"ROM not found: {self.rom_path}")
        self.data = self.rom_path.read_bytes()

        self.sha1 = hashlib.sha1(self.data).hexdigest()
        self.title = KNOWN_ROMS.get(self.sha1)
        if self.title is None:
            if not allow_unknown:
                known = "\n".join(f"    {h}  {n}" for h, n in KNOWN_ROMS.items())
                raise RomError(
                    f"Unrecognised ROM: {self.rom_path}\n"
                    f"  sha1 {self.sha1}\n"
                    f"This is not a known retail release. Accepted:\n{known}\n"
                    f"(Use --allow-unknown-rom only for development.)"
                )
            self.title = f"UNVERIFIED ({self.sha1})"

        want = ROM_SYMS.get(self.sha1)
        if want and self.sym_path.name != want:
            alt = self.sym_path.parent / want
            if not alt.is_file():
                raise RomError(
                    f"{self.title} needs {want}, and it is not next to "
                    f"{self.sym_path}.\n"
                    f"  Using {self.sym_path.name} instead would read this ROM "
                    f"at another build's addresses.\n"
                    f"Build the disassembly to produce one:  "
                    f"cd pokered-master && make"
                )
            print(f"sym   {self.sym_path.name} -> {want} (matches {self.title})")
            self.sym_path = alt

        if not self.sym_path.is_file():
            raise RomError(
                f"Symbol file not found: {self.sym_path}\n"
                f"Build the disassembly to produce one:  cd pokered-master && make"
            )
        self.sym = {}
        for line in self.sym_path.read_text(encoding="utf-8", errors="replace").splitlines():
            m = SYM_RE.match(line)
            if m:

                self.sym.setdefault(m.group(3), (int(m.group(1), 16), int(m.group(2), 16)))

    def offset(self, name):
        if name not in self.sym:
            raise RomError(f"symbol '{name}' not in {self.sym_path}")
        bank, addr = self.sym[name]
        return addr if addr < 0x4000 else bank * 0x4000 + (addr - 0x4000)

    def read(self, name_or_off, length, extra=0):
        off = self.offset(name_or_off) if isinstance(name_or_off, str) else name_or_off
        off += extra
        if off < 0 or off + length > len(self.data):
            raise RomError(f"read of {length} bytes at {off:#x} is outside the ROM")
        return self.data[off:off + length]

    def require(self, *names):
        missing = [n for n in names if n not in self.sym]
        if missing:
            raise RomError(
                f"{self.sym_path} is missing symbols: {', '.join(missing)}\n"
                f"  is this really a pokered .sym?"
            )

def default_paths(repo_root):
    d = Path(repo_root) / "pokered-master"
    return d / "pokered.gbc", d / "pokered.sym"

def open_rom(args, repo_root):
    rom, sym = default_paths(repo_root)
    try:
        r = Gen1Rom(args.rom or rom, args.sym or sym,
                    allow_unknown=getattr(args, "allow_unknown_rom", False))
    except RomError as e:
        sys.exit(f"error: {e}")
    return r
