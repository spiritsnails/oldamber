
#include "data_dir.h"
#include "exe_dir.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  include <io.h>
#  define dd_mkdir(p) _mkdir(p)
#  define dd_access(p, m) _access(p, m)
#  define DD_W_OK 2
#  define DD_SEP '\\'
#else
#  include <unistd.h>
#  include <dirent.h>
#  define dd_mkdir(p) mkdir((p), 0755)
#  define dd_access(p, m) access(p, m)
#  define DD_W_OK W_OK
#  define DD_SEP '/'
#endif

static char s_dir[1024];
static int  s_resolved;
static int  s_separate;
static int  s_user_data_cwd;

static int exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int make_tree(const char *path) {
    char tmp[1024];
    size_t n = strlen(path);
    if (n >= sizeof tmp) return 0;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
        char save = *p;
        *p = '\0';
        if (!exists(tmp)) dd_mkdir(tmp);
        *p = save;
    }
    if (!exists(tmp)) dd_mkdir(tmp);
    return exists(tmp);
}

static void with_sep(char *s, size_t n) {
    size_t len = strlen(s);
    if (len && len + 2 < n && s[len - 1] != '/' && s[len - 1] != '\\') {
        s[len] = DD_SEP;
        s[len + 1] = '\0';
    }
}

static int writable(const char *dir) {
    return dd_access(dir, DD_W_OK) == 0;
}

static int in_macos_bundle(const char *exe_dir) {
#ifdef __APPLE__
    const char *tail = "/Contents/MacOS/";
    size_t n = strlen(exe_dir), t = strlen(tail);
    return n >= t && strcmp(exe_dir + n - t, tail) == 0;
#else
    (void)exe_dir;
    return 0;
#endif
}

static int in_flatpak(void) {
    return SDL_getenv("FLATPAK_ID") != NULL || exists("/.flatpak-info");
}

static int pref_path(char *out, size_t n) {
    char *p = SDL_GetPrefPath("spiritsnails", "OldAmber");
    int ok;
    if (!p) return 0;
    ok = ((size_t)snprintf(out, n, "%s", p) < n);
    SDL_free(p);
    return ok && make_tree(out);
}

int DataDir_Get(char *out, size_t n) {
    char exe[1024], probe[1200];
    const char *env;

    if (s_resolved) return (size_t)snprintf(out, n, "%s", s_dir) < n;

    env = SDL_getenv("OLDAMBER_DATA_DIR");
    if (env && env[0] && (size_t)snprintf(s_dir, sizeof s_dir, "%s", env) < sizeof s_dir) {
        with_sep(s_dir, sizeof s_dir);
        if (make_tree(s_dir)) {
            s_separate = 1;
            s_resolved = 1;
            return (size_t)snprintf(out, n, "%s", s_dir) < n;
        }
    }

    if (!ExeDir_Get(exe, sizeof exe)) return 0;

    if (in_macos_bundle(exe) || in_flatpak()) {
        if (pref_path(s_dir, sizeof s_dir)) {
            with_sep(s_dir, sizeof s_dir);
            s_separate = 1;
            s_resolved = 1;
            return (size_t)snprintf(out, n, "%s", s_dir) < n;
        }
    }

    if ((size_t)snprintf(probe, sizeof probe, "%spackages", exe) < sizeof probe &&
        exists(probe)) {
        snprintf(s_dir, sizeof s_dir, "%s", exe);
        s_separate = 0;
        s_resolved = 1;
        return (size_t)snprintf(out, n, "%s", s_dir) < n;
    }

    if (!writable(exe)) {
        if (pref_path(s_dir, sizeof s_dir)) {
            with_sep(s_dir, sizeof s_dir);
            s_separate = 1;
            s_resolved = 1;
            return (size_t)snprintf(out, n, "%s", s_dir) < n;
        }
    }

    snprintf(s_dir, sizeof s_dir, "%s", exe);
    s_separate = 0;
    s_resolved = 1;
    return (size_t)snprintf(out, n, "%s", s_dir) < n;
}

int DataDir_IsSeparate(void) {
    char tmp[1024];
    if (!s_resolved) DataDir_Get(tmp, sizeof tmp);
    return s_separate;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t n;
    int ok = 1;
    if (!in) return 0;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    fclose(out);
    return ok;
}

static int copy_if_missing(const char *src, const char *dst) {
    char tmp[1280];
    if (exists(dst) || !exists(src)) return 1;
    if ((size_t)snprintf(tmp, sizeof tmp, "%s.migrating", dst) >= sizeof tmp) return 0;
    remove(tmp);
    if (!copy_file(src, tmp)) { remove(tmp); return 0; }
    if (exists(dst)) { remove(tmp); return 1; }
    if (rename(tmp, dst) != 0) { remove(tmp); return 0; }
    return 1;
}

static int copy_replace(const char *src, const char *dst) {
    char tmp[1280];
    if (!exists(src)) return 1;
    if ((size_t)snprintf(tmp, sizeof tmp, "%s.updating", dst) >= sizeof tmp) return 0;
    remove(tmp);
    if (!copy_file(src, tmp)) { remove(tmp); return 0; }
#ifdef _WIN32
    remove(dst);
#endif
    if (rename(tmp, dst) != 0) { remove(tmp); return 0; }
    return 1;
}

static int copy_tree_mode(const char *src, const char *dst, int replace) {
    char sp[1200], dp[1200];
    struct stat st;
    int ok = 1;
    if (stat(src, &st) != 0) return 1;
    if (!exists(dst) && !make_tree(dst)) return 0;

#ifdef _WIN32
    {
        struct _finddata_t fd;
        intptr_t h;
        char pat[1200];
        if ((size_t)snprintf(pat, sizeof pat, "%s\\*", src) >= sizeof pat) return 0;
        h = _findfirst(pat, &fd);
        if (h == -1) return 0;
        do {
            if (!strcmp(fd.name, ".") || !strcmp(fd.name, "..")) continue;
            if ((size_t)snprintf(sp, sizeof sp, "%s\\%s", src, fd.name) >= sizeof sp) continue;
            if ((size_t)snprintf(dp, sizeof dp, "%s\\%s", dst, fd.name) >= sizeof dp) continue;
            if (fd.attrib & _A_SUBDIR) { if (!copy_tree_mode(sp, dp, replace)) ok = 0; }
            else if (!(replace ? copy_replace(sp, dp) : copy_if_missing(sp, dp))) ok = 0;
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
#else
    {
        DIR *d = opendir(src);
        struct dirent *e;
        if (!d) return 0;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            if ((size_t)snprintf(sp, sizeof sp, "%s/%s", src, e->d_name) >= sizeof sp) continue;
            if ((size_t)snprintf(dp, sizeof dp, "%s/%s", dst, e->d_name) >= sizeof dp) continue;
            if (stat(sp, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (!copy_tree_mode(sp, dp, replace)) ok = 0;
            } else if (!(replace ? copy_replace(sp, dp) : copy_if_missing(sp, dp))) ok = 0;
        }
        closedir(d);
    }
#endif
    return ok;
}

static int copy_tree(const char *src, const char *dst) {
    return copy_tree_mode(src, dst, 0);
}

int DataDir_SeedFromInstall(void) {
    char exe[1024], data[1024], sp[1200], dp[1200], marker[1200], recorded[64] = "";
    char running[64] = "";
    int replace = 0, ok = 1;
    static const char *kTrees[] = {
        "mod_runtime/blocks", "mod_runtime/scenes",
        "mod_runtime/config", "mod_runtime/map_edits",
        "shaders", NULL
    };
    static const char *kFiles[] = { "mod_runtime/pks_flag_registry.txt", NULL };

    if (!DataDir_Get(data, sizeof data)) return 0;
    if (!s_separate) return 1;
    if (!ExeDir_Get(exe, sizeof exe)) return 0;

    const char *running_env = SDL_getenv("OLDAMBER_RUNNING_VERSION");
    if (running_env) snprintf(running, sizeof running, "%s", running_env);
    snprintf(marker, sizeof marker, "%s.content-version", data);
    if (running[0]) {
        FILE *f = fopen(marker, "rb");
        if (f) {
            if (!fgets(recorded, sizeof recorded, f)) recorded[0] = '\0';
            fclose(f);
            recorded[strcspn(recorded, "\r\n")] = '\0';
        }
        if (!strcmp(recorded, running)) return 1;
        replace = 1;
    }

    for (int i = 0; kTrees[i]; i++) {
        if ((size_t)snprintf(sp, sizeof sp, "%s%s", exe, kTrees[i]) >= sizeof sp) continue;
        if ((size_t)snprintf(dp, sizeof dp, "%s%s", data, kTrees[i]) >= sizeof dp) continue;
        if (!copy_tree_mode(sp, dp, replace)) ok = 0;
    }
    for (int i = 0; kFiles[i]; i++) {
        if ((size_t)snprintf(sp, sizeof sp, "%s%s", exe, kFiles[i]) >= sizeof sp) continue;
        if ((size_t)snprintf(dp, sizeof dp, "%s%s", data, kFiles[i]) >= sizeof dp) continue;

        if (!copy_if_missing(sp, dp)) ok = 0;
    }
    if (ok && running[0]) {
        char tmp[1280];
        int wrote;
        snprintf(tmp, sizeof tmp, "%s.updating", marker);
        FILE *f = fopen(tmp, "wb");
        wrote = f && fprintf(f, "%s\n", running) > 0;
        if (f && fclose(f) != 0) wrote = 0;
        if (!wrote) {
            remove(tmp);
            return 0;
        }
#ifdef _WIN32
        remove(marker);
#endif
        if (rename(tmp, marker) != 0) { remove(tmp); return 0; }
    }
    return ok;
}

int UserDataDir_Get(char *out, size_t n) {
    const char *env = SDL_getenv("OLDAMBER_DATA_DIR");
    char dir[1024];

    if (s_user_data_cwd) return (size_t)snprintf(out, n, ".%c", DD_SEP) < n;
    if (env && env[0] && (size_t)snprintf(dir, sizeof dir, "%s", env) < sizeof dir) {
        with_sep(dir, sizeof dir);
        if (make_tree(dir))
            return (size_t)snprintf(out, n, "%s", dir) < n;
    }
#ifdef _WIN32
    if (!pref_path(dir, sizeof dir)) return 0;
    with_sep(dir, sizeof dir);
    return (size_t)snprintf(out, n, "%s", dir) < n;
#else
    return DataDir_Get(out, n);
#endif
}

void UserData_UseCurrentDirectory(void) {
    s_user_data_cwd = 1;
}

int UserDataPath(const char *relative, char *out, size_t n) {
    char dir[1024];
    if (!relative || !UserDataDir_Get(dir, sizeof dir)) return 0;
    return (size_t)snprintf(out, n, "%s%s", dir, relative) < n;
}

static int migrate_user_data_from(const char *source, const char *data) {
    static const char *kFiles[] = {
        "assets.pak",
        "pokered.sav", "pokered.sav.vmaps", "pokered.sav.npcrt",
        "pokeblue.sav", "pokeblue.sav.vmaps", "pokeblue.sav.npcrt",
        "presentation.cfg", "controls.cfg",
        "mod_runtime/pks_flag_registry.txt", NULL
    };
    static const char *kTrees[] = {
        "packages", "states", "saves_backup",
        "mod_runtime/custom_art", "mod_runtime/generatedmaps", NULL
    };
    char root[1024], sp[1200], dp[1200], parent[1200];
    int ok = 1;
    if ((size_t)snprintf(root, sizeof root, "%s", source) >= sizeof root) return 0;
    with_sep(root, sizeof root);

    for (int i = 0; kFiles[i]; i++) {
        if ((size_t)snprintf(sp, sizeof sp, "%s%s", root, kFiles[i]) >= sizeof sp) continue;
        if ((size_t)snprintf(dp, sizeof dp, "%s%s", data, kFiles[i]) >= sizeof dp) continue;
        if (strchr(kFiles[i], '/')) {
            snprintf(parent, sizeof parent, "%smod_runtime", data);
            make_tree(parent);
        }
        if (!copy_if_missing(sp, dp)) ok = 0;
    }
    for (int i = 0; kTrees[i]; i++) {
        if ((size_t)snprintf(sp, sizeof sp, "%s%s", root, kTrees[i]) >= sizeof sp) continue;
        if ((size_t)snprintf(dp, sizeof dp, "%s%s", data, kTrees[i]) >= sizeof dp) continue;
        if (!copy_tree(sp, dp)) ok = 0;
    }
    return ok;
}

int UserData_MigrateFromInstall(void) {
    char exe[1024], data[1024], legacy[1024] = "";
    const char *legacy_env = SDL_getenv("OLDAMBER_LEGACY_INSTALL_DIR");
    int ok = 1;

    if (legacy_env)
        snprintf(legacy, sizeof legacy, "%s", legacy_env);
    if (!UserDataDir_Get(data, sizeof data)) return 0;
    if (legacy[0] && strcmp(legacy, data) != 0)
        if (!migrate_user_data_from(legacy, data)) ok = 0;

#ifdef _WIN32
    if (!ExeDir_Get(exe, sizeof exe)) return 0;
    if (strcmp(exe, data) != 0)
        if (!migrate_user_data_from(exe, data)) ok = 0;
#else
    (void)exe;
#endif
    return ok;
}
