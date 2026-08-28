
import os
import re
import sys

PAT = re.compile(r'REPO\s*(?:/|,)\s*((?:"[^"]*"\s*(?:/|,)\s*)*"[^"]*")')

SKIP_TOP = ("pokered-master", "generated", "packages", "build", "docs",
            "reference", "archive")
SKIP_PREFIX = ("mod_runtime/generatedmaps/", "mod_runtime/custom_art/")
SKIP_SUFFIX = (".pak", ".gb", ".gbc", ".sav", ".sym")

def shippable(rel):
    if rel.split("/", 1)[0] in SKIP_TOP:
        return False
    if rel.startswith(SKIP_PREFIX):
        return False
    if rel.lower().endswith(SKIP_SUFFIX):
        return False
    return True

def scan(repo):
    found = set()
    for base, _dirs, files in os.walk(os.path.join(repo, "tools")):
        for name in files:
            if not name.endswith(".py"):
                continue
            try:
                text = open(os.path.join(base, name),
                            encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            for m in PAT.finditer(text):
                parts = re.findall(r'"([^"]*)"', m.group(1))
                rel = "/".join(p.strip("/") for p in parts if p)

                if not os.path.isfile(os.path.join(repo, rel)):
                    continue
                if not shippable(rel):
                    continue
                found.add(rel)
    return sorted(found)

def main():

    try:
        sys.stdout.reconfigure(newline="\n")
    except AttributeError:
        pass

    repo = sys.argv[1] if len(sys.argv) > 1 else "."
    hits = scan(repo)
    if not hits:

        sys.exit("no extractor source reads found under %s/tools -- scan is broken"
                 % repo)
    print("\n".join(hits))

if __name__ == "__main__":
    main()
