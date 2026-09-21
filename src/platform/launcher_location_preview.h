#pragma once

#include <SDL.h>
#include <stddef.h>
#include <stdint.h>

#include "launcher_location_names.h"

SDL_Texture *LauncherLocationPreview_Create(SDL_Renderer *renderer,
                                            const char *version,
                                            const char *vmap,
                                            uint8_t player_x,
                                            uint8_t player_y);
