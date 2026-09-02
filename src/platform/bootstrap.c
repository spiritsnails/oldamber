#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define OA_SEP '\\'
#define oa_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/wait.h>
#define OA_SEP '/'
#define oa_mkdir(path) mkdir((path), 0755)
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#define PATH_CAP 4096
#define VERSION_CAP 64
#define UPDATE_RESTART_EXIT 75

static void report_error(const char *message) {
#ifdef _WIN32
    MessageBoxA(NULL,message,"OldAmber",MB_OK|MB_ICONERROR);
#endif
    fprintf(stderr,"%s\n",message);
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int make_tree(const char *path) {
    char copy[PATH_CAP];
    size_t len = strlen(path);
    if (!len || len >= sizeof copy) return 0;
    memcpy(copy, path, len + 1);
    for (char *p = copy + 1; *p; ++p) {
        if (*p != '/' && *p != '\\') continue;
        char saved = *p;
        *p = '\0';
        if (!path_exists(copy) && oa_mkdir(copy) != 0 && errno != EEXIST) return 0;
        *p = saved;
    }
    return path_exists(copy) || oa_mkdir(copy) == 0 || errno == EEXIST;
}

static int executable_path(char *out, size_t cap) {
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
    return n > 0 && n < cap;
#elif defined(__APPLE__)
    uint32_t n = (uint32_t)cap;
    return _NSGetExecutablePath(out, &n) == 0;
#else
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    if (n <= 0 || (size_t)n >= cap - 1) return 0;
    out[n] = '\0';
    return 1;
#endif
}

static void dirname_in_place(char *path) {
    char *slash = strrchr(path, '/');
    char *back = strrchr(path, '\\');
    char *cut = slash > back ? slash : back;
    if (cut) *cut = '\0';
}

static int user_data_dir(char *out, size_t cap) {
    const char *base;
#ifdef _WIN32
    base = getenv("APPDATA");
    if (!base || !base[0]) base = getenv("LOCALAPPDATA");
    if (!base || !base[0]) return 0;
    if ((size_t)snprintf(out, cap, "%s\\spiritsnails\\OldAmber", base) >= cap) return 0;
#elif defined(__APPLE__)
    base = getenv("HOME");
    if (!base || !base[0]) return 0;
    if ((size_t)snprintf(out, cap, "%s/Library/Application Support/spiritsnails/OldAmber", base) >= cap) return 0;
#else
    base = getenv("XDG_DATA_HOME");
    if (base && base[0]) {
        if ((size_t)snprintf(out, cap, "%s/spiritsnails/OldAmber", base) >= cap) return 0;
    } else {
        base = getenv("HOME");
        if (!base || !base[0]) return 0;
        if ((size_t)snprintf(out, cap, "%s/.local/share/spiritsnails/OldAmber", base) >= cap) return 0;
    }
#endif
    return make_tree(out);
}

static int safe_version(const char *version) {
    size_t n = strlen(version);
    if (!n || n >= VERSION_CAP) return 0;
    for (size_t i = 0; i < n; ++i) {
        char c = version[i];
        if (!((c >= '0' && c <= '9') || c == '.')) return 0;
    }
    return version[0] != '.' && version[n - 1] != '.' && strstr(version, "..") == NULL;
}

static int read_version(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (!fgets(out, (int)cap, f)) { fclose(f); return 0; }
    fclose(f);
    out[strcspn(out, "\r\n")] = '\0';
    return safe_version(out);
}

static int version_dir_name(const char *version, char *out, size_t cap) {
    size_t n = strlen(version);
    if (n + 2 > cap) return 0;
    out[0] = 'v';
    for (size_t i = 0; i < n; i++)
        out[i + 1] = version[i] == '.' ? '_' : version[i];
    out[n + 1] = '\0';
    return 1;
}

static int child_path(const char *root, const char *version, int user,
                      char *out, size_t cap);

static int write_version(const char *path, const char *version) {
    char tmp[PATH_CAP];
    if ((size_t)snprintf(tmp,sizeof tmp,"%s.new",path)>=sizeof tmp) return 0;
    FILE *f=fopen(tmp,"wb"); if (!f) return 0;
    int ok=fprintf(f,"%s\n",version)>0;
    if (fclose(f)!=0) ok=0;
    if (!ok) { remove(tmp); return 0; }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(tmp,path)!=0) { remove(tmp); return 0; }
    return 1;
}

static int rollback_pending(const char *install, const char *data, const char *launched) {
    char pending_path[PATH_CAP], previous_path[PATH_CAP], current_path[PATH_CAP];
    char pending[VERSION_CAP], previous[VERSION_CAP], child[PATH_CAP];
    snprintf(pending_path,sizeof pending_path,"%s%cpending-version",data,OA_SEP);
    if (!read_version(pending_path,pending,sizeof pending) || strcmp(pending,launched)!=0) return 0;
    snprintf(previous_path,sizeof previous_path,"%s%cprevious-version",data,OA_SEP);
    if (!read_version(previous_path,previous,sizeof previous)) return 0;
    if (!child_path(data,previous,1,child,sizeof child) &&
        !child_path(install,previous,0,child,sizeof child)) return 0;
    snprintf(current_path,sizeof current_path,"%s%ccurrent-version",data,OA_SEP);
    if (!write_version(current_path,previous)) return 0;
    remove(pending_path);
    fprintf(stderr,"OldAmber rolled back version %s to %s after it failed to start.\n",pending,previous);
    return 1;
}

static int child_path(const char *root, const char *version, int user,
                      char *out, size_t cap) {
    char version_dir[VERSION_CAP + 2];
#ifdef _WIN32
    const char *name = "oldamber-game.exe";
#else
    const char *name = "oldamber-game";
#endif
    if (!version_dir_name(version, version_dir, sizeof version_dir)) return 0;
    int n = snprintf(out, cap, "%s%c%sversions%c%s%c%s",
                     root, OA_SEP, user ? "" : "", OA_SEP,
                     version_dir, OA_SEP, name);
    return n > 0 && (size_t)n < cap && path_exists(out);
}

static int find_child(const char *install, const char *data, int allow_user,
                      char *version, size_t version_cap,
                      char *child, size_t child_cap) {
    char pointer[PATH_CAP];
    snprintf(pointer, sizeof pointer, "%s%ccurrent-version", data, OA_SEP);
    if (allow_user && read_version(pointer, version, version_cap) &&
        child_path(data, version, 1, child, child_cap)) return 1;

    snprintf(pointer, sizeof pointer, "%s%cbundled-version", install, OA_SEP);
    if (read_version(pointer, version, version_cap) &&
        child_path(install, version, 0, child, child_cap)) return 1;
    return 0;
}

static int launch_child(const char *child, int argc, char **argv) {
#ifdef _WIN32
    size_t cap = strlen(child) + 4;
    for (int i = 1; i < argc; ++i) cap += strlen(argv[i]) * 2 + 4;
    char *cmd = (char *)calloc(cap, 1);
    if (!cmd) return 1;
    snprintf(cmd, cap, "\"%s\"", child);
    for (int i = 1; i < argc; ++i) {
        strcat(cmd, " \"");
        for (const char *p = argv[i]; *p; ++p) {
            if (*p == '\"') strcat(cmd, "\\");
            size_t n = strlen(cmd);
            cmd[n] = *p;
            cmd[n + 1] = '\0';
        }
        strcat(cmd, "\"");
    }
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;
    if (!CreateProcessA(child, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "OldAmber could not launch %s (error %lu)\n", child, GetLastError());
        free(cmd);
        return 1;
    }
    free(cmd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
#else
    char **child_argv = (char **)calloc((size_t)argc + 1, sizeof(char *));
    if (!child_argv) return 1;
    child_argv[0] = (char *)child;
    for (int i = 1; i < argc; ++i) child_argv[i] = argv[i];
    pid_t pid = fork();
    if (pid == 0) {
        execv(child, child_argv);
        _exit(127);
    }
    free(child_argv);
    if (pid < 0) return 1;
    int status;
    if (waitpid(pid, &status, 0) < 0) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
#endif
}

int main(int argc, char **argv) {
    char install[PATH_CAP], data[PATH_CAP], version[VERSION_CAP], child[PATH_CAP];
    if (!executable_path(install, sizeof install)) {
        report_error("OldAmber could not locate its installation.");
        return 1;
    }
    dirname_in_place(install);
    if (!user_data_dir(data, sizeof data)) {
        report_error("OldAmber could not open its user data directory.");
        return 1;
    }
#ifdef _WIN32
    SetEnvironmentVariableA("OLDAMBER_DATA_DIR", data);
    SetEnvironmentVariableA("OLDAMBER_BOOTSTRAPPED", "1");
    SetEnvironmentVariableA("OLDAMBER_BOOTSTRAP_DIR", install);
    SetEnvironmentVariableA("OLDAMBER_LEGACY_INSTALL_DIR", install);
#else
    setenv("OLDAMBER_DATA_DIR", data, 1);
    setenv("OLDAMBER_BOOTSTRAPPED", "1", 1);
    setenv("OLDAMBER_BOOTSTRAP_DIR", install, 1);
    setenv("OLDAMBER_LEGACY_INSTALL_DIR", install, 1);
#endif

    for (int restarts = 0; restarts < 4; ++restarts) {
        int allow_user = getenv("FLATPAK_ID") == NULL;
        if (!find_child(install, data, allow_user, version, sizeof version, child, sizeof child)) {
            report_error("OldAmber has no runnable installed version.");
            return 1;
        }
#ifdef _WIN32
        SetEnvironmentVariableA("OLDAMBER_RUNNING_VERSION", version);
#else
        setenv("OLDAMBER_RUNNING_VERSION", version, 1);
#endif
        int code = launch_child(child, argc, argv);
        if (code == UPDATE_RESTART_EXIT) continue;
        if (rollback_pending(install, data, version)) continue;
        return code;
    }
    report_error("OldAmber stopped an update restart loop.");
    return 1;
}
