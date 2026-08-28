#pragma once
#include <stdint.h>

void Gen1Color_SetEnabled(int on);
int  Gen1Color_IsEnabled(void);

#define G1C_SPRITES_CRYSTAL 1
#define G1C_SPRITES_GEN1    2
#define G1C_SPRITES_FIRST   G1C_SPRITES_CRYSTAL
#define G1C_SPRITES_LAST    G1C_SPRITES_GEN1
void Gen1Color_SetSpriteStyle(int style);
int  Gen1Color_SpriteStyle(void);

#define G1C_UI_GEN2 0
#define G1C_UI_GEN1 1
void Gen1Color_SetUiStyle(int style);
int  Gen1Color_UiStyle(void);

#define G1C_MONPAL_ENHANCED 0
#define G1C_MONPAL_SGB      2
void Gen1Color_SetMonPalStyle(int style);
int  Gen1Color_MonPalStyle(void);

void Gen1Color_LoadBattleGfx(void);

void Gen1Color_Tick(void);

int  Gen1Color_ExpBarPixels(void);

int  Gen1Color_ExpBarPixelsShown(void);

void Gen1Color_ResetExpBar(void);
