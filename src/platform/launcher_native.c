
#include "launcher_native.h"

#ifdef _WIN32

#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <commdlg.h>
#include <string.h>

int LauncherNative_HasFileDialog(void) { return 1; }

int LauncherNative_BrowseFile(SDL_Window *win, const char *title,
                              const char *filter, char *out_path, size_t out_sz) {
    if (!out_path || out_sz == 0) return 0;

    wchar_t wfilter[256];
    int wi = 0;
    for (const char *p = filter; *p && wi < (int)(sizeof(wfilter) / sizeof(wchar_t)) - 2; p++)
        wfilter[wi++] = (*p == '|') ? L'\0' : (wchar_t)(unsigned char)*p;
    wfilter[wi++] = L'\0';
    wfilter[wi++] = L'\0';

    wchar_t wtitle[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle,
                        (int)(sizeof(wtitle) / sizeof(wchar_t)));

    wchar_t path_buf[MAX_PATH] = L"";

    HWND owner = NULL;
    if (win) {
        SDL_SysWMinfo wm;
        SDL_VERSION(&wm.version);
        if (SDL_GetWindowWMInfo(win, &wm) && wm.subsystem == SDL_SYSWM_WINDOWS)
            owner = wm.info.win.window;
    }

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = wfilter;
    ofn.lpstrFile   = path_buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = wtitle;

    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) return 0;

    int n = WideCharToMultiByte(CP_UTF8, 0, path_buf, -1, out_path,
                                (int)out_sz, NULL, NULL);
    return n > 0;
}

#elif defined(__APPLE__)

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *dialog_tool(void) {
    static const char *found;
    static int looked;
    static const char *kCandidates[] = {
        "/usr/bin/kdialog", "/usr/local/bin/kdialog",
        "/usr/bin/zenity",  "/usr/local/bin/zenity",
        NULL
    };
    if (looked) return found;
    looked = 1;
    for (int i = 0; kCandidates[i]; i++) {
        if (access(kCandidates[i], X_OK) == 0) { found = kCandidates[i]; break; }
    }
    return found;
}

int LauncherNative_HasFileDialog(void) { return dialog_tool() != NULL; }

static void sq(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    if (!out_sz) return;
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
    out[o] = '\0';
}

static void first_patterns(const char *filter, char *out, size_t out_sz) {
    const char *bar = filter ? strchr(filter, '|') : NULL;
    size_t o = 0;
    out[0] = '\0';
    if (!bar) return;
    for (const char *p = bar + 1; *p && *p != '|' && o + 1 < out_sz; p++)
        out[o++] = (*p == ';') ? ' ' : *p;
    out[o] = '\0';
}

int LauncherNative_BrowseFile(SDL_Window *win, const char *title,
                              const char *filter, char *out_path, size_t out_sz) {
    const char *tool = dialog_tool();
    char pats[256], qt[512], qp[512], cmd[1600];
    const char *home;
    FILE *fp;
    size_t n;

    (void)win;
    if (!out_path || out_sz == 0) return 0;
    out_path[0] = '\0';
    if (!tool) return 0;

    first_patterns(filter, pats, sizeof pats);
    if (!pats[0]) snprintf(pats, sizeof pats, "*");
    home = getenv("HOME");
    if (!home || !home[0]) home = ".";

    sq(title ? title : "Select a file", qt, sizeof qt);

    if (strstr(tool, "kdialog")) {

        char qf[400], qh[400];
        char label[320];
        snprintf(label, sizeof label, "%s|Game Boy ROM", pats);
        sq(label, qf, sizeof qf);
        sq(home, qh, sizeof qh);
        snprintf(cmd, sizeof cmd, "%s --getopenfilename %s %s --title %s 2>/dev/null",
                 tool, qh, qf, qt);
    } else {

        char qf[400], qh[400];
        char label[320];
        snprintf(label, sizeof label, "Game Boy ROM | %s", pats);
        sq(label, qf, sizeof qf);
        sq(home, qh, sizeof qh);
        snprintf(cmd, sizeof cmd,
                 "%s --file-selection --title=%s --filename=%s/ --file-filter=%s "
                 "2>/dev/null", tool, qt, qh, qf);
    }

    fp = popen(cmd, "r");
    if (!fp) return 0;
    if (!fgets(out_path, (int)out_sz, fp)) { pclose(fp); out_path[0] = '\0'; return 0; }
    pclose(fp);

    n = strlen(out_path);
    while (n && (out_path[n-1] == '\n' || out_path[n-1] == '\r')) out_path[--n] = '\0';
    return out_path[0] != '\0';
}

#endif
