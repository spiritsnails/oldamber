
#include <string.h>
#include <stdio.h>
#include "trade.h"
#include "overworld.h"
#include "player.h"
#include "text.h"
#include "pokemon.h"
#include "battle/move_anim.h"
#include "battle/battle_ui.h"
#include "yesno.h"
#include "party_menu.h"
#include "gbc_color.h"

#include "rom_text.h"
#include "pokedex.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include "../data/base_stats.h"
#include "../data/trade_gfx.h"
#include "../data/party_icon_data.h"
#include "../data/pokemon_sprites.h"
#include "gen2_species.h"
#include "mon_pic.h"

extern int Game_GetScene(void);
extern int Game_BeginTradeAnim(void);
extern int Game_BeginFieldEvolution(void);

trade_anim_data_t gTradeAnim;

#define TC_W 32
#define TC_H 18
static uint8_t s_canvas[TC_H][TC_W];

#define TRADE_BALL_DROP_ANIM   170u
#define TRADE_BALL_SHAKE_ANIM  171u
#define TRADE_BALL_TILT_ANIM   172u
#define TRADE_BALL_POOF_ANIM   173u

#define TRADE_PIC_COL 7
#define TRADE_PIC_ROW 2

#define TRADE_PIC_TILE_BASE 0

#define CABLE_END   0x5D
#define CABLE_HORIZ 0x5E
#define CABLE_CORNER_TOP 0x5F
#define CABLE_CORNER_BOT 0x60
#define CABLE_VERT  0x61

#define CABLE_BALL_TILE 0x7E

#define BLANK Font_CharToTile(0x7F)

typedef enum {
    TA_IDLE = 0,
    TA_LOAD_GFX,
    TA_SHOW_PLAYER_MON,
    TA_OPEN_CABLE_1,
    TA_BALL_ENTER_CABLE,
    TA_ANIM_L2R,
    TA_DELAY100_1,
    TA_CLEARWIN_1,
    TA_TEXT_WENT_TO,
    TA_TEXT_FOR_SENDS,
    TA_TEXT_FAREWELL,
    TA_ANIM_R2L,
    TA_CLEARWIN_2,
    TA_OPEN_CABLE_2,
    TA_SHOW_ENEMY_MON,
    TA_DELAY100_2,
    TA_SLIDE_TEXT,
    TA_CLEANUP,
    TA_DONE
} trade_state_t;

static trade_state_t s_state = TA_IDLE;
static int  s_phase;
static int  s_timer;
static int  s_i;
static uint8_t s_anim_id;
static move_anim_ctx_t s_anim;

static int  s_moving_right;
static int  s_base_x, s_base_y;
static uint8_t s_icon_species;
static int  s_bulge;
static trade_state_t s_slide_next;

static char s_player_name[NAME_LENGTH];
static char s_enemy_name[NAME_LENGTH];
static char s_msg[128];

static void tc_clear(void) {
    memset(s_canvas, (uint8_t)BLANK, sizeof s_canvas);
}

static void tc_set(int col, int row, uint8_t tile) {
    if (col >= 0 && col < TC_W && row >= 0 && row < TC_H) s_canvas[row][col] = tile;
}

static void tc_clear_area(int col, int row, int w, int h) {
    for (int r = row; r < row + h; r++)
        for (int c = col; c < col + w; c++) tc_set(c, r, (uint8_t)BLANK);
}

static int tchar(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    if (c == ' ') return Font_CharToTile(0x7F);
    if (c == '.') return Font_CharToTile(0xE8);
    if (c == '!') return Font_CharToTile(0xE7);
    if (c == '/') return Font_CharToTile(0xF3);
    return Font_CharToTile(c);
}

static void tc_str(int col, int row, const char *s) {
    for (; *s && col < TC_W; s++, col++) tc_set(col, row, (uint8_t)tchar((unsigned char)*s));
}

static void wset(int col, int row, uint8_t tile);

static void name_str(int col, int row, const char *s, int window) {
    for (int i = 0; i < NAME_LENGTH && s[i] && (uint8_t)s[i] != 0x50; i++) {
        uint8_t t = (uint8_t)tchar((unsigned char)s[i]);
        if (window) wset(col + i, row, t); else tc_set(col + i, row, t);
    }
}

#define GLYPH_BAR  Font_CharToTile(0x7A)
#define GLYPH_DOT  Font_CharToTile(0xF2)

#define GLYPH_ID   TRADE_GLYPH_ID_TILE
#define GLYPH_NO   TRADE_GLYPH_NO_TILE

static void tc_num_lz(int col, int row, unsigned value, int digits) {
    char buf[8];
    snprintf(buf, sizeof buf, "%0*u", digits, value);
    tc_str(col, row, buf);
}

static void tc_textbox(int col, int row, int b, int c) {
    int R = col + c + 1, B = row + b + 1;
    tc_set(col, row, (uint8_t)Font_CharToTile(0x79));
    tc_set(R,   row, (uint8_t)Font_CharToTile(0x7B));
    tc_set(col, B,   (uint8_t)Font_CharToTile(0x7D));
    tc_set(R,   B,   (uint8_t)Font_CharToTile(0x7E));
    for (int x = col + 1; x < R; x++) {
        tc_set(x, row, (uint8_t)Font_CharToTile(0x7A));
        tc_set(x, B,   (uint8_t)Font_CharToTile(0x7A));
    }
    for (int y = row + 1; y < B; y++) {
        tc_set(col, y, (uint8_t)Font_CharToTile(0x7C));
        tc_set(R,   y, (uint8_t)Font_CharToTile(0x7C));
        for (int x = col + 1; x < R; x++) tc_set(x, y, (uint8_t)BLANK);
    }
}

static void tc_blit_map(int col, int row, const uint8_t *map, int w, int h) {
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) tc_set(col + c, row + r, map[r * w + c]);
}

static void trade_load_party_sprite_gfx(void) {
    PartyIcons_LoadTiles();
    Display_SetOBP1(0xD0);
}

static uint8_t trade_reverse_bits(uint8_t b) {
    b = (uint8_t)(((b & 0xF0u) >> 4) | ((b & 0x0Fu) << 4));
    b = (uint8_t)(((b & 0xCCu) >> 2) | ((b & 0x33u) << 2));
    b = (uint8_t)(((b & 0xAAu) >> 1) | ((b & 0x55u) << 1));
    return b;
}

static uint8_t trade_obp0(void) {

    if (GbcColor_BattleAutoColor())
        return 0xD0;

    if (GbcColor_MonPalStyleGet() == GBC_MONPAL_SGB)
        return 0xF0;
    return 0xE4;
}

static void tc_load_mon_pic(uint8_t species) {
    uint8_t dex = Species_Dex(species);

    GbcColor_SetPalTradeMon(dex);
    if (!MonPic_Exists(dex)) return;
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int src = ty * 7 + (6 - tx);
            uint8_t tile[16];
            const uint8_t *st = MonPic_FrontTile(dex, src);
            for (int row = 0; row < 8; row++) {
                tile[row * 2 + 0] = trade_reverse_bits(st[row * 2 + 0]);
                tile[row * 2 + 1] = trade_reverse_bits(st[row * 2 + 1]);
            }
            uint8_t tid = (uint8_t)(TRADE_PIC_TILE_BASE + ty * 7 + tx);
            Display_LoadTile(tid, tile);
            tc_set(TRADE_PIC_COL + tx, TRADE_PIC_ROW + ty, tid);
        }
    }
}

static void trade_hook_clear_mon_pic(void) {
    tc_clear_area(TRADE_PIC_COL, TRADE_PIC_ROW, 7, 7);
}
static void trade_hook_clear_screen(void) {
    tc_clear();
}
static const move_anim_trade_hooks_t kTradeHooks = {
    trade_hook_clear_mon_pic,
    trade_hook_clear_screen,
};

static void trade_present(void) {
    int tile_off = (hSCX >> 3) & (TC_W - 1);
    int frac     = hSCX & 7;
    for (int r = 0; r < SCROLL_MAP_H; r++) {
        int cy = r - 2;
        for (int b = 0; b < SCROLL_MAP_W; b++) {
            int cx = (tile_off + b - 2) & (TC_W - 1);
            uint8_t t = (cy >= 0 && cy < TC_H) ? s_canvas[cy][cx] : (uint8_t)BLANK;
            gScrollTileMap[r * SCROLL_MAP_W + b] = t;
        }
    }
    gScrollPxX = -frac;
    gScrollPxY = 0;
}

static void trade_clear_sprites(void) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        wShadowOAM[i].y = 0; wShadowOAM[i].x = 0;
        wShadowOAM[i].tile = 0; wShadowOAM[i].flags = 0;
    }
}

static void trade_window_off(void) {
    hWY = SCREEN_HEIGHT_PX;
    hWX = 7;
    memset(gWindowTileMap, 0, sizeof gWindowTileMap);
}

static void card_draw_bg(int row0, uint8_t species, const char *nick,
                         const char *ot, uint16_t otid) {
    tc_textbox(4, row0, 6, 10);

    tc_set(5, row0 + 0, (uint8_t)GLYPH_BAR);
    tc_set(6, row0 + 0, (uint8_t)GLYPH_BAR);
    tc_set(7, row0 + 0, (uint8_t)GLYPH_NO);
    tc_set(8, row0 + 0, (uint8_t)GLYPH_DOT);
    tc_num_lz(9, row0 + 0, gSpeciesToDex[species], 3);
    tc_str(5, row0 + 2, nick);
    tc_str(5, row0 + 4, "OT/");
    name_str(8, row0 + 4, ot, 0);

    tc_set(5, row0 + 6, (uint8_t)GLYPH_ID);
    tc_set(6, row0 + 6, (uint8_t)GLYPH_NO);
    tc_set(7, row0 + 6, (uint8_t)GLYPH_DOT);
    tc_num_lz(8, row0 + 6, otid, 5);
}

static void wset(int col, int row, uint8_t tile) {
    if (col >= 0 && col < SCREEN_WIDTH && row >= 0 && row < SCREEN_HEIGHT)
        gWindowTileMap[row][col] = tile;
}
static void wstr(int col, int row, const char *s) {
    for (; *s && col < SCREEN_WIDTH; s++, col++) wset(col, row, (uint8_t)tchar((unsigned char)*s));
}
static void wnum_lz(int col, int row, unsigned v, int digits) {
    char buf[8];
    snprintf(buf, sizeof buf, "%0*u", digits, v);
    wstr(col, row, buf);
}
static void wtextbox(int col, int row, int b, int c) {
    int R = col + c + 1, B = row + b + 1;
    wset(col, row, (uint8_t)Font_CharToTile(0x79));
    wset(R,   row, (uint8_t)Font_CharToTile(0x7B));
    wset(col, B,   (uint8_t)Font_CharToTile(0x7D));
    wset(R,   B,   (uint8_t)Font_CharToTile(0x7E));
    for (int x = col + 1; x < R; x++) {
        wset(x, row, (uint8_t)Font_CharToTile(0x7A));
        wset(x, B,   (uint8_t)Font_CharToTile(0x7A));
    }
    for (int y = row + 1; y < B; y++) {
        wset(col, y, (uint8_t)Font_CharToTile(0x7C));
        wset(R,   y, (uint8_t)Font_CharToTile(0x7C));
        for (int x = col + 1; x < R; x++) wset(x, y, (uint8_t)BLANK);
    }
}

#define TRADE_CARD_WIN_ROW 10

static void card_draw_window(uint8_t species, const char *nick,
                             const char *ot, uint16_t otid) {
    const int r = TRADE_CARD_WIN_ROW;
    memset(gWindowTileMap, 0, sizeof gWindowTileMap);
    wtextbox(4, r, 6, 10);
    wset(5, r + 0, (uint8_t)GLYPH_BAR);
    wset(6, r + 0, (uint8_t)GLYPH_BAR);
    wset(7, r + 0, (uint8_t)GLYPH_NO);
    wset(8, r + 0, (uint8_t)GLYPH_DOT);
    wnum_lz(9, r + 0, gSpeciesToDex[species], 3);
    wstr(5, r + 2, nick);
    wstr(5, r + 4, "OT/");
    name_str(8, r + 4, ot, 1);
    wset(5, r + 6, (uint8_t)GLYPH_ID);
    wset(6, r + 6, (uint8_t)GLYPH_NO);
    wset(7, r + 6, (uint8_t)GLYPH_DOT);
    wnum_lz(8, r + 6, otid, 5);
}

static void cable_offscreen(void) {
    for (int c = SCREEN_WIDTH; c < TC_W; c++) tc_set(c, 4, CABLE_HORIZ);
}

static void draw_left_gameboy(void) {
    tc_clear();
    tc_set(11, 4, CABLE_END);
    for (int i = 0; i < 8; i++) tc_set(12 + i, 4, CABLE_HORIZ);
    tc_blit_map(5, 3, (const uint8_t *)gTradeGameBoyMap,
                TRADE_GAME_BOY_MAP_W, TRADE_GAME_BOY_MAP_H);
    tc_textbox(4, 12, 2, 7);
    name_str(5, 14, (const char *)wPlayerName, 0);
    cable_offscreen();
}

static void draw_right_gameboy(void) {
    tc_clear();
    for (int i = 0; i < 14; i++) tc_set(i, 4, CABLE_HORIZ);

    tc_set(14, 4, CABLE_CORNER_TOP);
    for (int r = 5; r <= 8; r++) tc_set(14, r, CABLE_VERT);
    tc_set(14, 9, CABLE_CORNER_BOT);
    tc_set(13, 9, CABLE_END);
    tc_blit_map(7, 8, (const uint8_t *)gTradeGameBoyMap,
                TRADE_GAME_BOY_MAP_W, TRADE_GAME_BOY_MAP_H);
    tc_textbox(6, 0, 2, 7);
    name_str(7, 2, gTradeAnim.enemy_trainer, 0);
    cable_offscreen();
}

static void draw_cable_ball(void) {
    uint8_t tile = (uint8_t)(CABLE_BALL_TILE + (s_bulge & 1));
    trade_clear_sprites();
    for (int i = 0; i < 4; i++) {
        wShadowOAM[i].y     = (uint8_t)(0x20 + ((i & 2) ? 8 : 0));
        wShadowOAM[i].x     = (uint8_t)(s_i  + ((i & 1) ? 8 : 0));
        wShadowOAM[i].tile  = tile;
        wShadowOAM[i].flags = (uint8_t)(((i & 1) ? OAM_FLAG_FLIP_X : 0) |
                                        ((i & 2) ? OAM_FLAG_FLIP_Y : 0));
    }
    s_bulge ^= 1;
}

static void draw_cable_across(void) {
    tc_clear();
    for (int c = 0; c < TC_W; c++) tc_set(c, 4, CABLE_HORIZ);
}

static void write_circled_mon_oam(void) {
    trade_clear_sprites();

    uint8_t dex  = gSpeciesToDex[s_icon_species];
    uint8_t icon = (dex >= 1 && dex <= 151) ? gMonPartyIconType[dex] : ICON_MON;
    uint8_t base = (uint8_t)(icon << 2);

    if (icon == ICON_HELIX) {
        wShadowOAM[0] = (oam_entry_t){ 16, 16, (uint8_t)(base + 0), 0 };
        wShadowOAM[1] = (oam_entry_t){ 16, 24, (uint8_t)(base + 1), 0 };
        wShadowOAM[2] = (oam_entry_t){ 24, 16, (uint8_t)(base + 2), 0 };
        wShadowOAM[3] = (oam_entry_t){ 24, 24, (uint8_t)(base + 3), 0 };
    } else {
        wShadowOAM[0] = (oam_entry_t){ 16, 16, (uint8_t)(base + 0), 0 };
        wShadowOAM[1] = (oam_entry_t){ 16, 24, (uint8_t)(base + 0), OAM_FLAG_FLIP_X };
        wShadowOAM[2] = (oam_entry_t){ 24, 16, (uint8_t)(base + 2), 0 };
        wShadowOAM[3] = (oam_entry_t){ 24, 24, (uint8_t)(base + 2), OAM_FLAG_FLIP_X };
    }

    static const int8_t kQuad[4][2] = { {8,8}, {24,8}, {8,24}, {24,24} };
    static const uint8_t kFlip[4]   = {
        0,
        OAM_FLAG_FLIP_X,
        OAM_FLAG_FLIP_Y,
        OAM_FLAG_FLIP_X | OAM_FLAG_FLIP_Y
    };
    uint8_t bub = (uint8_t)(ICON_TRADEBUBBLE << 2);
    int o = 4;
    for (int q = 0; q < 4; q++) {
        for (int i = 0; i < 4; i++) {
            int dx = (i & 1) ? 8 : 0;
            int dy = (i & 2) ? 8 : 0;

            int t = i ^ ((kFlip[q] & OAM_FLAG_FLIP_X) ? 1 : 0)
                      ^ ((kFlip[q] & OAM_FLAG_FLIP_Y) ? 2 : 0);
            wShadowOAM[o].y     = (uint8_t)(kQuad[q][1] + dy);
            wShadowOAM[o].x     = (uint8_t)(kQuad[q][0] + dx);
            wShadowOAM[o].tile  = (uint8_t)(bub + t);
            wShadowOAM[o].flags = (uint8_t)(kFlip[q] | OAM_FLAG_PALETTE);
            o++;
        }
    }

    for (int i = 0; i < 20; i++) {
        wShadowOAM[i].y = (uint8_t)(wShadowOAM[i].y + s_base_y);
        wShadowOAM[i].x = (uint8_t)(wShadowOAM[i].x + s_base_x);
    }
}

static uint8_t s_bgp = 0xE4;
static void anim_circled_mon(void) {
    s_bgp ^= 0x3C;
    Display_SetBGP(s_bgp);
    for (int i = 0; i < 20; i++)
        wShadowOAM[i].tile ^= ICON_ICONOFFSET;
}

static void add_offsets_to_oam(int dx, int dy) {
    for (int i = 0; i < 20; i++) {
        wShadowOAM[i].y = (uint8_t)(wShadowOAM[i].y + dy);
        wShadowOAM[i].x = (uint8_t)(wShadowOAM[i].x + dx);
    }
}

static void start_anim(uint8_t id) {
    memset(&s_anim, 0, sizeof s_anim);
    s_anim.animation_id = id;
    s_anim.animation_type = 0;
    s_anim_id = id;
    MoveAnim_SetTradeHooks(&kTradeHooks);
    MoveAnim_Begin(&s_anim);
}

static int anim_done(void) {
    if (!s_anim_id) return 1;
    if (MoveAnim_IsDone(&s_anim)) { s_anim_id = 0; return 1; }
    MoveAnim_Tick(&s_anim);
    if (MoveAnim_IsDone(&s_anim)) { s_anim_id = 0; return 1; }
    return 0;
}

static uint8_t s_saved_options, s_saved_scx, s_saved_scy, s_saved_whose_turn;

void TradeAnim_Begin(void) {
    s_saved_options = wOptions;
    s_saved_scx     = hSCX;
    s_saved_scy     = hSCY;

    s_saved_whose_turn = hWhoseTurn;
    hWhoseTurn = 0;
    wOptions = 0;
    hSCX = 0;
    hSCY = 0;

    s_state = TA_LOAD_GFX;
    s_phase = 0;
    s_timer = 0;
    s_i     = 0;
    s_anim_id = 0;
    s_bgp = 0xE4;
}

int TradeAnim_IsActive(void) {
    return s_state != TA_IDLE && s_state != TA_DONE;
}

void TradeAnim_Tick(void) {
    switch (s_state) {

    case TA_LOAD_GFX:
        tc_clear();
        TradeGfx_LoadTiles();
        PartyIcons_LoadTiles();
        trade_clear_sprites();
        trade_window_off();
        hSCX = 0;
        hSCY = 0;
        s_bgp = 0xE4;

        Display_SetPalette(0xE4, trade_obp0(), 0xD0);

        GbcColor_SetPalTradeGeneric();
        snprintf(s_player_name, sizeof s_player_name, "%s",
                 Pokemon_GetName(Species_Dex(gTradeAnim.player_species)));
        snprintf(s_enemy_name, sizeof s_enemy_name, "%s",
                 Pokemon_GetName(Species_Dex(gTradeAnim.enemy_species)));
        s_state = TA_SHOW_PLAYER_MON;
        s_phase = 0;
        break;

    case TA_SHOW_PLAYER_MON:
        switch (s_phase) {
        case 0:

            card_draw_window(gTradeAnim.player_species, s_player_name,
                             gTradeAnim.player_ot, gTradeAnim.player_otid);
            hWY = 0x50;
            hWX = 0x86;
            hSCX = 0x86;
            tc_clear();
            tc_load_mon_pic(gTradeAnim.player_species);
            s_timer = 10;
            s_i = 0x7E;
            s_phase = 1;
            break;
        case 1:
            if (--s_timer > 0) break;
            s_phase = 2;
            break;
        case 2: {

            hWX  = (uint8_t)s_i;
            hSCX = (uint8_t)s_i;
            s_i -= 2;
            if (s_i <= 0) { s_timer = 80; s_phase = 3; }
            break;
        }
        case 3:
            if (--s_timer > 0) break;
            start_anim(TRADE_BALL_POOF_ANIM);
            s_phase = 4;
            break;
        case 4:
            if (!anim_done()) break;
            start_anim(TRADE_BALL_DROP_ANIM);
            s_phase = 5;
            break;
        case 5:
            if (!anim_done()) break;
            Audio_PlayCry(gTradeAnim.player_species);
            s_state = TA_OPEN_CABLE_1;
            s_phase = 0;
            break;
        }
        break;

    case TA_OPEN_CABLE_1:
    case TA_OPEN_CABLE_2:
        switch (s_phase) {
        case 0:
            trade_window_off();
            tc_clear();
            s_timer = 10;
            s_phase = 1;
            break;
        case 1:
            if (--s_timer > 0) break;
            hSCX = 0xA0;
            s_timer = 1;
            s_phase = 2;
            break;
        case 2:
            if (--s_timer > 0) break;
            tc_blit_map(6, 2, (const uint8_t *)gTradeLinkCableMap,
                        TRADE_LINK_CABLE_MAP_W, TRADE_LINK_CABLE_MAP_H);
            s_timer = 3;
            s_phase = 3;
            break;
        case 3:
            if (--s_timer > 0) break;
            Audio_PlaySFX_HealHP();

            hSCX = 0xF0;
            s_state = (s_state == TA_OPEN_CABLE_1) ? TA_BALL_ENTER_CABLE
                                                   : TA_SHOW_ENEMY_MON;
            s_phase = 0;
            break;
        }
        break;

    case TA_BALL_ENTER_CABLE:
        switch (s_phase) {
        case 0:
            start_anim(TRADE_BALL_SHAKE_ANIM);
            s_phase = 1;
            break;
        case 1:
            if (!anim_done()) break;
            s_timer = 10;
            s_phase = 2;
            break;
        case 2:
            if (--s_timer > 0) break;

            Display_SetPalette(s_bgp, trade_obp0(), 0xD0);
            GbcColor_SetPalTradeGeneric();
            s_bulge = 0;
            s_i = 0x60;
            draw_cable_ball();
            s_timer = 3;
            s_phase = 3;
            break;
        case 3:

            if (--s_timer > 0) break;
            s_i += 4;
            if (s_i >= 0xA0) {
                trade_clear_sprites();
                tc_clear();
                s_state = TA_ANIM_L2R;
                s_phase = 0;
                break;
            }
            Audio_PlaySFX_Tink();
            draw_cable_ball();
            s_timer = 3;
            break;
        }
        break;

    case TA_ANIM_L2R:
        switch (s_phase) {
        case 0:

            tc_clear();
            hSCX = 0;
            trade_window_off();
            trade_load_party_sprite_gfx();
            s_moving_right = 1;
            s_base_x = 0x54;
            s_base_y = 0x1C;
            s_icon_species = gTradeAnim.player_species;
            write_circled_mon_oam();
            draw_left_gameboy();
            s_i = 6;
            s_timer = 8;
            s_phase = 1;
            break;
        case 1:
        case 3:
        case 5: {
            hSCX = (uint8_t)(hSCX + 2);
            if (--s_timer > 0) break;
            anim_circled_mon();
            s_timer = 8;
            if (--s_i > 0) break;
            if (s_phase == 1)      { draw_cable_across(); s_i = 4; s_phase = 3; }
            else if (s_phase == 3) { draw_right_gameboy(); s_i = 6; s_phase = 5; }
            else                   { s_i = 0; s_phase = 6; }
            break;
        }
        case 6:

            add_offsets_to_oam(4, 0);
            anim_circled_mon();
            s_timer = 8;
            s_i = 1;
            s_phase = 7;
            break;
        case 7:
            if (--s_timer > 0) break;
            s_timer = 8;
            if (s_i < 4) { add_offsets_to_oam(4, 0); anim_circled_mon(); s_i++; break; }
            add_offsets_to_oam(0, 10);
            anim_circled_mon();
            s_i = 1;
            s_phase = 8;
            break;
        case 8:
            if (--s_timer > 0) break;
            s_timer = 8;
            if (s_i < 4) { add_offsets_to_oam(0, 10); anim_circled_mon(); s_i++; break; }
            trade_clear_sprites();
            s_state = TA_DELAY100_1;
            s_phase = 0;
            s_timer = 100;
            break;
        }
        break;

    case TA_DELAY100_1:
        if (--s_timer > 0) break;
        s_state = TA_CLEARWIN_1;
        s_phase = 0;
        break;

    case TA_CLEARWIN_1:
    case TA_CLEARWIN_2:
        tc_clear();
        trade_clear_sprites();
        hSCX = 0;
        s_bgp = 0xE4;
        Display_SetPalette(0xE4, trade_obp0(), 0xD0);
        GbcColor_SetPalTradeGeneric();
        trade_window_off();
        s_state = (s_state == TA_CLEARWIN_1) ? TA_TEXT_WENT_TO : TA_OPEN_CABLE_2;
        s_phase = 0;
        break;

    case TA_TEXT_WENT_TO:
        switch (s_phase) {
        case 0:
            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();
            Text_SuppressCursorNext();
            RomTextSpliceN(s_msg, sizeof s_msg, "_TradeWentToText",
                           "{name}",      s_player_name,
                           "{ram:D887}",  gTradeAnim.enemy_trainer, NULL);
            Text_ShowASCII(s_msg);
            s_timer = 200;
            s_phase = 1;
            break;
        case 1:
            if (--s_timer > 0) break;

            s_slide_next = TA_TEXT_FOR_SENDS;
            s_state = TA_SLIDE_TEXT;
            s_phase = 0;
            break;
        }
        break;

    case TA_TEXT_FOR_SENDS:
        switch (s_phase) {
        case 0:
            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();
            Text_SuppressCursorNext();

            RomTextSpliceN(s_msg, sizeof s_msg, "_TradeForText",
                           "{name}", s_player_name, NULL);
            Text_ShowASCII(s_msg);
            s_timer = 80;
            s_phase = 1;
            break;
        case 1:

            if (--s_timer > 0) break;
            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();
            Text_SuppressCursorNext();
            RomTextSpliceN(s_msg, sizeof s_msg, "_TradeSendsText",
                           "{ram:D887}", gTradeAnim.enemy_trainer,
                           "{badge}",    s_enemy_name, NULL);
            Text_ShowASCII(s_msg);
            s_timer = 80;
            s_phase = 2;
            break;
        case 2:
            if (--s_timer > 0) break;
            s_state = TA_TEXT_FAREWELL;
            s_phase = 0;
            break;
        }
        break;

    case TA_TEXT_FAREWELL:
        switch (s_phase) {
        case 0:
            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();
            Text_SuppressCursorNext();
            RomTextSpliceN(s_msg, sizeof s_msg, "_TradeWavesFarewellText",
                           "{ram:D887}", gTradeAnim.enemy_trainer, NULL);
            Text_ShowASCII(s_msg);
            s_timer = 80;
            s_phase = 1;
            break;
        case 1:
            if (--s_timer > 0) break;
            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();
            Text_SuppressCursorNext();
            RomTextSpliceN(s_msg, sizeof s_msg, "_TradeTransferredText",
                           "{badge}", s_enemy_name, NULL);
            Text_ShowASCII(s_msg);
            s_timer = 80;
            s_phase = 2;
            break;
        case 2:
            if (--s_timer > 0) break;

            s_slide_next = TA_ANIM_R2L;
            s_state = TA_SLIDE_TEXT;
            s_phase = 0;
            break;
        }
        break;

    case TA_ANIM_R2L:
        switch (s_phase) {
        case 0:
            tc_clear();
            hSCX = 0;
            trade_window_off();
            trade_load_party_sprite_gfx();
            s_moving_right = 0;
            s_base_x = 0x64;
            s_base_y = 0x44;
            s_icon_species = gTradeAnim.enemy_species;
            write_circled_mon_oam();
            draw_right_gameboy();

            add_offsets_to_oam(0, -10);
            anim_circled_mon();
            s_timer = 8;
            s_i = 1;
            s_phase = 1;
            break;
        case 1:
            if (--s_timer > 0) break;
            s_timer = 8;
            if (s_i < 4) { add_offsets_to_oam(0, -10); anim_circled_mon(); s_i++; break; }
            add_offsets_to_oam(-4, 0);
            anim_circled_mon();
            s_i = 1;
            s_phase = 2;
            break;
        case 2:
            if (--s_timer > 0) break;
            s_timer = 8;
            if (s_i < 4) { add_offsets_to_oam(-4, 0); anim_circled_mon(); s_i++; break; }
            s_i = 6;
            s_phase = 3;
            break;
        case 3:
        case 5:
        case 7: {
            hSCX = (uint8_t)(hSCX - 2);
            if (--s_timer > 0) break;
            anim_circled_mon();
            s_timer = 8;
            if (--s_i > 0) break;
            if (s_phase == 3)      { draw_cable_across(); s_i = 4; s_phase = 5; }
            else if (s_phase == 5) { draw_left_gameboy();  s_i = 6; s_phase = 7; }
            else {
                trade_clear_sprites();
                s_state = TA_CLEARWIN_2;
                s_phase = 0;
            }
            break;
        }
        }
        break;

    case TA_SHOW_ENEMY_MON:
        switch (s_phase) {
        case 0:
            start_anim(TRADE_BALL_TILT_ANIM);
            s_phase = 1;
            break;
        case 1:
            if (!anim_done()) break;

            tc_clear();
            trade_clear_sprites();
            hSCX = 0;
            trade_window_off();
            card_draw_bg(10, gTradeAnim.enemy_species, s_enemy_name,
                         gTradeAnim.enemy_ot, gTradeAnim.enemy_otid);
            s_timer = 3;
            s_phase = 2;
            break;
        case 2:

            if (--s_timer > 0) break;
            tc_load_mon_pic(gTradeAnim.enemy_species);
            s_timer = 10;
            s_phase = 3;
            break;
        case 3:
            if (--s_timer > 0) break;
            start_anim(TRADE_BALL_POOF_ANIM);
            s_phase = 4;
            break;
        case 4:
            if (!anim_done()) break;
            Audio_PlayCry(gTradeAnim.enemy_species);
            s_timer = 100;
            s_phase = 5;
            break;
        case 5:
            if (--s_timer > 0) break;

            tc_clear_area(4, 10, 12, 8);
            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();
            Text_SuppressCursorNext();
            RomTextSpliceN(s_msg, sizeof s_msg, "_TradeTakeCareText",
                           "{badge}", s_enemy_name, NULL);
            Text_ShowASCII(s_msg);
            s_timer = 80;
            s_phase = 6;
            break;
        case 6:
            if (--s_timer > 0) break;

            s_state = TA_DELAY100_2;
            s_timer = 100;
            s_phase = 0;
            break;
        }
        break;

    case TA_SLIDE_TEXT:
        switch (s_phase) {
        case 0:
            s_timer = 50;
            s_phase = 1;
            break;
        case 1:
            if (--s_timer > 0) break;
            s_phase = 2;
            break;
        case 2:
            hWX = (uint8_t)(hWX + 2);
            if (hWX >= 0xA1) {

                Text_Close();
                tc_clear();
                s_timer = 10;
                s_phase = 3;
            }
            break;
        case 3:
            if (--s_timer > 0) break;
            hWX = 7;
            s_state = s_slide_next;
            s_phase = 0;
            break;
        }
        break;

    case TA_DELAY100_2:
        if (--s_timer > 0) break;
        s_state = TA_CLEANUP;
        break;

    case TA_CLEANUP:
        Text_Close();
        tc_clear();
        trade_clear_sprites();
        trade_window_off();
        trade_present();

        wOptions   = s_saved_options;
        hSCX       = s_saved_scx;
        hSCY       = s_saved_scy;
        hWhoseTurn = s_saved_whose_turn;
        gScrollPxX = 0;
        gScrollPxY = 0;
        Display_SetPalette(0xE4, 0xD0, 0xE0);

        GbcColor_MarkDirty();
        MoveAnim_SetTradeHooks(0);
        s_state = TA_DONE;
        break;

    default:
        break;
    }

    if (TradeAnim_IsActive()) trade_present();
}

const npc_trade_t gTradeMons[NUM_NPC_TRADES] = {
    { SPECIES_NIDORINO,   SPECIES_NIDORINA,  TRADE_DIALOGSET_CASUAL,    "TERRY"      },
    { SPECIES_ABRA,       SPECIES_MR_MIME,   TRADE_DIALOGSET_CASUAL,    "MARCEL"     },
    { SPECIES_BUTTERFREE, SPECIES_BEEDRILL,  TRADE_DIALOGSET_HAPPY,     "CHIKUCHIKU" },
    { SPECIES_PONYTA,     SPECIES_SEEL,      TRADE_DIALOGSET_CASUAL,    "SAILOR"     },
    { SPECIES_SPEAROW,    SPECIES_FARFETCHD, TRADE_DIALOGSET_HAPPY,     "DUX"        },
    { SPECIES_SLOWBRO,    SPECIES_LICKITUNG, TRADE_DIALOGSET_CASUAL,    "MARC"       },
    { SPECIES_POLIWHIRL,  SPECIES_JYNX,      TRADE_DIALOGSET_EVOLUTION, "LOLA"       },
    { SPECIES_RAICHU,     SPECIES_ELECTRODE, TRADE_DIALOGSET_EVOLUTION, "DORIS"      },
    { SPECIES_VENONAT,    SPECIES_TANGELA,   TRADE_DIALOGSET_HAPPY,     "CRINKLES"   },
    { SPECIES_NIDORAN_M,  SPECIES_NIDORAN_F, TRADE_DIALOGSET_HAPPY,     "SPOT"       },
};

static const char *const kTradeText[NUM_TRADE_DIALOGSETS][5] = {

    { "WannaTrade1Text", "NoTrade1Text", "WrongMon1Text",
      "Thanks1Text",     "AfterTrade1Text" },

    { "WannaTrade2Text", "NoTrade2Text", "WrongMon2Text",
      "Thanks2Text",     "AfterTrade2Text" },

    { "WannaTrade3Text", "NoTrade3Text", "WrongMon3Text",
      "Thanks3Text",     "AfterTrade3Text" },
};

#define TRADETEXT_WANNA_TRADE 0
#define TRADETEXT_NO_TRADE    1
#define TRADETEXT_WRONG_MON   2
#define TRADETEXT_THANKS      3
#define TRADETEXT_AFTER_TRADE 4

typedef enum {
    TR_IDLE = 0,
    TR_ASK,
    TR_PARTY,
    TR_CONNECT,
    TR_MOVIE,
    TR_TRADED_FOR,
    TR_EVO,
    TR_EVO_WAIT,
    TR_LINE,
    TR_DONE
} trade_flow_t;

static trade_flow_t s_flow = TR_IDLE;
static uint8_t s_which;
static uint8_t s_give, s_get;
static uint8_t s_dialogset;
static char s_nick[NAME_LENGTH];

static int  s_custom;
static int  s_result = TRADE_RESULT_CANCELLED;
static int  s_line;
static int  s_slot;
static uint8_t s_given_level;
static char s_flow_msg[256];

static void trade_text(const char *symbol, char *out, size_t n) {
    RomTextSpliceN(out, n, symbol,
                   "{ram:CD13}", Pokemon_GetNameBySpecies(s_give),
                   "{ram:CD1E}", Pokemon_GetNameBySpecies(s_get), NULL);
}

static void trade_say(int which_line) {
    trade_text(kTradeText[s_dialogset][which_line], s_flow_msg, sizeof s_flow_msg);
    Text_ShowASCII(s_flow_msg);
}

static void trade_finish_with(int which_line) {
    s_line = which_line;
    s_flow = TR_LINE;
    trade_say(which_line);
}

int Trade_IsActive(void) { return s_flow != TR_IDLE && s_flow != TR_DONE; }

void Trade_Abort(void) {
    if (!Trade_IsActive()) return;
    s_flow = TR_IDLE;

}

int Trade_Begin(uint8_t which) {
    if (which >= NUM_NPC_TRADES) return 0;
    if (Trade_IsActive()) return 0;

    s_which     = which;
    s_give      = gTradeMons[which].give;
    s_get       = gTradeMons[which].get;
    s_dialogset = gTradeMons[which].dialogset;
    s_custom    = 0;
    s_result    = TRADE_RESULT_CANCELLED;
    snprintf(s_nick, sizeof s_nick, "%s", gTradeMons[which].nick);

    if (wCompletedInGameTradeFlags & (1u << which)) {
        trade_finish_with(TRADETEXT_AFTER_TRADE);
        return 1;
    }

    trade_text(kTradeText[s_dialogset][TRADETEXT_WANNA_TRADE],
               s_flow_msg, sizeof s_flow_msg);
    YesNo_Show(s_flow_msg);
    s_flow = TR_ASK;
    return 1;
}

int Trade_GetResult(void) { return s_result; }

int Trade_BeginCustom(uint8_t give, uint8_t get, const char *nick) {
    if (Trade_IsActive()) return 0;
    if (give == 0 || get == 0) return 0;

    s_which     = 0xFF;
    s_give      = give;
    s_get       = get;
    s_dialogset = TRADE_DIALOGSET_CASUAL;
    s_custom    = 1;
    s_result    = TRADE_RESULT_CANCELLED;
    snprintf(s_nick, sizeof s_nick, "%s",
             (nick && *nick) ? nick : Pokemon_GetNameBySpecies(get));

    PartyMenu_Open(PARTY_MENU_TRADE);
    s_flow = TR_PARTY;
    return 1;
}

static void trade_prepare_anim(void) {
    memset(&gTradeAnim, 0, sizeof gTradeAnim);
    gTradeAnim.player_species = s_give;
    gTradeAnim.enemy_species  = s_get;
    memcpy(gTradeAnim.player_ot, wPartyMonOT[s_slot], NAME_LENGTH);
    gTradeAnim.player_otid = wPartyMons[s_slot].base.ot_id;

    snprintf(gTradeAnim.enemy_ot, sizeof gTradeAnim.enemy_ot, "TRAINER");
    gTradeAnim.enemy_otid = (uint16_t)((BattleRandom() << 8) | BattleRandom());

    snprintf(gTradeAnim.enemy_trainer, sizeof gTradeAnim.enemy_trainer, "TRAINER");
}

static void trade_apply(void) {
    Pokemon_RemoveFromParty(s_slot);

    Pokemon_AddToParty(s_get, s_given_level);
    if (wPartyCount == 0) return;
    int slot = wPartyCount - 1;

    Pokemon_EncodeNameString(s_nick, wPartyMonNicks[slot]);
    Pokemon_EncodeNameString(gTradeAnim.enemy_ot, wPartyMonOT[slot]);
    wPartyMons[slot].base.ot_id = gTradeAnim.enemy_otid;

    Pokedex_SetOwned(s_get);

    s_result = TRADE_RESULT_TRADED;

    if (!s_custom) wCompletedInGameTradeFlags |= (uint16_t)(1u << s_which);
}

void Trade_Tick(void) {
    switch (s_flow) {

    case TR_ASK:
        if (YesNo_IsOpen()) break;

        Text_Close();
        if (YesNo_GetResult() != 1) {
            trade_finish_with(TRADETEXT_NO_TRADE);
            break;
        }
        PartyMenu_Open(PARTY_MENU_TRADE);
        s_flow = TR_PARTY;
        break;

    case TR_PARTY: {

        if (PartyMenu_IsOpen()) break;
        int slot = PartyMenu_GetSelected();
        if (slot < 0) {
            s_result = TRADE_RESULT_CANCELLED;
            if (s_custom) { s_flow = TR_DONE; break; }
            trade_finish_with(TRADETEXT_NO_TRADE);
            break;
        }
        if (wPartyMons[slot].base.species != s_give) {
            s_result = TRADE_RESULT_WRONG_MON;
            if (s_custom) { s_flow = TR_DONE; break; }
            trade_finish_with(TRADETEXT_WRONG_MON);
            break;
        }
        s_slot = slot;
        s_given_level = wPartyMons[slot].level;
        trade_prepare_anim();
        Text_ShowASCII(RomText("_ConnectCableText"));
        s_flow = TR_CONNECT;
        break;
    }

    case TR_CONNECT:
        if (Text_IsOpen()) break;
        Game_BeginTradeAnim();
        s_flow = TR_MOVIE;
        break;

    case TR_MOVIE:

        if (Game_GetScene() != 0 ) break;
        trade_apply();

        snprintf(s_flow_msg, sizeof s_flow_msg, "{PLAYER} traded\n%s for\n%s!",
                 Pokemon_GetNameBySpecies(s_give), Pokemon_GetNameBySpecies(s_get));
        Text_SetPendingSFXOnPrint(Audio_PlaySFX_GetKeyItem);
        Text_ShowASCII(s_flow_msg);
        s_flow = TR_TRADED_FOR;
        break;

    case TR_TRADED_FOR:
        if (Text_IsOpen()) break;
        s_flow = TR_EVO;
        break;

    case TR_EVO:

        wWhichPokemon   = (uint8_t)(wPartyCount ? wPartyCount - 1 : 0);
        wForceEvolution = 1;
        wLinkState      = LINK_STATE_TRADING;
        wCanEvolveFlags = (uint8_t)(1u << wWhichPokemon);
        {
            int evolving = Game_BeginFieldEvolution();

            wLinkState      = 0;
            wForceEvolution = 0;
            if (evolving) { s_flow = TR_EVO_WAIT; break; }
            wCanEvolveFlags = 0;
        }
        if (s_custom) { s_flow = TR_DONE; break; }
        trade_finish_with(TRADETEXT_THANKS);
        break;

    case TR_EVO_WAIT:

        if (Game_GetScene() != 0 ) break;
        wCanEvolveFlags = 0;
        if (s_custom) { s_flow = TR_DONE; break; }
        trade_finish_with(TRADETEXT_THANKS);
        break;

    case TR_LINE:

        if (Text_IsOpen()) break;
        s_flow = TR_DONE;
        break;

    default:
        break;
    }
}
