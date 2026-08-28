#pragma once

#include <stddef.h>

typedef struct SDL_Window SDL_Window;

int LauncherNative_HasFileDialog(void);

int LauncherNative_BrowseFile(SDL_Window *win, const char *title,
                              const char *filter, char *out_path, size_t out_sz);
