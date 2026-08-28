
#ifndef STEAM_SHORTCUT_H
#define STEAM_SHORTCUT_H

#include <stddef.h>

int SteamShortcut_Offer(void);

int SteamShortcut_Request(char *err, size_t errsz);

int SteamShortcut_Poll(void);

#endif
