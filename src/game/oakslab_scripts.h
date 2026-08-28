#pragma once
#include <stdint.h>

void OaksLabScripts_OnMapLoad(void);
void OaksLabScripts_Tick(void);
int  OaksLabScripts_IsActive(void);

int OaksLabScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out);

void OaksLabScripts_PostRender(void);
