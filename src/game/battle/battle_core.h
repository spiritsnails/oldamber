#pragma once

#include <stdint.h>

typedef enum {
    BATTLE_EVENT_NONE = 0,
    BATTLE_EVENT_PLAY_ANIM = 1,
    BATTLE_EVENT_STATUS_MSG = 2,
    BATTLE_EVENT_HIT_SFX = 3,
    BATTLE_EVENT_HP_TARGET = 4,
    BATTLE_EVENT_MOVE_RESULT = 5,
    BATTLE_EVENT_RESIDUAL_MSG = 6,
    BATTLE_EVENT_STAT_MOD_TEXT = 7,
    BATTLE_EVENT_EFFECT_MSG = 8,
} battle_event_type_t;

typedef enum {
    BATTLE_EFFECT_MSG_NONE = 0,
    BATTLE_EFFECT_MSG_CONVERTED_TYPE,
    BATTLE_EFFECT_MSG_HAZE,
    BATTLE_EFFECT_MSG_COINS_SCATTERED,
    BATTLE_EFFECT_MSG_GAINED_ARMOR,
    BATTLE_EFFECT_MSG_LIGHT_SCREEN,
    BATTLE_EFFECT_MSG_SUBSTITUTE_MADE,
    BATTLE_EFFECT_MSG_SUBSTITUTE_TOO_WEAK,
    BATTLE_EFFECT_MSG_HIT_WITH_RECOIL,
    BATTLE_EFFECT_MSG_RAN_FROM_BATTLE,
    BATTLE_EFFECT_MSG_RAGE_BUILDING,
    BATTLE_EFFECT_MSG_MIRROR_MOVE_FAILED,
    BATTLE_EFFECT_MSG_CRASHED,
    BATTLE_EFFECT_MSG_TRANSFORMED,
    BATTLE_EFFECT_MSG_MOVE_DISABLED,
    BATTLE_EFFECT_MSG_LEARNED_MOVE,
    BATTLE_EFFECT_MSG_SUBSTITUTE_TOOK_DAMAGE,
    BATTLE_EFFECT_MSG_HIT_N_TIMES,
    BATTLE_EFFECT_MSG_FIRE_DEFROSTED,
    BATTLE_EFFECT_MSG_HAS_SUBSTITUTE,
    BATTLE_EFFECT_MSG_STARTED_SLEEPING,
    BATTLE_EFFECT_MSG_FELL_ASLEEP_HEALTHY,
    BATTLE_EFFECT_MSG_REGAINED_HEALTH,
    BATTLE_EFFECT_MSG_SUBSTITUTE_BROKE,
    BATTLE_EFFECT_MSG_NOTHING_HAPPENED,
} battle_effect_msg_t;

typedef enum {
    BATTLE_MOVE_RESULT_NONE = 0,

    BATTLE_MOVE_RESULT_MISS = 1,
    BATTLE_MOVE_RESULT_NO_EFFECT = 2,
    BATTLE_MOVE_RESULT_UNAFFECTED = 3,

    BATTLE_MOVE_RESULT_DIDNT_AFFECT = 4,
    BATTLE_MOVE_RESULT_ALREADY_ASLEEP = 5,
    BATTLE_MOVE_RESULT_BUT_IT_FAILED = 6,

    BATTLE_MOVE_RESULT_NO_EFFECT_PLAIN = 7,

    BATTLE_MOVE_RESULT_EVADED = 8,
} battle_move_result_t;
#define BATTLE_MOVE_RESULT_MAX BATTLE_MOVE_RESULT_EVADED

#define BATTLE_RESULT_SUPPRESSES_ANIM(r) \
    ((r) != BATTLE_MOVE_RESULT_NONE && (r) != BATTLE_MOVE_RESULT_NO_EFFECT_PLAIN)

typedef enum {
    BATTLE_RESIDUAL_MSG_NONE = 0,
    BATTLE_RESIDUAL_MSG_BURN = 1,
    BATTLE_RESIDUAL_MSG_POISON = 2,
    BATTLE_RESIDUAL_MSG_LEECH_SEED = 3,
} battle_residual_msg_t;

typedef struct {
    uint8_t type;
    uint8_t arg0;
    uint8_t arg1;
    uint8_t arg2;
} battle_event_t;

void BattleEvent_ResetTurnQueue(void);
int BattleEvent_Pop(battle_event_t *out);
int BattleEvent_HasPending(void);
void BattleEvent_PushMoveResult(uint8_t result);
void BattleEvent_PushStatModText(uint8_t stat_idx, uint8_t side, uint8_t down, uint8_t greatly);

void BattleEvent_PushEffectMsg(uint8_t msg, uint8_t side, uint8_t extra);

void BattleEvent_PushPlayAnim(uint8_t anim_id, uint8_t forced_turn);

void BattleEvent_PushHPTarget(uint8_t side);

void Battle_ExecutePlayerMove(void);

void Battle_ExecuteEnemyMove(void);

int Battle_HandlePoisonBurnLeechSeed(void);

void Battle_HandlePlayerMonFainted(void);

void Battle_HandleEnemyMonFainted(void);

typedef enum {
    BSTAT_MSG_NONE = 0,
    BSTAT_MSG_FAST_ASLEEP,
    BSTAT_MSG_WOKE_UP,
    BSTAT_MSG_FROZEN,
    BSTAT_MSG_CANT_MOVE,
    BSTAT_MSG_FLINCHED,
    BSTAT_MSG_MUST_RECHARGE,
    BSTAT_MSG_HURT_ITSELF,
    BSTAT_MSG_FULLY_PARALYZED,
    BSTAT_MSG_MOVE_DISABLED,
    BSTAT_MSG_IS_CONFUSED,
    BSTAT_MSG_DISABLED_NO_MORE,
    BSTAT_MSG_CONFUSED_NO_MORE,
    BSTAT_MSG_TOO_SCARED,
    BSTAT_MSG_GET_OUT,
} battle_status_msg_t;

int Battle_MapIsPokemonTower(void);

int Battle_IsGhostBattle(void);

battle_status_msg_t Battle_GetPlayerStatusMsg(void);
battle_status_msg_t Battle_GetPlayerPreStatusMsg(void);

battle_status_msg_t Battle_GetEnemyStatusMsg(void);
battle_status_msg_t Battle_GetEnemyPreStatusMsg(void);

uint8_t Battle_GetHitHpLog(int whose, const uint16_t **out);
uint8_t  Battle_GetLastPlayerHitCount(void);
uint16_t Battle_GetLastPlayerFirstTargetHP(void);
uint8_t  Battle_GetLastEnemyHitCount(void);
uint16_t Battle_GetLastEnemyFirstTargetHP(void);

uint8_t Battle_GetPlayerStatusAffectedAnimPending(void);
uint8_t Battle_GetEnemyStatusAffectedAnimPending(void);

typedef enum {
    BATTLE_ANNOUNCE_USED_MOVE = 0,
    BATTLE_ANNOUNCE_THRASHING,
    BATTLE_ANNOUNCE_ATTACK_CONTINUES,
    BATTLE_ANNOUNCE_UNLEASHED_ENERGY,
} battle_announce_t;

battle_announce_t Battle_GetPlayerAnnounce(void);
battle_announce_t Battle_GetEnemyAnnounce(void);

uint8_t Battle_GetLastCrit(void);

void Battle_GetCurrentMove(void);

uint8_t Battle_GetPlayerReplacedMove(void);
uint8_t Battle_GetEnemyReplacedMove(void);

uint8_t Battle_SideExecutedMove(int whose);

uint8_t Battle_GetPlayerStatusAnimId(void);
uint8_t Battle_GetEnemyStatusAnimId(void);
uint8_t Battle_GetPlayerConfusionSelfHitAnimPending(void);
uint8_t Battle_GetEnemyConfusionSelfHitAnimPending(void);
