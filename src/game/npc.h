#pragma once

#include <stdint.h>

#define NPC_STATE_MAX 16

typedef struct npc_state_t {
    int      npc_count;
    int      npc_last_interacted;
    uint8_t  npc_sprite[NPC_STATE_MAX];
    uint8_t  npc_x[NPC_STATE_MAX];
    uint8_t  npc_y[NPC_STATE_MAX];
    uint8_t  npc_facing[NPC_STATE_MAX];
    uint8_t  npc_move_type[NPC_STATE_MAX];
    int      npc_move_timer[NPC_STATE_MAX];
    int      npc_walk_frames[NPC_STATE_MAX];
    int      npc_walk_total[NPC_STATE_MAX];
    int      npc_step_px[NPC_STATE_MAX];
    int      npc_px_off[NPC_STATE_MAX];
    int      npc_py_off[NPC_STATE_MAX];
    uint8_t  npc_hidden[NPC_STATE_MAX];
} npc_state_t;

void NPC_Load(void);

void NPC_ReloadTiles(void);

void NPC_Update(void);

void NPC_BuildView(int scroll_px_x, int scroll_px_y);

void NPC_FacePlayer(int npc_idx);

int NPC_IsBlocked(int nx, int ny);

int NPC_FindAtTile(int tx, int ty);

int NPC_FindAtTileIncludingHidden(int tx, int ty);
int NPC_GetSpriteId(int i);

int NPC_SpriteCanFacePlayer(uint8_t sprite_id);

void NPC_HideSprite(int npc_slot_idx);

void NPC_ShowSprite(int npc_slot_idx);

void NPC_DebugDump(void);

int  NPC_GetMoveType(int npc_idx);
void NPC_SetMoveType(int npc_idx, int move_type);

void NPC_HideAll(void);
void NPC_ShowAll(void);

void NPC_HideOAM(void);

void NPC_HideOverUITiles(void);

void NPC_SetFacing(int i, int facing);
int  NPC_GetFacing(int i);

int NPC_GetLastInteracted(void);

void NPC_GetScreenPos(int i, int *px, int *py);

int NPC_GetScreenTopLeft(int i, int *px, int *py);

void NPC_GetTilePos(int i, int *tx, int *ty);

int NPC_GetDeclIdx(int i);

void NPC_SetTilePos(int i, int tx, int ty);

int NPC_GetCount(void);

int NPC_IsWalking(int i);

int NPC_IsHidden(int i);

void NPC_DoScriptedStep(int i, int dir);

void NPC_DoScriptedStepTimed(int i, int dir, int walk_frames, int step_px);

int  NPC_DebugSpawn(uint8_t sprite_id, int tx, int ty, int facing, int move_type);
void NPC_DebugDespawn(int i);

void NPC_StateCapture(npc_state_t *out);
void NPC_StateRestore(const npc_state_t *in);
