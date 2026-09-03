#pragma once

#include <stddef.h>

typedef enum {
    LAUNCHER_GOT_PAK,
    LAUNCHER_CANCELLED,
    LAUNCHER_RESTART,
} launcher_result_t;

launcher_result_t Launcher_Run(const char *tools_dir, const char *out_pak_path,
                               const char *romimport_tools_dir,
                               char *chosen_version, size_t chosen_sz);

int Launcher_DebugToolingEnabled(void);
