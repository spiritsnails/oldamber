#pragma once
#include <stddef.h>
#include <stdint.h>

void BattleUI_Enter(void);

void BattleUI_Restore(void);

void BattleUI_Tick(void);

int BattleUI_IsActive(void);

int BattleUI_GetState(void);

int BattleUI_IsEvolutionScreen(void);

int BattleUI_IsAtActionMenu(void);

int BattleUI_GetSavedMenuItem(void);

int BattleUI_GetTurnCount(void);

void BattleUI_SetTurnLimit(int n);

int BattleUI_SnapshotHUD(char *out, size_t outsz);

int BattleUI_BeginPendingEvolution(void);

int  BattleUI_CenterMonNameOffset(int len);

void BattleUI_SetPlayerNameCentered(int on);

int  BattleUI_PlayerMonIsOut(void);

uint8_t BattleUI_EnemyPicSpecies(void);

void BattleUI_SetBadgeRecvText(const char *text);

void BattleUI_SetBadgeInfoText(const char *text);

uint16_t BattleUI_GetAnimOAMStart(void);
uint16_t BattleUI_GetAnimOAMEnd(void);

uint16_t BattleUI_GetEnemyOAMStart(void);
uint16_t BattleUI_GetEnemyOAMEnd(void);

void BattleUI_EnemySpriteCaptureState(void);
void BattleUI_EnemySpriteSetVisible(uint8_t visible);

int BattleUI_EnemyDrawnAsGhost(void);
uint8_t BattleUI_IsEnemySpriteVisible(void);
void BattleUI_EnemySpriteOffsetY(int8_t delta);

int BattleUI_HpBarAnim(int side, int *px_out, int *hp_out, int *max_out);

int BattleUI_PoofRomTile(uint8_t sprite_tile);

int BattleUI_HudOverlayActive(void);

#define BUI_ENEMY_PIC_NONE    0
#define BUI_ENEMY_PIC_MON     1
#define BUI_ENEMY_PIC_TRAINER 2
#define BUI_ENEMY_PIC_GHOST   3
int BattleUI_EnemyPicKind(void);

int BattleUI_EnemyMonOnField(void);

#include "../../data/hof_player_sprites.h"
#define gRedBackSprite kHofRedBackSprite

const uint8_t *BattleUI_PlayerBackTile(int i);
