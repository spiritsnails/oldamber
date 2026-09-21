#pragma once

#include <SDL.h>

#include "launcher_nav.h"

int LauncherSaveManager_Run(SDL_Renderer *r, SDL_Window *win,
                            launcher_nav_t *nav);
