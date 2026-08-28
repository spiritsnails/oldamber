
import hashlib
import os
import sys
import traceback

BUNDLE = getattr(sys, "_MEIPASS", None)
REPO = BUNDLE or os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))

sys.path.insert(0, os.path.join(REPO, "tools", "assetpack"))
sys.path.insert(0, os.path.join(REPO, "tools", "romimport"))

def _take_dest_flag():
    argv = sys.argv[1:]
    for i, a in enumerate(argv):
        if a == "--dest" and i + 1 < len(argv):
            sys.argv = [sys.argv[0]] + argv[:i] + argv[i + 2:]
            return argv[i + 1]
        if a.startswith("--dest="):
            sys.argv = [sys.argv[0]] + argv[:i] + argv[i + 1:]
            return a[len("--dest="):]
    return None

_DEST_OVERRIDE = _take_dest_flag()
DEST = _DEST_OVERRIDE or (
    os.path.dirname(sys.executable) if getattr(sys, "frozen", False)
    else os.getcwd())
if _DEST_OVERRIDE:
    os.makedirs(DEST, exist_ok=True)

SYM = os.path.join(REPO, "pokered-master", "pokered.sym")

TITLE = "Pokemon Red (port) - first-run setup"

def _take_no_dialog_flag():
    if os.environ.get("OLDAMBER_SETUP_NO_DIALOG"):
        return True
    if "--no-dialog" in sys.argv[1:]:

        sys.argv = [sys.argv[0]] + [a for a in sys.argv[1:] if a != "--no-dialog"]
        return True
    return False

NO_DIALOG = _take_no_dialog_flag()

def log(msg=""):
    print(msg, flush=True)

def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def pick_rom():
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox
    except Exception:
        return None
    root = tk.Tk()
    root.withdraw()
    messagebox.showinfo(
        TITLE,
        "This build ships no game data.\n\n"
        "Select your own legally obtained Pokemon Red ROM and this will "
        "build the assets the game needs. It takes about a minute.\n\n"
        "Your ROM is only read -- it is not copied or modified.")
    path = filedialog.askopenfilename(
        title="Select your Pokemon Red ROM",
        filetypes=[("Game Boy ROM", "*.gb *.gbc"), ("All files", "*.*")])
    root.destroy()
    return path or None

def show_dialog(msg, error=False):

    if NO_DIALOG:
        return 0

    try:
        import tkinter as tk
        from tkinter import messagebox
        root = tk.Tk()
        root.withdraw()
        (messagebox.showerror if error else messagebox.showinfo)(TITLE, msg)
        root.destroy()
        return 1
    except Exception:
        pass

    import shutil
    import subprocess

    osa = ('display dialog (item 1 of argv) with title "' + TITLE + '"'
           ' buttons {"OK"} default button "OK"'
           + (" with icon stop" if error else ""))
    for argv in (
        ["osascript", "-e", "on run argv", "-e", osa, "-e", "end run", msg],
        ["kdialog", "--error" if error else "--msgbox", msg, "--title", TITLE],
        ["zenity", "--error" if error else "--info", "--text", msg, "--title", TITLE],
        ["xmessage", "-center", msg],
    ):
        if not shutil.which(argv[0]):
            continue
        try:
            subprocess.run(argv, check=False, timeout=600)
            return 1
        except Exception:
            continue
    return 0

def fail(msg):
    log()
    log("SETUP FAILED")
    log(msg)

    if not show_dialog(msg, error=True):

        try:
            report = os.path.join(DEST, "setup-failed.txt")
            with open(report, "w", encoding="utf-8") as fh:
                fh.write("OldAmber setup failed.\n\n" + msg + "\n")
            log("Written to: %s" % report)
        except Exception:
            pass

    try:
        if sys.stdin and sys.stdin.isatty():
            input("\nPress Enter to close...")
    except Exception:
        pass
    return 1

def done(msg):
    log()
    log(msg)
    show_dialog(msg)
    return 0

def main():
    rom = sys.argv[1] if len(sys.argv) > 1 else pick_rom()
    if not rom:
        return fail("No ROM selected. Run this again and pick your Pokemon "
                    "Red ROM file (usually a .gb file).")
    if not os.path.isfile(rom):
        return fail(f"That file does not exist:\n{rom}")

    log(f"ROM : {rom}")
    log(f"SHA1: {sha1(rom)}")
    log(f"Out : {DEST}")
    log()

    from gen1_rom import Gen1Rom, RomError
    try:
        Gen1Rom(rom, SYM)
    except RomError as e:
        return fail(
            "That file was not recognised as a supported Pokemon Red ROM.\n\n"
            f"{e}\n\n"
            "It must be an unmodified Pokemon Red (UE) ROM. Note that Blue "
            "and Yellow will not work, and neither will a patched or "
            "trimmed dump.")
    except Exception as e:
        return fail(f"Could not read that ROM:\n{e}")

    log("[1/4] Building assets.pak ...")
    import build_pak

    pkg = getattr(build_pak, "ROM_PACKAGE_ID", {}).get(sha1(rom), "red")

    pkg_dir = os.path.join(DEST, "packages", pkg)
    pak = os.path.join(DEST, "assets.pak")
    sys.argv = ["build_pak", "--rom", rom, "--sym", SYM, "--out", pak,
                "--packages-dir", pkg_dir]
    try:
        build_pak.main()
    except SystemExit as e:
        if e.code:
            return fail(f"Building assets.pak failed (code {e.code}).")

    log()
    log("[2/4] Building map tile art (this is the slow part) ...")
    import emit_kanto
    art_root = os.path.join(DEST, "mod_runtime", "custom_art", "kanto")
    os.makedirs(art_root, exist_ok=True)

    sys.argv = ["emit_kanto", "--rom", rom, "--sym", SYM,
                "--art-all", "--art-out", art_root]
    try:
        emit_kanto.main()
    except SystemExit as e:
        if e.code:
            return fail(f"Building tile art failed (code {e.code}).")

    n = sum(len(f) for _, _, f in os.walk(art_root))

    gen_root = os.path.join(DEST, "mod_runtime", "generatedmaps")
    pkg_root = os.path.join(gen_root, pkg)
    os.makedirs(pkg_root, exist_ok=True)

    log()
    log("[3/4] Building maps ...")
    sys.argv = ["emit_kanto", "--rom", rom, "--sym", SYM,
                "--all", "--all-out", pkg_root]
    try:
        emit_kanto.main()
    except SystemExit as e:
        if e.code:
            return fail(f"Building maps failed (code {e.code}).")

    log()
    log("[4/4] Building the text table ...")
    import emit_scene_text
    sys.argv = ["emit_scene_text", "--rom", rom, "--sym", SYM,
                "--out", os.path.join(pkg_root, "scene_text.tbl")]
    try:
        emit_scene_text.main()
    except SystemExit as e:
        if e.code:
            return fail(f"Building the text table failed (code {e.code}).")

    blocks = len([f for _, _, fs in os.walk(gen_root) for f in fs
                  if f.endswith(".block")])
    tbl = os.path.join(pkg_root, "scene_text.tbl")

    if not os.path.isfile(pak):
        return fail("assets.pak was not produced. Setup did not complete.")

    if blocks < 200:
        return fail(f"Only {blocks} maps were built (expected 200+).\n"
                    "The game would render maps as garbage. Setup did not complete.")
    if not os.path.isfile(tbl):
        return fail("The ROM text table was not produced.\n"
                    "Every text box in the game would be blank. "
                    "Setup did not complete.")

    return done(
        "Setup complete.\n\n"
        f"assets.pak   {os.path.getsize(pak) // 1024} KB\n"
        f"tile art     {n} files\n"
        f"maps         {blocks} files\n"
        f"text table   {os.path.getsize(tbl) // 1024} KB\n\n"
        "You can now start the game.\n"
        "You only need to run this setup once.")

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        traceback.print_exc()
        sys.exit(fail("Something went wrong. The details above will mean "
                      "something to whoever sent you this build."))
