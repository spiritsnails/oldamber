
import argparse
import hashlib
from pathlib import Path

PLATFORMS = {
    "windows-x64": "OldAmber-{version}-windows-x64-update.tar.gz",
    "linux-x64": "OldAmber-{version}-linux-x64-update.tar.gz",
    "macos-universal": "OldAmber-{version}-macos-universal-update.tar.gz",
}

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", default=(repo / "VERSION").read_text().strip())
    parser.add_argument("--artifacts", type=Path, default=repo / "build")
    parser.add_argument("--output", type=Path, default=repo / "build" / "oldamber-update.txt")
    args = parser.parse_args()

    rows = ["schema=1", f"version={args.version}", "minimum-bootstrap=1"]
    for platform, pattern in PLATFORMS.items():
        name = pattern.format(version=args.version)
        path = args.artifacts / name
        if not path.is_file():
            raise SystemExit(f"missing update payload: {path}")
        url = f"https://github.com/spiritsnails/oldamber/releases/download/v{args.version}/{name}"
        rows.extend([
            f"{platform}.url={url}",
            f"{platform}.sha256={digest(path)}",
            f"{platform}.size={path.stat().st_size}",
        ])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(rows) + "\n", newline="\n")
    print(args.output)

if __name__ == "__main__":
    main()
