
#include "rom_import.h"
#include "exe_dir.h"
#include "data_dir.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define EXEC_POPEN  _popen
#define EXEC_PCLOSE _pclose
#define SETUP_EXE   "setup.exe"
#else
#define EXEC_POPEN  popen
#define EXEC_PCLOSE pclose
#define SETUP_EXE   "setup"
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

int RomImport_BundledSetupPath(char *out, size_t n) {
    static const char *const subdirs[] = { "internal/", "" };
    char dir[1024], path[1200];
    FILE *f;
    size_t i;
    if (!ExeDir_Get(dir, sizeof dir)) return 0;
    for (i = 0; i < sizeof subdirs / sizeof subdirs[0]; i++) {
        if ((size_t)snprintf(path, sizeof path, "%s%s%s", dir, subdirs[i], SETUP_EXE)
            >= sizeof path)
            continue;
        f = fopen(path, "rb");
        if (!f) continue;
        fclose(f);
        if ((size_t)snprintf(out, n, "%s", path) >= n) return 0;
        return 1;
    }
    return 0;
}

int RomImport_HaveBundledSetup(void) {
    char path[1200];
    return RomImport_BundledSetupPath(path, sizeof path);
}

typedef struct {
#ifdef _WIN32
    HANDLE rd;
    HANDLE proc;
    char   buf[4096];
    size_t len;
    int    eof;
#else
    FILE  *fp;
#endif
} proc_t;

static int proc_open(proc_t *p, char *cmd, const char *exe) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE wr = NULL;

    (void)exe;
    memset(p, 0, sizeof *p);
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&p->rd, &wr, &sa, 0)) return 0;

    SetHandleInformation(p->rd, HANDLE_FLAG_INHERIT, 0);

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    si.hStdInput  = NULL;

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(p->rd); CloseHandle(wr);
        p->rd = NULL;
        return 0;
    }
    CloseHandle(wr);
    CloseHandle(pi.hThread);
    p->proc = pi.hProcess;
    return 1;
#else
    (void)exe;
    p->fp = EXEC_POPEN(cmd, "r");
    return p->fp != NULL;
#endif
}

static int proc_gets(proc_t *p, char *out, size_t out_sz) {
#ifdef _WIN32
    for (;;) {
        char *nl = (char *)memchr(p->buf, '\n', p->len);
        size_t take;
        DWORD got = 0;

        if (nl || (p->eof && p->len)) {
            take = nl ? (size_t)(nl - p->buf) + 1 : p->len;
            if (take >= out_sz) take = out_sz - 1;
            memcpy(out, p->buf, take);
            out[take] = '\0';
            memmove(p->buf, p->buf + take, p->len - take);
            p->len -= take;
            return 1;
        }
        if (p->eof) return 0;
        if (p->len == sizeof p->buf) {
            memcpy(out, p->buf, out_sz - 1);
            out[out_sz - 1] = '\0';
            p->len = 0;
            return 1;
        }
        if (!ReadFile(p->rd, p->buf + p->len, (DWORD)(sizeof p->buf - p->len),
                      &got, NULL) || got == 0) {
            p->eof = 1;
            continue;
        }
        p->len += got;
    }
#else
    return fgets(out, (int)out_sz, p->fp) != NULL;
#endif
}

static int proc_close(proc_t *p) {
#ifdef _WIN32
    DWORD code = 1;
    if (p->rd) CloseHandle(p->rd);
    if (p->proc) {
        WaitForSingleObject(p->proc, INFINITE);
        GetExitCodeProcess(p->proc, &code);
        CloseHandle(p->proc);
    }
    return (int)code;
#else
    return EXEC_PCLOSE(p->fp);
#endif
}

int RomImport_RunBundledSetup(const char *rom_path,
                              void (*on_stage)(void *ctx, int stage), void *ctx,
                              char *err, size_t errsz) {
    char exe[1200], cmd[3072], q[1400], tail[512] = "";
    proc_t pr;
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
#ifndef _WIN32
    strncat(cmd, " 2>&1", sizeof cmd - strlen(cmd) - 1);
#endif

    if (!proc_open(&pr, cmd, exe)) {
        snprintf(err, errsz, "could not run %s", exe);
        return 0;
    }
    {
        char line[256];
        while (proc_gets(&pr, line, sizeof line)) {
            printf("%s", line);

            if (stage == 1 && strstr(line, "[3/4]")) {
                stage = 2;
                if (on_stage) on_stage(ctx, 2);
            }
            snprintf(tail, sizeof tail, "%s", line);
        }
    }
    fflush(stdout);
    rc = proc_close(&pr);
    if (rc != 0) {
        size_t n = strlen(tail);
        while (n && (tail[n-1] == '\n' || tail[n-1] == '\r')) tail[--n] = '\0';
        snprintf(err, errsz, "setup failed: %s", tail[0] ? tail : "(no output)");
        return 0;
    }
    return 1;
}
