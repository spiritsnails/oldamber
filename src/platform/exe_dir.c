
#include "exe_dir.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#  include <mach-o/dyld.h>
#  include <stdlib.h>
#  include <stdint.h>
#endif

int ExeDir_Get(char *out, size_t n) {
#ifdef __APPLE__
    char raw[1024], real[1024];
    uint32_t sz = (uint32_t)sizeof raw;
    char *slash;
    if (_NSGetExecutablePath(raw, &sz) != 0) return 0;

    if (!realpath(raw, real)) {
        if ((size_t)snprintf(real, sizeof real, "%s", raw) >= sizeof real)
            return 0;
    }
    slash = strrchr(real, '/');
    if (!slash) return 0;
    slash[1] = '\0';
    return (size_t)snprintf(out, n, "%s", real) < n;
#else
    char *base = SDL_GetBasePath();
    int ok;
    if (!base) return 0;
    ok = ((size_t)snprintf(out, n, "%s", base) < n);
    SDL_free(base);
    return ok;
#endif
}
