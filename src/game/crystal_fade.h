#pragma once

#include <stdint.h>

#define CRYSTAL_FADE_OUT_TO_WHITE   0
#define CRYSTAL_FADE_IN_FROM_WHITE  1
#define CRYSTAL_FADE_OUT_TO_BLACK   2
#define CRYSTAL_FADE_IN_FROM_BLACK  3

void CrystalFade_Start(int kind);

int  CrystalFade_Tick(void);

int  CrystalFade_Active(void);

void CrystalFade_Reset(void);

void CrystalFade_SetDelay(int frames);
int  CrystalFade_GetDelay(void);
