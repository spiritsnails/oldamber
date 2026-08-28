#pragma once
#include <stdint.h>

void Player_Init(uint8_t start_x, uint8_t start_y);
void Player_SetPos(int16_t x, int16_t y);
void Player_Update(void);

void Player_ForceStepDown(void);

void Player_ForceStepFromDoor(void);

void Player_ForceStepFromDoorFacingUp(void);

void Player_DoScriptedStep(int dir);

void Player_DoScriptedStepWithLedge(int dir);

extern int gScriptedMovement;

extern int gPlayerFacing;

extern int gScrollPxX;
extern int gScrollPxY;

void Player_GetFacingTile(int *out_x, int *out_y);

int  Player_IsTurning(void);

int Player_GetLedgeDir(uint8_t tile_id);

int Player_IsMoving(void);

int Player_IsLedgeJumping(void);

typedef enum {
    PLAYER_SURF_STEP_INVALID = 0,
    PLAYER_SURF_STEP_WATER,
    PLAYER_SURF_STEP_LAND,
} player_surf_step_t;

player_surf_step_t Player_ClassifySurfStep(int nx, int ny);

extern int gStepJustCompleted;

void Player_SyncOAM(void);

void Player_SetFishingPose(int on);

void Player_HideIfOverUI(void);

extern int gNoClip;

void Player_IgnoreInputFrames(int n);

void Player_StartSimulatedMovement(const int8_t *seq, int last_idx);

int Player_IsSimulatingMovement(void);

int Player_GetSimulatedStepsRemaining(void);

int Player_GetSimulatedHeldDir(void);

void Player_SetSpinnerSpin(int enabled);

void Player_SetWarpSpin(int enabled);
void Player_WarpSpinStep(void);
void Player_SetWarpSpinY(int dy);

void Player_SetHoldBSprintEnabled(int enabled);
int  Player_GetHoldBSprintEnabled(void);

int Player_ConsumePushedBoulderEvent(uint8_t *out_map, int *out_x, int *out_y);
