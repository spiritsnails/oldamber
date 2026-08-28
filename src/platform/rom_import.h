#pragma once

#include <stddef.h>

int RomImport_BuildPak(const char *rom_path, const char *tools_dir,
                       const char *out_pak_path, char *err, size_t errsz);

int RomImport_EmitKantoMaps(const char *rom_path, const char *romimport_tools_dir,
                            char *err, size_t errsz);

int RomImport_LooksLikeGBRom(const char *path);

int RomImport_HaveBundledSetup(void);

int RomImport_BundledSetupPath(char *out, size_t n);

int RomImport_RunBundledSetup(const char *rom_path,
                              void (*on_stage)(void *ctx, int stage), void *ctx,
                              char *err, size_t errsz);
