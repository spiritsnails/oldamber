#pragma once

#include <stddef.h>

#define GAMEVER_MAX 4

const char *GameVersion_Current(void);

void GameVersion_Set(const char *id);

int GameVersion_ScanInstalled(const char *out[], int max);

int GameVersion_SupportedCount(void);

const char *GameVersion_Label(const char *id);

const char *GameVersion_LabelAt(int index);

const char *GameVersion_FromRomHeader(const char *rom_path);

const char *GameVersion_SavePath(const char *id);
