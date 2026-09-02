
#include "steam_shortcut.h"
#include "display.h"

#include <stdio.h>
#include <string.h>

#if defined(__linux__)

#include <SDL.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHORTCUT_NAME "OldAmber"
#define DESKTOP_BASE  "OldAmber.desktop"

static int s_offer = -1;

static void say_err(char *err, size_t n, const char *msg) {
    if (err && n) snprintf(err, n, "%s", msg);
}

static int find_bytes(const char *hay, size_t hn, const char *needle, size_t nn) {
    if (nn == 0 || hn < nn) return 0;
    for (size_t i = 0; i + nn <= hn; i++)
        if (memcmp(hay + i, needle, nn) == 0) return 1;
    return 0;
}

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    return (h && h[0]) ? h : NULL;
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    size_t n = strlen(path);
    if (n >= sizeof tmp) return;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        (void)mkdir(tmp, 0755);
        *p = '/';
    }
    (void)mkdir(tmp, 0755);
}

static int exe_dir(char *out, size_t n) {
    char exe[1024];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (len <= 0) return 0;
    exe[len] = '\0';
    char *slash = strrchr(exe, '/');
    if (!slash) return 0;
    *slash = '\0';
    if ((size_t)snprintf(out, n, "%s", exe) >= n) return 0;
    return 1;
}

static int target_path(char *out, size_t n) {
    char dir[1024];
    const char *bootstrap = getenv("OLDAMBER_BOOTSTRAP_DIR");
    if (bootstrap && bootstrap[0]) {
        char shim[1200];
        if ((size_t)snprintf(shim, sizeof shim, "%s/OldAmber.sh", bootstrap) < sizeof shim &&
            access(shim, X_OK) == 0)
            return (size_t)snprintf(out, n, "%s", shim) < n;
    }
    if (exe_dir(dir, sizeof dir)) {
        char shim[1200];
        if ((size_t)snprintf(shim, sizeof shim, "%s/OldAmber.sh", dir) < sizeof shim &&
            access(shim, X_OK) == 0) {
            if ((size_t)snprintf(out, n, "%s", shim) >= n) return 0;
            return 1;
        }
    }
    char exe[1024];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (len <= 0) return 0;
    exe[len] = '\0';
    if ((size_t)snprintf(out, n, "%s", exe) >= n) return 0;
    return 1;
}

static int icon_path(char *out, size_t n) {
    char dir[1024];
    const char *bootstrap = getenv("OLDAMBER_BOOTSTRAP_DIR");
    if (bootstrap && bootstrap[0]) {
        if ((size_t)snprintf(out, n, "%s/icon.png", bootstrap) >= n) return 0;
        return access(out, R_OK) == 0;
    }
    if (!exe_dir(dir, sizeof dir)) return 0;
    if ((size_t)snprintf(out, n, "%s/icon.png", dir) >= n) return 0;
    return access(out, R_OK) == 0;
}

static int steam_pid_alive(void) {
    const char *h = home_dir();
    if (!h) return 0;
    char path[1024];
    snprintf(path, sizeof path, "%s/.steam/steam.pid", h);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long pid = 0;
    int got = fscanf(f, "%ld", &pid);
    fclose(f);
    if (got != 1 || pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0;
}

static int shortcut_exists(const char *target) {
    const char *h = home_dir();
    if (!h) return 0;

    char udroot[1024];
    snprintf(udroot, sizeof udroot, "%s/.steam/steam/userdata", h);
    DIR *d = opendir(udroot);
    if (!d) return 0;

    int found = 0;
    struct dirent *e;
    while (!found && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char vdf[1400];
        if ((size_t)snprintf(vdf, sizeof vdf, "%s/%s/config/shortcuts.vdf",
                             udroot, e->d_name) >= sizeof vdf)
            continue;
        FILE *f = fopen(vdf, "rb");
        if (!f) continue;
        if (fseek(f, 0, SEEK_END) == 0) {
            long sz = ftell(f);

            if (sz > 0 && sz < 32L * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
                char *buf = (char *)malloc((size_t)sz);
                if (buf) {
                    size_t rd = fread(buf, 1, (size_t)sz, f);
                    found = find_bytes(buf, rd, target, strlen(target));
                    free(buf);
                }
            }
        }
        fclose(f);
    }
    closedir(d);
    return found;
}

static void esc_exec(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 3 < n; i++) {
        char c = in[i];
        if (c == '\\' || c == '"' || c == '`' || c == '$') out[o++] = '\\';
        out[o++] = c;
    }
    out[o] = '\0';
}

static int write_desktop(const char *target, char *out, size_t n) {
    const char *h = home_dir();
    if (!h) return 0;

    char dir[1024];
    snprintf(dir, sizeof dir, "%s/.local/share/applications", h);
    mkdir_p(dir);
    if ((size_t)snprintf(out, n, "%s/%s", dir, DESKTOP_BASE) >= n) return 0;

    char quoted[2048];
    esc_exec(target, quoted, sizeof quoted);

    char workdir[1024];
    snprintf(workdir, sizeof workdir, "%s", target);
    char *slash = strrchr(workdir, '/');
    if (slash) *slash = '\0';

    char icon[1024];
    int have_icon = icon_path(icon, sizeof icon);

    FILE *f = fopen(out, "w");
    if (!f) return 0;
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Name=%s\n", SHORTCUT_NAME);
    fprintf(f, "Comment=Pokemon Red and Blue, natively ported\n");
    fprintf(f, "Exec=\"%s\"\n", quoted);
    fprintf(f, "Path=%s\n", workdir);
    if (have_icon) fprintf(f, "Icon=%s\n", icon);
    fprintf(f, "Terminal=false\n");
    fprintf(f, "Categories=Game;\n");
    int ok = (fclose(f) == 0);
    if (ok) (void)chmod(out, 0755);
    return ok;
}

static void url_encode(const char *in, char *out, size_t n) {
    static const char *hex = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < n; i++) {
        unsigned char c = (unsigned char)in[i];
        int safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                   c == '.' || c == '~';
        if (safe) {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0xF];
        }
    }
    out[o] = '\0';
}

static int run_steam_url(const char *url) {
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {

        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        execlp("steam", "steam", url, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static char   s_pending[1024];
static Uint32 s_deadline;

int SteamShortcut_Offer(void) {
    if (s_offer >= 0) return s_offer;
    s_offer = 0;

    if (!Display_IsSteamDeck()) {
        fprintf(stderr, "[steam] no offer: not a Steam Deck\n");
        return s_offer;
    }
    if (!steam_pid_alive()) {
        fprintf(stderr, "[steam] no offer: Steam is not running\n");
        return s_offer;
    }

    char target[1024];
    if (!target_path(target, sizeof target)) {
        fprintf(stderr, "[steam] no offer: cannot resolve my own path\n");
        return s_offer;
    }
    if (shortcut_exists(target)) {
        fprintf(stderr, "[steam] no offer: already in the library -> %s\n", target);
        return s_offer;
    }

    fprintf(stderr, "[steam] offering to add -> %s\n", target);
    s_offer = 1;
    return s_offer;
}

int SteamShortcut_Request(char *err, size_t errsz) {
    if (err && errsz) err[0] = '\0';
    s_pending[0] = '\0';

    char target[1024];
    if (!target_path(target, sizeof target)) {
        say_err(err, errsz, "COULD NOT FIND THIS PROGRAM ON DISK");
        return 0;
    }

    if (!steam_pid_alive()) {
        say_err(err, errsz, "STEAM IS NOT RUNNING");
        return 0;
    }

    char desktop[1024];
    if (!write_desktop(target, desktop, sizeof desktop)) {
        say_err(err, errsz, "COULD NOT WRITE THE SHORTCUT FILE");
        fprintf(stderr, "[steam] could not write .desktop for %s\n", target);
        return 0;
    }

    char url[3072], enc[2048];
    url_encode(desktop, enc, sizeof enc);
    snprintf(url, sizeof url, "steam://addnonsteamgame/%s", enc);

    {
        int fd = open("/tmp/addnonsteamgamefile", O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            say_err(err, errsz, "COULD NOT PREPARE THE STEAM REQUEST");
            fprintf(stderr, "[steam] cannot create /tmp/addnonsteamgamefile\n");
            return 0;
        }
        close(fd);
    }

    if (!run_steam_url(url)) {
        say_err(err, errsz, "STEAM REFUSED THE REQUEST");
        fprintf(stderr, "[steam] handing off %s failed\n", url);
        return 0;
    }

    snprintf(s_pending, sizeof s_pending, "%s", target);
    s_deadline = SDL_GetTicks() + 5000;
    fprintf(stderr, "[steam] add requested for %s\n", target);
    return 1;
}

int SteamShortcut_Poll(void) {
    if (!s_pending[0]) return -1;
    if (shortcut_exists(s_pending)) {
        fprintf(stderr, "[steam] confirmed in shortcuts.vdf: %s\n", s_pending);
        s_pending[0] = '\0';
        s_offer = 0;
        return 1;
    }

    if ((Sint32)(SDL_GetTicks() - s_deadline) >= 0) {
        fprintf(stderr, "[steam] no shortcuts.vdf change 5s after the request\n");
        s_pending[0] = '\0';
        return -1;
    }
    return 0;
}

#else

int SteamShortcut_Offer(void) { return 0; }

int SteamShortcut_Request(char *err, size_t errsz) {
    if (err && errsz) snprintf(err, errsz, "NOT SUPPORTED ON THIS SYSTEM");
    return 0;
}

int SteamShortcut_Poll(void) { return -1; }

#endif
