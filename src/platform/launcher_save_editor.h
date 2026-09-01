#pragma once
#include <SDL.h>
#include "launcher_nav.h"

int LauncherSaveEditor_Run(SDL_Renderer *r, SDL_Window *win,
                           launcher_nav_t *nav, const char *path,
                           const char *version_label);
