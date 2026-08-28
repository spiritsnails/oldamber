#pragma once

#include <stdint.h>

void PyAI_SetEnabled(int enabled, const char *script_path);
int PyAI_IsEnabled(void);
const char *PyAI_GetScriptPath(void);

int PyAI_ChooseEnemyMove(uint8_t *out_slot, uint8_t *out_move);
