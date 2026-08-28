#pragma once

#include <stdint.h>

#define CRYSTAL_HP_GREEN  0
#define CRYSTAL_HP_YELLOW 1
#define CRYSTAL_HP_RED    2

#define CRYSTAL_ICON_SLOTS 6

void CrystalIconAnim_Init(int slot, int hp_color);

void CrystalIconAnim_Reset(void);

void CrystalIconAnim_Tick(void);

int  CrystalIconAnim_Frame(int slot);

void CrystalIconAnim_SetSelected(int slot);

void CrystalIconAnim_Offset(int slot, int *dx, int *dy);

int  CrystalIcon_ForDex(int dex);
