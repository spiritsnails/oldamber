#pragma once

#include <stdint.h>
#include "types.h"

void Trainer_LoadMap(void);

void Trainer_CheckSight(void);

int Trainer_SightTick(void);

int Trainer_IsEngaging(void);

void Trainer_MarkCurrentDefeated(void);

const char *Trainer_PeekPendingAfterBattleScene(void);
void Trainer_ClearPendingAfterBattleScene(void);

void Trainer_EngageImmediate(int npc_idx);

void Trainer_PlayEncounterMusic(uint8_t trainer_class);

extern uint8_t gEngagedTrainerClass;
extern uint8_t gEngagedTrainerNo;

extern uint16_t gEngagedJohtoParty;

extern const char *gTrainerAfterText;

void Trainer_SetDefeatText(int trainer_class, const char *raw);

#define EMOTE_SHOCK     0
#define EMOTE_QUESTION  1
#define EMOTE_SMILE     2

void Emote_ShowOnNPC(int npc_idx);

void Emote_ShowOnPlayer(void);

void Emote_ShowOnPlayerKind(int which);

void Emote_Hide(void);

int Emote_BuildOAM(oam_entry_t out[4]);
