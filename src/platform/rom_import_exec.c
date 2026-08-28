
#include "rom_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char kNintendoLogo[48] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

int RomImport_LooksLikeGBRom(const char *path) {
    FILE *f = fopen(path, "rb");
    int ok = 0;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);
        if (size >= 0x8000 && (size % 0x8000) == 0) {
            unsigned char logo[48];
            if (fseek(f, 0x104, SEEK_SET) == 0 &&
                fread(logo, 1, sizeof logo, f) == sizeof logo &&
                memcmp(logo, kNintendoLogo, sizeof logo) == 0) {
                ok = 1;
            }
        }
    }
    fclose(f);
    return ok;
}

#ifdef _WIN32
#define EXEC_POPEN  _popen
#define EXEC_PCLOSE _pclose
#else
#define EXEC_POPEN  popen
#define EXEC_PCLOSE pclose
#endif

static size_t trailing_backslashes(const char *s) {
    size_t n = 0, len = strlen(s);
    while (n < len && s[len - 1 - n] == '\\') n++;
    return n;
}

static void sh_quote(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    if (!out_sz) return;
#ifdef _WIN32
    {
        size_t extra = trailing_backslashes(in);
        if (o + 1 < out_sz) out[o++] = '"';
        for (const char *p = in; *p && o + 2 < out_sz; p++) {
            if (*p == '"') out[o++] = '\\';
            out[o++] = *p;
        }
        while (extra-- && o + 2 < out_sz) out[o++] = '\\';
    }
    if (o + 1 < out_sz) out[o++] = '"';
#else
    if (o + 1 < out_sz) out[o++] = '\'';
    for (const char *p = in; *p && o + 6 < out_sz; p++) {
        if (*p == '\'') {
            out[o++] = '\''; out[o++] = '"'; out[o++] = '\'';
            out[o++] = '"';  out[o++] = '\'';
        } else {
            out[o++] = *p;
        }
    }
    if (o + 1 < out_sz) out[o++] = '\'';
#endif
    out[o] = '\0';
}

static const char *python_runner(void) {
    static char found[256];
    static int tried = 0;
    if (tried) return found[0] ? found : NULL;
    tried = 1;
    found[0] = '\0';
    {
#ifdef _WIN32
        static const char *kCandidates[] = {
            "C:/Progra~1/Python311/python.exe", NULL };
        const char *probe_cmd = "where python 2>nul";
#else
        static const char *kCandidates[] = {
            "/usr/bin/python3", "/usr/local/bin/python3", "/bin/python3", NULL };
        const char *probe_cmd = "command -v python3 2>/dev/null";
#endif
        for (int i = 0; kCandidates[i]; i++) {
            FILE *p = fopen(kCandidates[i], "r");
            if (p) {
                fclose(p);
                snprintf(found, sizeof found, "%s", kCandidates[i]);
                return found;
            }
        }
        {
            FILE *w = EXEC_POPEN(probe_cmd, "r");
            if (w) {
                if (fgets(found, (int)sizeof found, w)) {
                    size_t n = strlen(found);
                    while (n && (found[n-1] == '\n' || found[n-1] == '\r'))
                        found[--n] = '\0';
                }
                EXEC_PCLOSE(w);
            }
        }
    }
    return found[0] ? found : NULL;
}

static int run_script(const char *script, const char *const *args, int nargs,
                      char *err, size_t errsz) {
    const char *py = python_runner();
    char cmd[2048], q[1024], tail[512] = "";
    FILE *fp;
    int rc;

    if (!py) {
        snprintf(err, errsz, "python3 not found on this system");
        return 0;
    }

    sh_quote(py, q, sizeof q);
    snprintf(cmd, sizeof cmd, "%s", q);
    sh_quote(script, q, sizeof q);
    strncat(cmd, " ", sizeof cmd - strlen(cmd) - 1);
    strncat(cmd, q, sizeof cmd - strlen(cmd) - 1);
    for (int i = 0; i < nargs; i++) {
        sh_quote(args[i], q, sizeof q);
        strncat(cmd, " ", sizeof cmd - strlen(cmd) - 1);
        strncat(cmd, q, sizeof cmd - strlen(cmd) - 1);
    }
    strncat(cmd, " 2>&1", sizeof cmd - strlen(cmd) - 1);

    fp = EXEC_POPEN(cmd, "r");
    if (!fp) {
        snprintf(err, errsz, "could not run %s", script);
        return 0;
    }
    {
        char line[256];
        while (fgets(line, sizeof line, fp)) {
            printf("%s", line);
            snprintf(tail, sizeof tail, "%s", line);
        }
    }
    fflush(stdout);
    rc = EXEC_PCLOSE(fp);
    if (rc != 0) {
        size_t n = strlen(tail);
        while (n && (tail[n-1] == '\n' || tail[n-1] == '\r')) tail[--n] = '\0';
        snprintf(err, errsz, "%s failed: %s", script, tail[0] ? tail : "(no output)");
        return 0;
    }
    return 1;
}

static void join(char *out, size_t outsz, const char *dir, const char *file) {
    size_t n = strlen(dir);
    if (n && (dir[n-1] == '/' || dir[n-1] == '\\'))
        snprintf(out, outsz, "%s%s", dir, file);
    else
        snprintf(out, outsz, "%s/%s", dir, file);
}

int RomImport_BuildPak(const char *rom_path, const char *tools_dir,
                       const char *out_pak_path, char *err, size_t errsz) {
    char script[512];
    const char *args[4];
    if (err && errsz) err[0] = '\0';
    join(script, sizeof script, tools_dir, "build_pak.py");
    args[0] = "--rom"; args[1] = rom_path;
    args[2] = "--out"; args[3] = out_pak_path;
    return run_script(script, args, 4, err, errsz);
}

int RomImport_EmitKantoMaps(const char *rom_path, const char *romimport_tools_dir,
                            char *err, size_t errsz) {
    char script[512];
    const char *art[3], *all[3], *text[2];
    if (err && errsz) err[0] = '\0';

    join(script, sizeof script, romimport_tools_dir, "emit_kanto.py");
    art[0] = "--rom"; art[1] = rom_path; art[2] = "--art-all";
    if (!run_script(script, art, 3, err, errsz)) return 0;

    all[0] = "--rom"; all[1] = rom_path; all[2] = "--all";
    if (!run_script(script, all, 3, err, errsz)) return 0;

    join(script, sizeof script, romimport_tools_dir, "emit_scene_text.py");
    text[0] = "--rom"; text[1] = rom_path;
    return run_script(script, text, 2, err, errsz);
}
