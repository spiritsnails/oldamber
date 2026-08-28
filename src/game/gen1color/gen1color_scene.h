#pragma once
#include <stdint.h>

#define G1C_EXPBAR_SLOT_BASE 25

void G1CScene_Draw(void);

void G1CScene_DrawGen1(void);

void G1CScene_ResetIntro(void);

int G1CScene_PaletteBlack(void);

int G1CScene_EnemyMonIsOut(void);

int G1CScene_RedOnScreen(void);

int G1CScene_HpPixels(int hp, int max_hp);
