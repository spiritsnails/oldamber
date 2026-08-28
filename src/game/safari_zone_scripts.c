#include "safari_zone_scripts.h"
#include "rom_text.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "constants.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "warp.h"
#include "yesno.h"
#include "../data/event_constants.h"
#include "../data/font_data.h"
#include "../data/map_data.h"
#include "overworld.h"
#include "amberscript_mapbank.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MAP_SAFARI_ZONE_GATE               0x9c
#define MAP_SAFARI_ZONE_EAST               0xd9
#define MAP_SAFARI_ZONE_NORTH              0xda
#define MAP_SAFARI_ZONE_WEST               0xdb
#define MAP_SAFARI_ZONE_CENTER             0xdc
#define MAP_SAFARI_ZONE_CENTER_REST_HOUSE  0xdd
#define MAP_SAFARI_ZONE_SECRET_HOUSE       0xde
#define MAP_SAFARI_ZONE_WEST_REST_HOUSE    0xdf
#define MAP_SAFARI_ZONE_EAST_REST_HOUSE    0xe0
#define MAP_SAFARI_ZONE_NORTH_REST_HOUSE   0xe1

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

typedef enum {
    SCRIPT_SAFARIZONEGATE_DEFAULT = 0,
    SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_RIGHT,
    SCRIPT_SAFARIZONEGATE_WOULD_YOU_LIKE_TO_JOIN,
    SCRIPT_SAFARIZONEGATE_PLAYER_MOVING,
    SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_DOWN,
    SCRIPT_SAFARIZONEGATE_LEAVING_SAFARI,
    SCRIPT_SAFARIZONEGATE_SET_SCRIPT_AFTER_MOVE,
} safari_gate_script_t;

typedef enum {
    AT_NONE = 0,
    AT_SHOW_JOIN_PROMPT,
    AT_OPEN_JOIN_PROMPT,
    AT_SHOW_LEAVE_PROMPT,
    AT_OPEN_LEAVE_PROMPT,
    AT_SHOW_WORKER2_PROMPT,
    AT_OPEN_WORKER2_PROMPT,
    AT_MOVE_RIGHT1_THEN_JOIN,
    AT_MOVE_UP3,
    AT_MOVE_DOWN1_TO_DEFAULT,
    AT_MOVE_DOWN3_TO_DEFAULT,
    AT_MOVE_UP1_TO_LEAVING,
    AT_SHOW_GAME_OVER_TEXT,
    AT_WARP_GAME_OVER_TO_GATE,
    AT_PLAY_GET_ITEM_AND_SHOW_PA_TEXT,
} after_text_action_t;

typedef enum {
    PROMPT_NONE = 0,
    PROMPT_JOIN,
    PROMPT_LEAVE,
    PROMPT_WORKER2,
} prompt_state_t;

static uint8_t s_after_move_delay = 0;
static after_text_action_t s_after_text = AT_NONE;
static prompt_state_t s_prompt_state = PROMPT_NONE;
static uint8_t s_game_over_show_times_up = 0;

#define CHAR_TERM  0x50u
#define CHAR_SPACE 0x7Fu
#define CHAR_YEN   0xF0u
#define BC_TL      0x79u
#define BC_H       0x7Au
#define BC_TR      0x7Bu
#define BC_V       0x7Cu
#define BC_BL      0x7Du
#define BC_BR      0x7Eu

#define MONEY_L          11
#define MONEY_R          19
#define MONEY_T           0
#define MONEY_B           2
#define MONEY_LABEL_COL  13
#define MONEY_YEN_COL    12
#define MONEY_DIGIT_COL  13
#define MONEY_ROW         1

static const uint8_t kStrMoney[] = {0x8C, 0x8E, 0x8D, 0x84, 0x98, CHAR_TERM};

static const int8_t kSafariWalkRight1[] = { DIR_RIGHT, -1 };
static const int8_t kSafariWalkUp3[]    = { DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kSafariWalkDown1[]  = { DIR_DOWN, -1 };
static const int8_t kSafariWalkDown3[]  = { DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };
static const int8_t kSafariWalkUp1[]    = { DIR_UP, -1 };

static const char *const kSafariInteriorVmapNames[] = {
    "SafariZoneEast", "SafariZoneNorth", "SafariZoneWest", "SafariZoneCenter",
    "SafariZoneCenterRestHouse", "SafariZoneSecretHouse",
    "SafariZoneWestRestHouse", "SafariZoneEastRestHouse", "SafariZoneNorthRestHouse",
};

static const char *const kSafariWildVmapNames[] = {
    "SafariZoneEast", "SafariZoneNorth", "SafariZoneWest", "SafariZoneCenter",
};

static int vmap_name_in_list(uint8_t map, const char *const *names, int count) {
    if (map < PKS_VIRTUAL_MAP_FIRST || map > PKS_VIRTUAL_MAP_LAST) return 0;
    const char *n = AmberScript_MapBank_NameForRealId(map);
    if (!n) return 0;

    for (int i = 0; i < count; i++)
        if (strcasecmp(n, names[i]) == 0) return 1;
    return 0;
}

static int map_is_safari_interior(uint8_t map) {
    if (map >= MAP_SAFARI_ZONE_EAST && map <= MAP_SAFARI_ZONE_NORTH_REST_HOUSE) return 1;
    return vmap_name_in_list(map, kSafariInteriorVmapNames,
                              (int)(sizeof(kSafariInteriorVmapNames) / sizeof(kSafariInteriorVmapNames[0])));
}

int SafariZoneScripts_MapShowsStepCounter(uint8_t map) {
    return map_is_safari_interior(map);
}

int SafariZoneScripts_MapHasWildEncounters(uint8_t map) {
    if (map >= MAP_SAFARI_ZONE_EAST && map < MAP_SAFARI_ZONE_CENTER_REST_HOUSE) return 1;
    return vmap_name_in_list(map, kSafariWildVmapNames,
                              (int)(sizeof(kSafariWildVmapNames) / sizeof(kSafariWildVmapNames[0])));
}

static uint32_t bcd_money_to_u32(const uint8_t m[3]) {
    return ((m[0] >> 4) & 0xF) * 100000u + (m[0] & 0xF) * 10000u +
           ((m[1] >> 4) & 0xF) * 1000u + (m[1] & 0xF) * 100u +
           ((m[2] >> 4) & 0xF) * 10u + (m[2] & 0xF);
}

#define TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())

static void safari_set_tile(int row, int col, uint8_t ch) {
    gScrollTileMap[TMIDX(row, col)] = (uint8_t)Font_CharToTile(ch);
}

static void safari_draw_pstr(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++) safari_set_tile(row, col, *s);
}

static void safari_draw_box(int box_l, int box_t, int box_r, int box_b) {
    safari_set_tile(box_t, box_l, BC_TL);
    for (int c = box_l + 1; c < box_r; c++) safari_set_tile(box_t, c, BC_H);
    safari_set_tile(box_t, box_r, BC_TR);
    for (int r = box_t + 1; r < box_b; r++) {
        safari_set_tile(r, box_l, BC_V);
        safari_set_tile(r, box_r, BC_V);
    }
    safari_set_tile(box_b, box_l, BC_BL);
    for (int c = box_l + 1; c < box_r; c++) safari_set_tile(box_b, c, BC_H);
    safari_set_tile(box_b, box_r, BC_BR);
}

static void safari_clear_box_interior(int box_l, int box_t, int box_r, int box_b) {
    for (int r = box_t + 1; r < box_b; r++) {
        for (int c = box_l + 1; c < box_r; c++) safari_set_tile(r, c, CHAR_SPACE);
    }
}

static void safari_draw_digits(int col, int row, uint32_t v, int width) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%*u", width, (unsigned)v);
    for (int i = 0; i < width; i++) {
        char ch = buf[i];
        if (ch >= '0' && ch <= '9') safari_set_tile(row, col + i, (uint8_t)(0xF6u + (ch - '0')));
        else safari_set_tile(row, col + i, CHAR_SPACE);
    }
}

static void safari_draw_money_box(void) {
    safari_draw_box(MONEY_L, MONEY_T, MONEY_R, MONEY_B);
    safari_clear_box_interior(MONEY_L, MONEY_T, MONEY_R, MONEY_B);
    safari_draw_pstr(MONEY_LABEL_COL, MONEY_T, kStrMoney);
    safari_set_tile(MONEY_ROW, MONEY_YEN_COL, CHAR_YEN);
    safari_draw_digits(MONEY_DIGIT_COL, MONEY_ROW, bcd_money_to_u32(wPlayerMoney), 6);
}

static void u32_to_bcd_money(uint32_t v, uint8_t m[3]) {
    if (v > 999999u) v = 999999u;
    m[0] = (uint8_t)(((v / 100000u) << 4) | ((v / 10000u) % 10u));
    m[1] = (uint8_t)((((v / 1000u) % 10u) << 4) | ((v / 100u) % 10u));
    m[2] = (uint8_t)((((v / 10u) % 10u) << 4) | (v % 10u));
}

static void safari_gate_clear_prompt(void) {
    s_prompt_state = PROMPT_NONE;
}

static int safari_seq_last_idx(const int8_t *seq) {
    int i = 0;
    while (seq[i] != -1) i++;
    return i - 1;
}

static void safari_gate_start_move(const int8_t *seq) {
    wJoyIgnore = PAD_CTRL_PAD;
    Player_StartSimulatedMovement(seq, safari_seq_last_idx(seq));
}

static int safari_gate_continue_move(void) {
    return Player_IsSimulatingMovement() || Player_IsMoving();
}

static void safari_zone_game_over(void) {
    if (wSafariZoneGameOver) return;

    Audio_PlaySFX_SafariZonePA();
    s_game_over_show_times_up = (wNumSafariBalls > 0);
    if (s_game_over_show_times_up) {
        Text_ShowASCII(RomText("TimesUpText"));
    } else {
        Text_ShowASCII(RomText("StartBattle.outOfSafariBallsText"));
    }
    s_after_text = AT_SHOW_GAME_OVER_TEXT;
    wPlayerMovingDirection = DIR_DOWN;
    SetEvent(EVENT_SAFARI_GAME_OVER);
    wSafariZoneGameOver = 1;
    wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_LEAVING_SAFARI;
}

static void safari_zone_game_over_check(void) {
    if (!CheckEvent(EVENT_IN_SAFARI_ZONE)) {
        wSafariZoneGameOver = 0;
        return;
    }

    if (!map_is_safari_interior(wCurMap)) {
        wSafariZoneGameOver = 0;
        return;
    }
    if (wSafariZoneGameOver) return;
    if (wSafariSteps == 0 || wNumSafariBalls == 0) {
        safari_zone_game_over();
    } else {
        wSafariZoneGameOver = 0;
    }
}

static void safari_zone_open_join_prompt(void) {

    static char buf[128];
    s_prompt_state = PROMPT_JOIN;
    if (!buf[0])
        snprintf(buf, sizeof buf, "%s\f%s",
                 RomTextPage("_SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText", 0),
                 RomTextPage("_SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText", 1));
    YesNo_Show(buf);
}

static void safari_zone_open_leave_prompt(void) {
    s_prompt_state = PROMPT_LEAVE;
    YesNo_Show(RomText("_SafariZoneGateSafariZoneWorker1LeavingEarlyText"));
}

static void safari_zone_open_worker2_prompt(void) {
    s_prompt_state = PROMPT_WORKER2;
    YesNo_Show(RomText("SafariZoneGateSafariZoneWorker2Text.FirstTimeHereText"));
}

static void safari_zone_handle_after_text(void) {
    if (s_after_text == AT_NONE) return;
    if (Text_IsOpen()) return;

    switch (s_after_text) {
    case AT_SHOW_JOIN_PROMPT:
        wJoyIgnore = 0;
        hJoyHeld = 0;
        s_after_text = AT_OPEN_JOIN_PROMPT;
        return;

    case AT_OPEN_JOIN_PROMPT:
        wJoyIgnore = 0;
        hJoyHeld = 0;
        safari_zone_open_join_prompt();
        s_after_text = AT_NONE;
        return;

    case AT_SHOW_LEAVE_PROMPT:
        wJoyIgnore = 0;
        hJoyHeld = 0;
        s_after_text = AT_OPEN_LEAVE_PROMPT;
        return;

    case AT_OPEN_LEAVE_PROMPT:
        wJoyIgnore = 0;
        hJoyHeld = 0;
        safari_zone_open_leave_prompt();
        s_after_text = AT_NONE;
        return;

    case AT_SHOW_WORKER2_PROMPT:
        wJoyIgnore = 0;
        hJoyHeld = 0;
        s_after_text = AT_OPEN_WORKER2_PROMPT;
        return;

    case AT_OPEN_WORKER2_PROMPT:
        wJoyIgnore = 0;
        hJoyHeld = 0;
        safari_zone_open_worker2_prompt();
        s_after_text = AT_NONE;
        return;

    case AT_MOVE_RIGHT1_THEN_JOIN:
        safari_gate_start_move(kSafariWalkRight1);
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_RIGHT;
        s_after_text = AT_NONE;
        return;

    case AT_MOVE_UP3:
        safari_gate_start_move(kSafariWalkUp3);
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_PLAYER_MOVING;
        s_after_text = AT_NONE;
        return;

    case AT_MOVE_DOWN1_TO_DEFAULT:
        safari_gate_start_move(kSafariWalkDown1);
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_DOWN;
        s_after_text = AT_NONE;
        return;

    case AT_MOVE_DOWN3_TO_DEFAULT:
        safari_gate_start_move(kSafariWalkDown3);
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_DOWN;
        s_after_text = AT_NONE;
        return;

    case AT_MOVE_UP1_TO_LEAVING:
        safari_gate_start_move(kSafariWalkUp1);
        wNextSafariZoneGateScript = SCRIPT_SAFARIZONEGATE_LEAVING_SAFARI;
        s_after_move_delay = 3;
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_SET_SCRIPT_AFTER_MOVE;
        s_after_text = AT_NONE;
        return;

    case AT_SHOW_GAME_OVER_TEXT:
        Text_ShowASCII(RomText("_GameOverText"));
        s_after_text = AT_WARP_GAME_OVER_TO_GATE;
        return;

    case AT_WARP_GAME_OVER_TO_GATE:

        Warp_QueueTeleportVmap("SafariZoneGate", 4, 0);
        s_after_text = AT_NONE;
        return;

    case AT_PLAY_GET_ITEM_AND_SHOW_PA_TEXT:
        Audio_PlaySFX_GetItem1();
        {
            static char buf[128];
            if (!buf[0])
                snprintf(buf, sizeof buf, "%s\f%s",
                         RomTextPage("SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText.MakePaymentText", 2),
                         RomTextPage("SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText.MakePaymentText", 3));
            Text_ShowASCII(buf);
        }
        s_after_text = AT_MOVE_UP3;
        return;

    case AT_NONE:
    default:
        s_after_text = AT_NONE;
        return;
    }
}

static void safari_zone_handle_prompt(void) {
    if (s_prompt_state == PROMPT_NONE) return;
    if (YesNo_IsOpen()) return;

    const int result = YesNo_GetResult() ? 1 : 0;
    prompt_state_t prompt = s_prompt_state;
    safari_gate_clear_prompt();

    switch (prompt) {
    case PROMPT_JOIN:
        if (!result) {
            Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText.PleaseComeAgainText"));
            s_after_text = AT_MOVE_DOWN1_TO_DEFAULT;
            return;
        }
        {
            uint32_t money = bcd_money_to_u32(wPlayerMoney);
            if (money < 500u) {
                Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText.NotEnoughMoneyText"));
                s_after_text = AT_MOVE_DOWN1_TO_DEFAULT;
                return;
            }
            money -= 500u;
            u32_to_bcd_money(money, wPlayerMoney);
            wNumSafariBalls = 30;
            wSafariSteps = 502;
            SetEvent(EVENT_IN_SAFARI_ZONE);
            ClearEvent(EVENT_SAFARI_GAME_OVER);
            wSafariZoneGameOver = 0;
            {
                static char buf[96];
                if (!buf[0])
                    snprintf(buf, sizeof buf, "%s\f%s",
                             RomTextPage("SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText.MakePaymentText", 0),
                             RomTextPage("SafariZoneGateSafariZoneWorker1WouldYouLikeToJoinText.MakePaymentText", 1));
                Text_ShowASCII(buf);
            }
            s_after_text = AT_PLAY_GET_ITEM_AND_SHOW_PA_TEXT;
        }
        return;

    case PROMPT_LEAVE:
        if (result) {
            Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker1LeavingEarlyText.ReturnSafariBallsText"));
            ClearEvent(EVENT_SAFARI_GAME_OVER);
            ClearEvent(EVENT_IN_SAFARI_ZONE);
            wSafariZoneGameOver = 0;
            safari_gate_start_move(kSafariWalkDown3);
            wNextSafariZoneGateScript = SCRIPT_SAFARIZONEGATE_DEFAULT;
            s_after_move_delay = 3;
            wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_SET_SCRIPT_AFTER_MOVE;
        } else {
            Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker1LeavingEarlyText.GoodLuckText"));
            s_after_text = AT_MOVE_UP1_TO_LEAVING;
        }
        return;

    case PROMPT_WORKER2:
        if (!result) {
            Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker2Text.SafariZoneExplanationText"));
        } else {
            Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker2Text.YoureARegularHereText"));
        }
        return;

    case PROMPT_NONE:
    default:
        return;
    }
}

static void safari_zone_default_trigger(void) {
    if (wCurMap != MAP_SAFARI_ZONE_GATE) return;
    if (wSafariZoneGateCurScript != SCRIPT_SAFARIZONEGATE_DEFAULT) return;
    if (Text_IsOpen() || YesNo_IsOpen() || Player_IsMoving()) return;
    if (!((wXCoord == 3 && wYCoord == 2) || (wXCoord == 4 && wYCoord == 2))) return;

    Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker1Text"));
    hJoyHeld = 0;
    gPlayerFacing = DIR_RIGHT;
    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);

    if (wXCoord == 3) {
        s_after_text = AT_MOVE_RIGHT1_THEN_JOIN;
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_RIGHT;
    } else {
        s_after_text = AT_NONE;
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_WOULD_YOU_LIKE_TO_JOIN;
    }
}

static void safari_zone_gate_tick_script(void) {
restart:
    switch (wSafariZoneGateCurScript) {
    case SCRIPT_SAFARIZONEGATE_DEFAULT:
        return;

    case SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_RIGHT:
        if (safari_gate_continue_move()) return;
        wJoyIgnore = 0;
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_WOULD_YOU_LIKE_TO_JOIN;
        goto restart;

    case SCRIPT_SAFARIZONEGATE_WOULD_YOU_LIKE_TO_JOIN:
        if (s_prompt_state == PROMPT_NONE && !Text_IsOpen() && !YesNo_IsOpen()) {
            wJoyIgnore = 0;
            hJoyHeld = 0;
            safari_zone_open_join_prompt();
        }
        return;

    case SCRIPT_SAFARIZONEGATE_PLAYER_MOVING:
        if (safari_gate_continue_move()) return;
        wJoyIgnore = 0;
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_LEAVING_SAFARI;
        goto restart;

    case SCRIPT_SAFARIZONEGATE_PLAYER_MOVING_DOWN:
        if (safari_gate_continue_move()) return;
        wJoyIgnore = 0;
        wSafariZoneGateCurScript = SCRIPT_SAFARIZONEGATE_DEFAULT;
        return;

    case SCRIPT_SAFARIZONEGATE_LEAVING_SAFARI:
        gPlayerFacing = DIR_DOWN;
        if (CheckEvent(EVENT_SAFARI_GAME_OVER)) {
            ClearEvent(EVENT_SAFARI_GAME_OVER);
            ClearEvent(EVENT_IN_SAFARI_ZONE);
            wNumSafariBalls = 0;
            Text_ShowASCII(RomText("SafariZoneGateSafariZoneWorker1GoodHaulComeAgainText"));
            s_after_text = AT_MOVE_DOWN3_TO_DEFAULT;
            return;
        }
        if (s_prompt_state == PROMPT_NONE && !Text_IsOpen()) {
            wJoyIgnore = 0;
            hJoyHeld = 0;
            safari_zone_open_leave_prompt();
        }
        return;

    case SCRIPT_SAFARIZONEGATE_SET_SCRIPT_AFTER_MOVE:
        if (safari_gate_continue_move()) return;
        if (s_after_move_delay > 0) {
            s_after_move_delay--;
            return;
        }
        wSafariZoneGateCurScript = wNextSafariZoneGateScript;
        goto restart;
    }
}

int SafariZoneScripts_IsActive(void) {

    if (wCurMap != MAP_SAFARI_ZONE_GATE) return 0;
    return wSafariZoneGateCurScript != SCRIPT_SAFARIZONEGATE_DEFAULT ||
           s_after_text != AT_NONE ||
           s_prompt_state != PROMPT_NONE ||
           wSafariZoneGameOver != 0;
}

void SafariZoneScripts_OnMapLoad(void) {

    if (!map_is_safari_interior(wCurMap) && wCurMap != MAP_SAFARI_ZONE_GATE) {
        wSafariZoneGameOver = 0;
    }
}

void SafariZoneScripts_GateStepCheck(void) {
    if (wCurMap != MAP_SAFARI_ZONE_GATE) return;
    safari_zone_default_trigger();
}

void SafariZoneScripts_StepCheck(void) {
    if (!CheckEvent(EVENT_IN_SAFARI_ZONE)) return;
    if (!map_is_safari_interior(wCurMap)) return;
    if (wSafariSteps == 0) {
        safari_zone_game_over();
        return;
    }
    wSafariSteps--;
    if (wSafariSteps == 0) safari_zone_game_over();
}

void SafariZoneScripts_Tick(void) {
    safari_zone_game_over_check();
    safari_zone_handle_after_text();
    if (YesNo_IsOpen()) {
        YesNo_Tick();
    }
    safari_zone_handle_prompt();

    if (wCurMap == MAP_SAFARI_ZONE_GATE) {
        safari_zone_gate_tick_script();
    }
}

void SafariZoneScripts_PostRender(void) {
    if (s_prompt_state == PROMPT_JOIN) {
        safari_draw_money_box();
        NPC_HideOverUITiles();
        Player_HideIfOverUI();
    }
    if (YesNo_IsOpen()) YesNo_PostRender();
}

void SafariZoneScripts_DebugGetState(uint16_t *steps, uint8_t *balls, uint8_t *script_state) {
    if (steps) *steps = wSafariSteps;
    if (balls) *balls = wNumSafariBalls;
    if (script_state) *script_state = wSafariZoneGateCurScript;
}

void SafariZoneScripts_DebugSetState(uint16_t steps) {
    wSafariSteps = steps;
}

void SafariZoneScripts_Enter(void) {
    wNumSafariBalls = 30;
    wSafariSteps = 502;
    SetEvent(EVENT_IN_SAFARI_ZONE);
    ClearEvent(EVENT_SAFARI_GAME_OVER);
    wSafariZoneGameOver = 0;
}

void SafariZoneScripts_Leave(void) {
    ClearEvent(EVENT_SAFARI_GAME_OVER);
    ClearEvent(EVENT_IN_SAFARI_ZONE);
    wSafariZoneGameOver = 0;
    wNumSafariBalls = 0;
}
