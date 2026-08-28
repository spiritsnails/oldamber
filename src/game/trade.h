#pragma once
#include <stdint.h>
#include "constants.h"

typedef struct {
    uint8_t  player_species;
    uint8_t  enemy_species;
    char     player_ot[NAME_LENGTH];
    char     enemy_ot[NAME_LENGTH];
    uint16_t player_otid;
    uint16_t enemy_otid;
    char     enemy_trainer[NAME_LENGTH];
} trade_anim_data_t;

extern trade_anim_data_t gTradeAnim;

void TradeAnim_Begin(void);
int  TradeAnim_IsActive(void);
void TradeAnim_Tick(void);

#define TRADE_DIALOGSET_CASUAL     0
#define TRADE_DIALOGSET_EVOLUTION  1
#define TRADE_DIALOGSET_HAPPY      2
#define NUM_TRADE_DIALOGSETS       3

#define TRADE_FOR_TERRY      0
#define TRADE_FOR_MARCEL     1
#define TRADE_FOR_CHIKUCHIKU 2
#define TRADE_FOR_SAILOR     3
#define TRADE_FOR_DUX        4
#define TRADE_FOR_MARC       5
#define TRADE_FOR_LOLA       6
#define TRADE_FOR_DORIS      7
#define TRADE_FOR_CRINKLES   8
#define TRADE_FOR_SPOT       9
#define NUM_NPC_TRADES      10

typedef struct {
    uint8_t     give;
    uint8_t     get;
    uint8_t     dialogset;
    const char *nick;
} npc_trade_t;

extern const npc_trade_t gTradeMons[NUM_NPC_TRADES];

int  Trade_Begin(uint8_t which);
int  Trade_IsActive(void);
void Trade_Tick(void);

#define TRADE_RESULT_CANCELLED 0
#define TRADE_RESULT_TRADED    1
#define TRADE_RESULT_WRONG_MON 2

int  Trade_BeginCustom(uint8_t give, uint8_t get, const char *nick);
int  Trade_GetResult(void);

void Trade_Abort(void);
