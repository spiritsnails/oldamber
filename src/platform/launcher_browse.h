#pragma once

#include <SDL.h>
#include <stddef.h>

#include "launcher_nav.h"

int LauncherBrowse_Run(SDL_Renderer *r, SDL_Window *win, launcher_nav_t *nav,
                       const char *title, const char *const *exts,
                       const char *start_dir, char *out_path, size_t out_sz);
