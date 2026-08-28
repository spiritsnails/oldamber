
#include "rom_import.h"
#include "exe_dir.h"
#include "data_dir.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define EXEC_POPEN  _popen
#define EXEC_PCLOSE _pclose
#define SETUP_EXE   "setup.exe"
#else
#define EXEC_POPEN  popen
#define EXEC_PCLOSE pclose
#define SETUP_EXE   "setup"
#endif

static void sh_quote(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    if (!out_sz) return;
#ifdef _WIN32
    if (o + 1 < out_sz) out[o++] = '"';
    for (const char *p = in; *p && o + 2 < out_sz; p++) {
        if (*p == '"') out[o++] = '\\';
        out[o++] = *p;
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

int RomImport_BundledSetupPath(char *out, size_t n) {
    char dir[1024], path[1200];
    FILE *f;
    if (!ExeDir_Get(dir, sizeof dir)) return 0;
    if ((size_t)snprintf(path, sizeof path, "%s%s", dir, SETUP_EXE) >= sizeof path)
        return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    if ((size_t)snprintf(out, n, "%s", path) >= n) return 0;
    return 1;
}

int RomImport_HaveBundledSetup(void) {
    char path[1200];
    return RomImport_BundledSetupPath(path, sizeof path);
}

int RomImport_RunBundledSetup(const char *rom_path,
                              void (*on_stage)(void *ctx, int stage), void *ctx,
                              char *err, size_t errsz) {
    char exe[1200], cmd[3072], q[1400], tail[512] = "";
    FILE *fp;
    int rc, stage = 1;

    if (err && errsz) err[0] = '\0';
    if (!RomImport_BundledSetupPath(exe, sizeof exe)) {
        snprintf(err, errsz, "no %s beside the game", SETUP_EXE);
        return 0;
    }

    sh_quote(exe, q, sizeof q);
    snprintf(cmd, sizeof cmd, "%s", q);
    sh_quote(rom_path, q, sizeof q);
    strncat(cmd, " ", sizeof cmd - strlen(cmd) - 1);
    strncat(cmd, q, sizeof cmd - strlen(cmd) - 1);

    strncat(cmd, " --no-dialog", sizeof cmd - strlen(cmd) - 1);

    {
        char data[1024];
        if (DataDir_Get(data, sizeof data)) {
            sh_quote(data, q, sizeof q);
            strncat(cmd, " --dest ", sizeof cmd - strlen(cmd) - 1);
            strncat(cmd, q, sizeof cmd - strlen(cmd) - 1);
        }
    }
    strncat(cmd, " 2>&1", sizeof cmd - strlen(cmd) - 1);

    fp = EXEC_POPEN(cmd, "r");
    if (!fp) {
        snprintf(err, errsz, "could not run %s", exe);
        return 0;
    }
    {
        char line[256];
        while (fgets(line, sizeof line, fp)) {
            printf("%s", line);

            if (stage == 1 && strstr(line, "[3/4]")) {
                stage = 2;
                if (on_stage) on_stage(ctx, 2);
            }
            snprintf(tail, sizeof tail, "%s", line);
        }
    }
    fflush(stdout);
    rc = EXEC_PCLOSE(fp);
    if (rc != 0) {
        size_t n = strlen(tail);
        while (n && (tail[n-1] == '\n' || tail[n-1] == '\r')) tail[--n] = '\0';
        snprintf(err, errsz, "setup failed: %s", tail[0] ? tail : "(no output)");
        return 0;
    }
    return 1;
}
