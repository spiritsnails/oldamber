#pragma once
#include <stdint.h>

int SgbBorder_Available(void);

int SgbBorder_SetEnabled(int on);
int SgbBorder_IsEnabled(void);

const uint32_t *SgbBorder_Frame(void);
