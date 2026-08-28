#pragma once
#include <stdint.h>

#define CRYSTAL_ANIM_NORMAL 0
#define CRYSTAL_ANIM_SLOW   4

#define CRYSTAL_ANIM_OWNER_BATTLE  0
#define CRYSTAL_ANIM_OWNER_SUMMARY 1

void CrystalPicAnim_Start(int dex, int speed);
void CrystalPicAnim_StartOwner(int owner, int dex, int speed);

void CrystalPicAnim_Stop(void);
void CrystalPicAnim_StopOwner(int owner);

void CrystalPicAnim_Tick(void);

int CrystalPicAnim_Running(void);

const uint8_t *CrystalPicAnim_FrameMap(int dex);
