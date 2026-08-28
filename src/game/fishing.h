#pragma once

#include <stdint.h>

int  Fishing_CanUse(void);

void Fishing_Use(uint8_t item_id);

int  Fishing_IsActive(void);

void Fishing_Tick(void);

void Fishing_PostRender(void);
