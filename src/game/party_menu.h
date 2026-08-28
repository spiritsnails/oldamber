#pragma once

#include <stdint.h>

#define PARTY_MENU_TMHM 3
#define PARTY_MENU_ITEM_USE 4

void PartyMenu_SetEvoStone(uint8_t stone_item_id);

#define PARTY_MENU_TRADE 5

#define PARTY_MENU_BATTLE_SHIFT 6
void PartyMenu_Open(int force);

void PartyMenu_FinishOpenFade(void);

int  PartyMenu_IsOpen(void);

void PartyMenu_Tick(void);

void PartyMenu_ShowItemUseResult(int slot, uint16_t healed, int success);

void PartyMenu_AnimateItemHeal(int slot, uint16_t old_hp, uint16_t new_hp, uint16_t healed);

void PartyMenu_SetHealResultMessage(const char *line1, const char *line2);
void PartyMenu_ShowRareCandyResult(int slot, int success, int new_level, int evolved);

void PartyMenu_ShowStatRoseResult(int slot, const char *stat, int success);

void PartyMenu_ShowTextResult(const char *line1, const char *line2);

const char *PartyMenu_MonName(int slot);

typedef enum {
    PM_MOVEUSE_CLOSE  = 0,
    PM_MOVEUSE_RELOOP = 1,
} pm_moveuse_disp_t;

typedef pm_moveuse_disp_t (*pm_moveuse_fn)(int slot, int move_index,
                                           char *line1, char *line2);

void PartyMenu_RequestMoveSelect(const char *prompt1, const char *prompt2,
                                 pm_moveuse_fn apply);

void PartyMenu_KeepIconsForEvolution(void);

void PartyMenu_RestoreIcons(void);

void PartyMenu_ClearIcons(void);

int  PartyMenu_GetSelected(void);

int  PartyMenu_GetSavedCursor(void);
