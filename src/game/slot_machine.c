
#include "slot_machine.h"
#include "rom_text.h"
#include "text.h"
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "inventory.h"
#include "yesno.h"
#include "trainer_sight.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include "../data/slots_gfx.h"
#include <string.h>
#include <stdio.h>

extern uint8_t BattleRandom(void);

#define SM_TMIDX(row, col) (((row) + 2) * SCROLL_MAP_W + ((col) + 2) + Map_UiColOfs())

#define SM_CREDIT_COL   5
#define SM_CREDIT_ROW   1
#define SM_PAYOUT_COL  11
#define SM_PAYOUT_ROW   1

#define SM_BET_BOX_ROW 11
#define SM_BET_BOX_COL 14
#define SM_BET_BOX_H    7
#define SM_BET_BOX_W    6
#define SM_BET_CUR_COL 15
#define SM_BET_TXT_COL 16
#define SM_BET_ROW0    12
#define SM_BET_ROW_STEP 2

#define SM_YN_ROW      12
#define SM_YN_COL      14
#define SM_YN_H         5
#define SM_YN_W         6
#define SM_YN_CUR_COL  15
#define SM_YN_TXT_COL  16
#define SM_YN_ROW0     13

#define SM_YN_ROW_STEP  2

#define SM_BALL_LCOL    3
#define SM_BALL_RCOL   16
#define SM_BALL_LIT   0x14
#define SM_BALL_UNLIT 0x23

#define SM_OAM_BASE     0
#define SM_ROWS_PER_REEL 6

#define SM_FLAG_CAN_WIN        0x40
#define SM_FLAG_CAN_WIN_7_BAR  0x80

#define SM_CH_SPACE  0x7F
#define SM_CH_TL     0x79
#define SM_CH_H      0x7A
#define SM_CH_TR     0x7B
#define SM_CH_V      0x7C
#define SM_CH_BL     0x7D
#define SM_CH_BR     0x7E
#define SM_CH_CURSOR 0xED
#define SM_CH_DOWN   0xEE
#define SM_CH_TIMES  0xF1

#define SM_BGP_NORMAL  0xE4
#define SM_OBP0_NORMAL 0xE4
#define SM_OBP1_NORMAL 0xE0

typedef enum {
    SM_IDLE = 0,
    SM_ASK,
    SM_BUBBLE,
    SM_FADE_OUT,
    SM_FADE_IN,
    SM_BET_PROMPT,
    SM_BET_MENU,
    SM_NOT_ENOUGH,
    SM_START_TEXT,
    SM_SPIN1,
    SM_SPIN2,
    SM_CHECK,
    SM_REROLL,
    SM_NO_MATCH,
    SM_WIN_YEAH,
    SM_WIN_FLASH,
    SM_WIN_TEXT,
    SM_WIN_WAIT,
    SM_PAYOUT,
    SM_PAYOUT_DONE,
    SM_OUT_OF_COINS,
    SM_ONE_MORE_TEXT,
    SM_ONE_MORE_MENU,
    SM_EXIT,
    SM_EXIT_FADE_OUT,
    SM_EXIT_FADE_IN
} sm_state_t;

static sm_state_t s_state = SM_IDLE;
static int  s_timer;
static int  s_screen_up;

static uint8_t s_flags;
static uint8_t s_allow_matches_counter;
static uint8_t s_lucky_index;

static uint8_t s_seven_bar_chance;
static uint8_t s_offset[3];
static uint8_t s_tiles[3][3];
static uint8_t s_slip[2];
static uint8_t s_reroll_counter;
static uint8_t s_stopping;
static uint8_t s_bet;
static uint16_t s_payout;
static uint8_t s_winning_symbol;
static int  s_spin_steps;
static int  s_flash_left;
static int  s_anim_counter;
static int  s_bet_cursor;
static int  s_yn_cursor;
static int  s_a_latched;
static int  s_reroll_half;
static char s_win_msg[48];

static uint8_t s_bgp, s_obp0;

static int sm_coins_get(void) {
    return ((wPlayerCoins[0] >> 4) & 0xF) * 1000 + (wPlayerCoins[0] & 0xF) * 100
         + ((wPlayerCoins[1] >> 4) & 0xF) * 10  + (wPlayerCoins[1] & 0xF);
}

static void sm_coins_set(int v) {

    v %= 10000;
    if (v < 0) v += 10000;
    wPlayerCoins[0] = (uint8_t)(((v / 1000) % 10) << 4 | ((v / 100) % 10));
    wPlayerCoins[1] = (uint8_t)(((v / 10)   % 10) << 4 | (v % 10));
}

static void sm_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[SM_TMIDX(row, col)] = tile;
}

static void sm_set_raw(int col, int row, uint8_t tile) { sm_set(col, row, tile); }

static uint8_t sm_ct(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    if (c == '!')             return (uint8_t)Font_CharToTile(0xE7);
    if (c == '?')             return (uint8_t)Font_CharToTile(0xE6);
    return (uint8_t)Font_CharToTile(SM_CH_SPACE);
}

static void sm_label(int col, int row, const char *s) {
    while (*s) sm_set(col++, row, sm_ct(*s++));
}

static void sm_box(int col, int row, int w, int h) {
    int r, c;
    for (r = 1; r < h - 1; r++) {
        for (c = 1; c < w - 1; c++) sm_set(col + c, row + r, (uint8_t)Font_CharToTile(SM_CH_SPACE));
        sm_set(col,         row + r, (uint8_t)Font_CharToTile(SM_CH_V));
        sm_set(col + w - 1, row + r, (uint8_t)Font_CharToTile(SM_CH_V));
    }
    sm_set(col,         row,         (uint8_t)Font_CharToTile(SM_CH_TL));
    sm_set(col + w - 1, row,         (uint8_t)Font_CharToTile(SM_CH_TR));
    sm_set(col,         row + h - 1, (uint8_t)Font_CharToTile(SM_CH_BL));
    sm_set(col + w - 1, row + h - 1, (uint8_t)Font_CharToTile(SM_CH_BR));
    for (c = 1; c < w - 1; c++) {
        sm_set(col + c, row,         (uint8_t)Font_CharToTile(SM_CH_H));
        sm_set(col + c, row + h - 1, (uint8_t)Font_CharToTile(SM_CH_H));
    }
}

static void sm_print_credit(void) {
    char buf[8];
    int i;
    snprintf(buf, sizeof(buf), "%04d", sm_coins_get());
    for (i = 0; i < 4; i++) sm_set(SM_CREDIT_COL + i, SM_CREDIT_ROW, sm_ct(buf[i]));
}

static void sm_print_payout(void) {
    char buf[8];
    int i;
    snprintf(buf, sizeof(buf), "%04d", (int)s_payout);
    for (i = 0; i < 4; i++) sm_set(SM_PAYOUT_COL + i, SM_PAYOUT_ROW, sm_ct(buf[i]));
}

static void sm_ball_row(int row, uint8_t t) {
    sm_set_raw(SM_BALL_LCOL, row,     t);
    sm_set_raw(SM_BALL_RCOL, row,     t);
    sm_set_raw(SM_BALL_LCOL, row + 1, (uint8_t)(t + 1));
    sm_set_raw(SM_BALL_RCOL, row + 1, (uint8_t)(t + 1));
}

static void sm_update_balls(uint8_t tile, int bet) {
    if (bet >= 3) { sm_ball_row(2, tile); sm_ball_row(10, tile); }
    if (bet >= 2) { sm_ball_row(4, tile); sm_ball_row(8,  tile); }
    sm_ball_row(6, tile);
}

static void sm_anim_wheel(int w) {
    const uint8_t *reel = kSlotsReel[w];
    int oam = SM_OAM_BASE + w * 12;
    uint8_t base_x = (uint8_t)(0x30 + w * 0x20);
    uint8_t y = 0x58;
    uint8_t off = s_offset[w];
    int i;

    for (i = 0; i < SM_ROWS_PER_REEL; i++) {
        uint8_t v = reel[off + i];
        wShadowOAM[oam + i * 2 + 0].y     = y;
        wShadowOAM[oam + i * 2 + 0].x     = base_x;
        wShadowOAM[oam + i * 2 + 0].tile  = v;
        wShadowOAM[oam + i * 2 + 0].flags = OAM_FLAG_PRIORITY;
        wShadowOAM[oam + i * 2 + 1].y     = y;
        wShadowOAM[oam + i * 2 + 1].x     = (uint8_t)(base_x + 8);
        wShadowOAM[oam + i * 2 + 1].tile  = (uint8_t)(v + 1);
        wShadowOAM[oam + i * 2 + 1].flags = OAM_FLAG_PRIORITY;
        y = (uint8_t)(y - 8);
    }

    off++;
    if (off == SLOTS_REEL_WRAP) off = 0;
    s_offset[w] = off;
}

static void sm_get_wheel_tiles(int w) {
    const uint8_t *reel = kSlotsReel[w];
    uint8_t off = s_offset[w];
    s_tiles[w][0] = reel[off];
    s_tiles[w][1] = reel[off + 2];
    s_tiles[w][2] = reel[off + 4];
}

static void sm_get_all_wheel_tiles(void) {
    sm_get_wheel_tiles(2);
    sm_get_wheel_tiles(1);
    sm_get_wheel_tiles(0);
}

static void sm_set_flags(void) {
    uint8_t b;
    if (s_flags & SM_FLAG_CAN_WIN_7_BAR) return;
    if (s_allow_matches_counter != 0) { s_flags |= SM_FLAG_CAN_WIN; return; }
    b = BattleRandom();
    if (b == 0) { s_allow_matches_counter = 60; return; }
    if (s_seven_bar_chance < b) { s_flags |= SM_FLAG_CAN_WIN_7_BAR; return; }
    if (210 < b)                { s_flags |= SM_FLAG_CAN_WIN;       return; }
    s_flags = 0;
}

static int sm_find_w1_w2_matches(int *de_idx) {
    const uint8_t *w1 = s_tiles[0], *w2 = s_tiles[1];
    if (w2[0] == w1[0]) { *de_idx = 0; return 1; }
    if (w2[1] == w1[0]) { *de_idx = 1; return 1; }
    if (w2[1] == w1[1]) { *de_idx = 1; return 1; }
    if (w2[1] == w1[2]) { *de_idx = 1; return 1; }
    if (w2[2] == w1[2]) { *de_idx = 2; return 1; }
    *de_idx = 0;
    return 0;
}

static int sm_stop_wheel1_early(void) {
    sm_get_wheel_tiles(0);
    if (s_flags & SM_FLAG_CAN_WIN_7_BAR)
        return 1;
    if (s_tiles[0][1] == SLOTS_TILE_CHERRY)
        return 1;
    s_slip[0] = 0;
    return 0;
}

static int sm_stop_wheel2_early(void) {
    int de_idx = 0;
    sm_get_wheel_tiles(1);
    if (s_flags & SM_FLAG_CAN_WIN_7_BAR) {
        (void)sm_find_w1_w2_matches(&de_idx);
        if (s_tiles[1][de_idx] >= SLOTS_TILE_BAR + 1) return 0;
        s_slip[1] = 0;
        return 1;
    }
    if (!sm_find_w1_w2_matches(&de_idx)) return 0;
    s_slip[1] = 0;
    return 1;
}

static int sm_stop_or_anim(int w) {
    if (s_stopping < (uint8_t)(w + 1)) { sm_anim_wheel(w); return 0; }
    if ((s_offset[w] & 1) == 0)        { sm_anim_wheel(w); return 0; }

    if (w == 2) return 1;

    if (s_slip[w] == 0) return 0;
    s_slip[w]--;
    if (w == 0) { if (!sm_stop_wheel1_early()) return 0; }
    else        { if ( sm_stop_wheel2_early()) return 0; }
    sm_anim_wheel(w);
    return 0;
}

static void sm_handle_spin_input(void) {
    if (!s_a_latched) return;
    s_a_latched = 0;
    if (s_stopping == 1 || s_stopping == 2) {

        if (s_slip[s_stopping - 1] != 0) return;
    }
    s_stopping++;
    Audio_PlaySFX_SlotsStopWheel();
}

static int sm_reward_for(uint8_t winning_symbol, int *flash) {
    switch (winning_symbol) {
        case 0x00:
            *flash = 0x14;
            return 300;
        case 0x04:
            *flash = 0x08;
            return 100;
        case 0x08:
            *flash = 0x02;
            return 8;
        default:
            *flash = 0x04;
            return 15;
    }
}

static const char *sm_symbol_name(uint8_t winning_symbol) {
    switch (winning_symbol) {
        case 0x00: return "7";
        case 0x04: return "BAR";
        case 0x08: return "CHERRY";
        case 0x0C: return "FISH";
        case 0x10: return "BIRD";
        default:   return "MOUSE";
    }
}

static void sm_print_winning_symbol(void) {
    uint8_t base = (uint8_t)(SLOTS_SYMBOL_BG_BASE + s_winning_symbol);
    sm_set_raw(2, 14, base);
    sm_set_raw(3, 14, (uint8_t)(base + 1));
    sm_set_raw(2, 13, (uint8_t)(base + 2));
    sm_set_raw(3, 13, (uint8_t)(base + 3));
    sm_set(18, 16, (uint8_t)Font_CharToTile(SM_CH_DOWN));
}

static void sm_draw_screen(void) {
    int r, c;
    for (r = 0; r < SLOTS_MAP_H; r++)
        for (c = 0; c < SLOTS_MAP_W; c++)
            sm_set_raw(c, r, kSlotsTileMap[r][c]);

    for (r = SLOTS_MAP_H; r < SCREEN_HEIGHT; r++)
        for (c = 0; c < SCREEN_WIDTH; c++)
            sm_set(c, r, (uint8_t)Font_CharToTile(SM_CH_SPACE));
    sm_print_credit();
    sm_print_payout();
}

static void sm_load_screen(void) {
    int i;
    for (i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    hWY = 144;
    SlotsGfx_Load();
    s_offset[0] = s_offset[1] = s_offset[2] = 0x1C;
    sm_anim_wheel(0);
    sm_anim_wheel(1);
    sm_anim_wheel(2);
    sm_draw_screen();
    s_screen_up = 1;
}

static void sm_settle_text_to_bg(void) {
    Text_BlitBoxToBGAndHideWindow();
}

static void sm_restore_overworld(void) {
    s_screen_up = 0;
    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void sm_draw_bet_menu(void) {
    int i;
    sm_box(SM_BET_BOX_COL, SM_BET_BOX_ROW, SM_BET_BOX_W, SM_BET_BOX_H);
    for (i = 0; i < 3; i++) {
        int row = SM_BET_ROW0 + i * SM_BET_ROW_STEP;
        sm_set(SM_BET_CUR_COL, row,
               (uint8_t)Font_CharToTile(i == s_bet_cursor ? SM_CH_CURSOR : SM_CH_SPACE));
        sm_set(SM_BET_TXT_COL,     row, (uint8_t)Font_CharToTile(SM_CH_TIMES));
        sm_set(SM_BET_TXT_COL + 1, row, sm_ct((char)('3' - i)));
    }
}

static void sm_draw_yes_no(void) {
    sm_box(SM_YN_COL, SM_YN_ROW, SM_YN_W, SM_YN_H);
    sm_set(SM_YN_CUR_COL, SM_YN_ROW0,
           (uint8_t)Font_CharToTile(s_yn_cursor == 0 ? SM_CH_CURSOR : SM_CH_SPACE));
    sm_label(SM_YN_TXT_COL, SM_YN_ROW0, "YES");
    sm_set(SM_YN_CUR_COL, SM_YN_ROW0 + SM_YN_ROW_STEP,
           (uint8_t)Font_CharToTile(s_yn_cursor == 1 ? SM_CH_CURSOR : SM_CH_SPACE));
    sm_label(SM_YN_TXT_COL, SM_YN_ROW0 + SM_YN_ROW_STEP, "NO");
}

#define SM_FADE_STEPS        4
#define SM_FADE_STEP_FRAMES  8

static const uint8_t kSmFadeOutToWhite[SM_FADE_STEPS][3] = {
    {0xE4, 0xD0, 0xE0},
    {0x28, 0x04, 0x20},
    {0x0A, 0x01, 0x08},
    {0x00, 0x00, 0x00},
};

static const uint8_t kSmFadeInFromWhite[SM_FADE_STEPS][3] = {
    {0x00, 0x00, 0x00},
    {0x0A, 0x01, 0x08},
    {0x28, 0x04, 0x20},
    {0xE4, 0xD0, 0xE0},
};

static int s_fade_step;

static int sm_fade_advance(const uint8_t table[SM_FADE_STEPS][3]) {
    if (s_timer > 0) { s_timer--; return 0; }
    Display_SetPalette(table[s_fade_step][0], table[s_fade_step][1], table[s_fade_step][2]);
    s_timer = SM_FADE_STEP_FRAMES;
    return (++s_fade_step >= SM_FADE_STEPS);
}

static void sm_fade_begin(void) { s_fade_step = 0; s_timer = 0; }

void SlotMachine_SelectLucky(void) {
    uint8_t a = BattleRandom();
    if (a < 7) a = 8;
    s_lucky_index = (uint8_t)(a >> 3);
}

static int sm_able_to_play(void) {
    if (Inventory_GetQty(ITEM_COIN_CASE) <= 0) {
        Text_ShowASCII(RomText("_RequireCoinCaseText"));
        return 0;
    }
    if (sm_coins_get() == 0) {
        Text_ShowASCII(RomText("_GameCornerNoCoinsText"));
        return 0;
    }
    return 1;
}

void SlotMachine_Start(int machine_index, int kind) {
    switch (kind) {
        case SLOTS_MACHINE_OUTOFORDER:
            Text_ShowASCII(RomText("_GameCornerOutOfOrderText"));
            return;
        case SLOTS_MACHINE_OUTTOLUNCH:
            Text_ShowASCII(RomText("_GameCornerOutToLunchText"));
            return;
        case SLOTS_MACHINE_SOMEONESKEYS:
            Text_ShowASCII(RomText("_GameCornerSomeonesKeysText"));
            return;
        default:
            break;
    }
    if (!sm_able_to_play()) return;

    s_seven_bar_chance =
        (machine_index >= 0 && (uint8_t)(machine_index + 1) == s_lucky_index) ? 250 : 253;

    s_allow_matches_counter = 0;
    s_screen_up = 0;
    s_yn_cursor = 0;
    s_state = SM_ASK;
    YesNo_Show(RomText("_PlaySlotMachineText"));
}

int SlotMachine_IsOpen(void) { return s_state != SM_IDLE; }

void SlotMachine_Reset(void) {
    if (s_screen_up) {
        sm_restore_overworld();
        Display_LoadMapPalette();
    }
    Emote_Hide();
    s_state = SM_IDLE;
    s_screen_up = 0;
}

static void sm_begin_round(void) {
    s_payout = 0;
    sm_print_credit();
    sm_print_payout();
    s_bet_cursor = 0;
    s_state = SM_BET_PROMPT;
}

static void sm_begin_spin(void) {
    sm_coins_set(sm_coins_get() - s_bet);
    sm_print_credit();
    sm_update_balls(SM_BALL_LIT, s_bet);
    sm_set_flags();

    s_slip[0] = s_slip[1] = 4;
    s_reroll_counter = 4;
    Audio_PlaySFX_SlotsNewSpin();
    s_state = SM_START_TEXT;
}

static uint8_t sm_find_match(void) {
    const uint8_t *w1 = s_tiles[0], *w2 = s_tiles[1], *w3 = s_tiles[2];
    if (s_bet == 3) {
        if (w2[1] == w1[0] && w3[2] == w1[0]) return w1[0];
        if (w2[1] == w1[2] && w3[0] == w1[2]) return w1[2];
    }
    if (s_bet >= 2) {
        if (w2[2] == w1[2] && w3[2] == w1[2]) return w1[2];
        if (w2[0] == w1[0] && w3[0] == w1[0]) return w1[0];
    }
    if (w2[1] == w1[1] && w3[1] == w1[1]) return w1[1];
    return 0;
}

static void sm_start_reroll(void) {
    s_reroll_half = 2;
    s_timer = 0;
    s_state = SM_REROLL;
}

static void sm_accept_match(uint8_t tile) {
    int payout, flash;
    s_winning_symbol = (uint8_t)(tile - 2);
    payout = sm_reward_for(s_winning_symbol, &flash);
    s_flash_left = flash;

    if (payout == 8 || payout == 15) {
        if (s_allow_matches_counter != 0) s_allow_matches_counter--;
    } else if (payout == 100) {
        Audio_PlaySFX_GetKeyItem();
        s_flags = 0;
    }

    s_payout = (uint16_t)payout;

    if (payout == 300) {

        s_state = SM_WIN_YEAH;
        Text_ShowASCII("Yeah!");
        return;
    }
    s_timer = 5;
    s_state = SM_WIN_FLASH;
}

static void sm_check_matches(void) {
    uint8_t tile;
    sm_get_all_wheel_tiles();
    tile = sm_find_match();

    if (tile != 0) {
        if (!(s_flags & (SM_FLAG_CAN_WIN | SM_FLAG_CAN_WIN_7_BAR))) { sm_start_reroll(); return; }
        if (!(s_flags & SM_FLAG_CAN_WIN_7_BAR) && tile < SLOTS_TILE_BAR + 1) { sm_start_reroll(); return; }
        sm_accept_match(tile);
        return;
    }

    if (s_flags & (SM_FLAG_CAN_WIN | SM_FLAG_CAN_WIN_7_BAR)) {
        s_reroll_counter--;
        if (s_reroll_counter != 0) { sm_start_reroll(); return; }
    }
    s_state = SM_NO_MATCH;
    Text_ShowASCII(RomText("_NotThisTimeText"));
}

static void sm_end_round(void) {
    if (sm_coins_get() == 0) {
        s_timer = 60;
        s_state = SM_OUT_OF_COINS;
        wDoNotWaitForButtonPress = 1;
        Text_ShowASCII(RomText("_OutOfCoinsSlotMachineText"));
        return;
    }
    s_yn_cursor = 0;
    s_state = SM_ONE_MORE_TEXT;
    wDoNotWaitForButtonPress = 1;
    Text_ShowASCII(RomText("_OneMoreGoSlotMachineText"));
}

void SlotMachine_Tick(void) {
    if (s_state == SM_IDLE) return;

    if (hJoyPressed & PAD_A) s_a_latched = 1;

    switch (s_state) {

    case SM_ASK:

        if (YesNo_IsOpen()) {
            YesNo_Tick();
            YesNo_PostRender();
            return;
        }
        if (!YesNo_GetResult()) {
            s_state = SM_IDLE;
            Text_Close();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            return;
        }

        Emote_ShowOnPlayerKind(EMOTE_SMILE);
        s_timer = 60;
        s_state = SM_BUBBLE;
        return;

    case SM_BUBBLE:
        if (--s_timer > 0) return;
        Emote_Hide();
        Text_Close();
        sm_fade_begin();
        s_state = SM_FADE_OUT;
        return;

    case SM_FADE_OUT:

        if (!sm_fade_advance(kSmFadeOutToWhite)) return;
        sm_load_screen();
        sm_fade_begin();
        s_state = SM_FADE_IN;
        return;

    case SM_FADE_IN:
        if (!sm_fade_advance(kSmFadeInFromWhite)) return;

        Display_SetPalette(SM_BGP_NORMAL, SM_OBP0_NORMAL, SM_OBP1_NORMAL);
        s_bgp  = SM_BGP_NORMAL;
        s_obp0 = SM_OBP0_NORMAL;
        sm_begin_round();
        return;

    case SM_BET_PROMPT:
        wDoNotWaitForButtonPress = 1;
        Text_ShowASCII(RomText("_BetHowManySlotMachineText"));
        s_state = SM_BET_MENU;
        return;

    case SM_BET_MENU:
        if (Text_IsOpen()) return;
        sm_settle_text_to_bg();
        if (hJoyPressed & PAD_UP)   s_bet_cursor = (s_bet_cursor + 2) % 3;
        if (hJoyPressed & PAD_DOWN) s_bet_cursor = (s_bet_cursor + 1) % 3;
        sm_draw_bet_menu();
        if (hJoyPressed & PAD_B) {
            s_a_latched = 0;
            Audio_PlaySFX_PressAB();
            s_state = SM_EXIT;
            return;
        }
        if (hJoyPressed & PAD_A) {
            s_a_latched = 0;
            Audio_PlaySFX_PressAB();
            s_bet = (uint8_t)(3 - s_bet_cursor);
            if (sm_coins_get() < s_bet) {
                s_state = SM_NOT_ENOUGH;
                Text_ShowASCII(RomText("_NotEnoughCoinsSlotMachineText"));
                return;
            }
            Text_Close();
            sm_draw_screen();
            sm_begin_spin();
        }
        return;

    case SM_NOT_ENOUGH:
        if (Text_IsOpen()) return;
        s_state = SM_BET_PROMPT;
        return;

    case SM_START_TEXT:
        wDoNotWaitForButtonPress = 1;
        Text_ShowASCII("Start!");
        s_spin_steps = 20;
        s_timer = 0;
        s_stopping = 0;
        s_a_latched = 0;
        s_state = SM_SPIN1;
        return;

    case SM_SPIN1:
        if (s_timer > 0) { s_timer--; return; }
        sm_anim_wheel(0);
        sm_anim_wheel(1);
        sm_anim_wheel(2);
        s_timer = 2;
        if (--s_spin_steps <= 0) {
            s_stopping = 0;
            s_state = SM_SPIN2;
        }
        return;

    case SM_SPIN2:
        if (s_timer > 0) { s_timer--; return; }
        sm_handle_spin_input();
        sm_stop_or_anim(0);
        sm_stop_or_anim(1);
        if (sm_stop_or_anim(2)) { s_state = SM_CHECK; return; }
        s_timer = 3;
        return;

    case SM_CHECK:
        sm_check_matches();
        return;

    case SM_REROLL:
        if (s_timer > 0) { s_timer--; return; }
        sm_anim_wheel(2);
        if (--s_reroll_half <= 0) { s_state = SM_CHECK; return; }
        s_timer = 1;
        return;

    case SM_NO_MATCH:
        if (Text_IsOpen()) return;
        sm_end_round();
        return;

    case SM_WIN_YEAH:
        if (Text_IsOpen()) return;

        Audio_PlaySFX_GetItem2();
        if (BattleRandom() >= 0x80) s_flags = 0;
        s_allow_matches_counter = 0;
        s_timer = 5;
        s_state = SM_WIN_FLASH;
        return;

    case SM_WIN_FLASH:
        if (s_timer > 0) { s_timer--; return; }
        s_bgp ^= 0x40; Display_SetBGP(s_bgp);
        s_timer = 5;
        if (--s_flash_left <= 0) {
            sm_print_payout();
            snprintf(s_win_msg, sizeof(s_win_msg), "    %s lined up!\nScored %d coins!",
                     sm_symbol_name(s_winning_symbol), (int)s_payout);
            wDoNotWaitForButtonPress = 1;
            Text_ShowASCII(s_win_msg);
            s_state = SM_WIN_TEXT;
        }
        return;

    case SM_WIN_TEXT:
        if (Text_IsOpen()) return;
        sm_settle_text_to_bg();
        sm_print_winning_symbol();
        s_a_latched = 0;
        s_state = SM_WIN_WAIT;
        return;

    case SM_WIN_WAIT:
        if (hJoyPressed & (PAD_A | PAD_B)) {
            s_a_latched = 0;
            Audio_PlaySFX_PressAB();
            Text_Close();
            sm_draw_screen();
            sm_update_balls(SM_BALL_LIT, s_bet);
            sm_print_payout();
            s_anim_counter = 5;
            s_timer = 0;
            s_state = SM_PAYOUT;
        }
        return;

    case SM_PAYOUT:
        if (s_timer > 0) { s_timer--; return; }
        if (s_payout == 0) { s_state = SM_PAYOUT_DONE; return; }
        s_payout--;
        sm_coins_set(sm_coins_get() + 1);
        sm_print_credit();
        sm_print_payout();
        Audio_PlaySFX_SlotsReward();
        if (--s_anim_counter == 0) {
            s_obp0 ^= 0x40; Display_SetOBP0(s_obp0);
            s_anim_counter = 5;
        }

        s_timer = (s_winning_symbol < 0x07) ? 4 : 8;
        return;

    case SM_PAYOUT_DONE:
        Display_SetOBP0(SM_OBP0_NORMAL);
        sm_end_round();
        return;

    case SM_OUT_OF_COINS:
        if (Text_IsOpen()) return;
        if (--s_timer > 0) return;
        s_state = SM_EXIT;
        return;

    case SM_ONE_MORE_TEXT:
        if (Text_IsOpen()) return;
        sm_settle_text_to_bg();
        sm_draw_yes_no();
        s_a_latched = 0;
        s_state = SM_ONE_MORE_MENU;
        return;

    case SM_ONE_MORE_MENU:
        if (hJoyPressed & PAD_UP)   s_yn_cursor = 0;
        if (hJoyPressed & PAD_DOWN) s_yn_cursor = 1;
        sm_draw_yes_no();
        if (hJoyPressed & (PAD_A | PAD_B)) {
            int yes = (hJoyPressed & PAD_B) ? 0 : (s_yn_cursor == 0);
            s_a_latched = 0;
            Audio_PlaySFX_PressAB();
            Text_Close();
            if (!yes) { s_state = SM_EXIT; return; }
            sm_draw_screen();
            sm_update_balls(SM_BALL_UNLIT, 3);
            sm_begin_round();
        }
        return;

    case SM_EXIT:
        Text_Close();
        sm_fade_begin();
        s_state = SM_EXIT_FADE_OUT;
        return;

    case SM_EXIT_FADE_OUT:

        if (!sm_fade_advance(kSmFadeOutToWhite)) return;
        sm_restore_overworld();
        sm_fade_begin();
        s_state = SM_EXIT_FADE_IN;
        return;

    case SM_EXIT_FADE_IN:
        if (!sm_fade_advance(kSmFadeInFromWhite)) return;

        Display_LoadMapPalette();
        s_state = SM_IDLE;
        return;

    default:
        s_state = SM_IDLE;
        return;
    }
}
