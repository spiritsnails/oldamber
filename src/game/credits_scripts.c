#include "credits_scripts.h"
#include "assetpack_bind.h"
#include "constants.h"
#include "music.h"
#include "johto_music.h"
#include "gbc_color.h"
#include "overworld.h"
#include "player.h"
#include "../data/base_stats.h"
#include "../data/credits_data.h"
#include "../data/font_data.h"
#include "../data/pokemon_sprites.h"
#include "mon_pic.h"
#include "gen2_species.h"
#include "../data/splash_screen_data.h"
#include "../platform/display.h"
#include "../platform/game_version.h"
#include "../platform/hardware.h"
#include <stdint.h>
#include <string.h>

typedef enum {
    CREDITS_IDLE = 0,
    CREDITS_START_WAIT,
    CREDITS_PAGE_SETUP,
    CREDITS_PAGE_PARSE,
    CREDITS_FADE_WAIT,
    CREDITS_POST_WAIT,
    CREDITS_THE_END_WAIT,
    CREDITS_THE_END_FADE_WAIT,
    CREDITS_MON_ANIM_WAIT,
    CREDITS_DONE,
} credits_state_t;

enum {
    CMD_TEXT_FADE_MON = -1,
    CMD_TEXT_MON      = -2,
    CMD_TEXT_FADE     = -3,
    CMD_TEXT          = -4,
    CMD_COPYRIGHT     = -5,
    CMD_THE_END       = -6,
};

typedef struct {
    int8_t x_off;
    const char *text;
} cred_line_t;

enum {
    TXT_VERSION = 0, TXT_TAJIRI, TXT_TA_OOTA, TXT_MORIMOTO, TXT_WATANABE,
    TXT_MASUDA, TXT_NISINO, TXT_SUGIMORI, TXT_NISHIDA, TXT_MIYAMOTO,
    TXT_KAWAGUCHI, TXT_ISHIHARA, TXT_YAMAUCHI, TXT_ZINNAI, TXT_HISHIDA,
    TXT_SAKAI, TXT_YAMAGUCHI, TXT_YAMAMOTO, TXT_TANIGUCHI, TXT_NONOMURA,
    TXT_FUZIWARA, TXT_MATSUSIMA, TXT_TOMISAWA, TXT_KAWAMOTO, TXT_KAKEI,
    TXT_TSUCHIYA, TXT_TA_NAKAMURA, TXT_YUDA, TXT_MON, TXT_DIRECTOR,
    TXT_PROGRAMMERS, TXT_CHAR_DESIGN, TXT_MUSIC, TXT_SOUND_EFFECTS,
    TXT_GAME_DESIGN, TXT_MONSTER_DESIGN, TXT_GAME_SCENE, TXT_PARAM,
    TXT_MAP, TXT_TEST, TXT_SPECIAL, TXT_PRODUCERS, TXT_PRODUCER,
    TXT_EXECUTIVE, TXT_TAMADA, TXT_SA_OOTA, TXT_YOSHIKAWA, TXT_TO_OOTA,
    TXT_US_STAFF, TXT_US_COORD, TXT_TILDEN, TXT_KAWAKAMI, TXT_HI_NAKAMURA,
    TXT_GIESE, TXT_OSBORNE, TXT_TRANS, TXT_OGASAWARA, TXT_IWATA,
    TXT_IZUSHI, TXT_HARADA, TXT_MURAKAWA, TXT_FUKUI, TXT_CLUB, TXT_PAAD
};

static const cred_line_t kCredText[] = {
    { -8, "RED VERSION STAFF" }, { -6, "SATOSHI TAJIRI" }, { -6, "TAKENORI OOTA" },
    { -7, "SHIGEKI MORIMOTO" }, { -7, "TETSUYA WATANABE" }, { -6, "JUNICHI MASUDA" },
    { -5, "KOHJI NISINO" }, { -5, "KEN SUGIMORI" }, { -6, "ATSUKO NISHIDA" },
    { -7, "SHIGERU MIYAMOTO" }, { -8, "TAKASHI KAWAGUCHI" }, { -8, "TSUNEKAZU ISHIHARA" },
    { -7, "HIROSHI YAMAUCHI" }, { -7, "HIROYUKI ZINNAI" }, { -7, "TATSUYA HISHIDA" },
    { -6, "YASUHIRO SAKAI" }, { -7, "WATARU YAMAGUCHI" }, { -8, "KAZUYUKI YAMAMOTO" },
    { -8, "RYOHSUKE TANIGUCHI" }, { -8, "FUMIHIRO NONOMURA" }, { -7, "MOTOFUMI FUZIWARA" },
    { -7, "KENJI MATSUSIMA" }, { -7, "AKIHITO TOMISAWA" }, { -7, "HIROSHI KAWAMOTO" },
    { -6, "AKIYOSHI KAKEI" }, { -7, "KAZUKI TSUCHIYA" }, { -6, "TAKEO NAKAMURA" },
    { -6, "MASAMITSU YUDA" }, { -3, "#MON" }, { -3, "DIRECTOR" }, { -5, "PROGRAMMERS" },
    { -7, "CHARACTER DESIGN" }, { -2, "MUSIC" }, { -6, "SOUND EFFECTS" },
    { -5, "GAME DESIGN" }, { -6, "MONSTER DESIGN" }, { -6, "GAME SCENARIO" },
    { -8, "PARAMETRIC DESIGN" }, { -4, "MAP DESIGN" }, { -7, "PRODUCT TESTING" },
    { -6, "SPECIAL THANKS" }, { -4, "PRODUCERS" }, { -4, "PRODUCER" },
    { -8, "EXECUTIVE PRODUCER" }, { -6, "SOUSUKE TAMADA" }, { -5, "SATOSHI OOTA" },
    { -6, "RENA YOSHIKAWA" }, { -6, "TOMOMICHI OOTA" }, { -7, "US VERSION STAFF" },
    { -7, "US COORDINATION" }, { -5, "GAIL TILDEN" }, { -6, "NAOKO KAWAKAMI" },
    { -6, "HIRO NAKAMURA" }, { -6, "WILLIAM GIESE" }, { -5, "SARA OSBORNE" },
    { -7, "TEXT TRANSLATION" }, { -6, "NOB OGASAWARA" }, { -5, "SATORU IWATA" },
    { -7, "TAKEHIRO IZUSHI" }, { -7, "TAKAHIRO HARADA" }, { -7, "TERUKI MURAKAWA" },
    { -5, "KOHTA FUKUI" }, { -9, "NCL SUPER MARIO CLUB" }, { -5, "PAAD TESTING" },
};

static const int16_t kCreditsOrder[] = {
    TXT_MON, TXT_VERSION, CMD_TEXT_FADE_MON,
    TXT_DIRECTOR, TXT_TAJIRI, CMD_TEXT_FADE_MON,
    TXT_PROGRAMMERS, TXT_TA_OOTA, TXT_MORIMOTO, CMD_TEXT_FADE,
    TXT_PROGRAMMERS, TXT_WATANABE, TXT_MASUDA, TXT_TAMADA, CMD_TEXT_MON,
    TXT_CHAR_DESIGN, TXT_SUGIMORI, TXT_NISHIDA, CMD_TEXT_FADE_MON,
    TXT_MUSIC, TXT_MASUDA, CMD_TEXT_FADE,
    TXT_SOUND_EFFECTS, TXT_MASUDA, CMD_TEXT_MON,
    TXT_GAME_DESIGN, TXT_TAJIRI, CMD_TEXT_FADE_MON,
    TXT_MONSTER_DESIGN, TXT_SUGIMORI, TXT_NISHIDA, TXT_FUZIWARA, CMD_TEXT_FADE,
    TXT_MONSTER_DESIGN, TXT_MORIMOTO, TXT_SA_OOTA, TXT_YOSHIKAWA, CMD_TEXT_MON,
    TXT_GAME_SCENE, TXT_TAJIRI, CMD_TEXT_FADE,
    TXT_GAME_SCENE, TXT_TANIGUCHI, TXT_NONOMURA, TXT_ZINNAI, CMD_TEXT_MON,
    TXT_PARAM, TXT_NISINO, TXT_TA_NAKAMURA, CMD_TEXT_FADE_MON,
    TXT_MAP, TXT_TAJIRI, TXT_NISINO, CMD_TEXT_FADE,
    TXT_MAP, TXT_MATSUSIMA, TXT_NONOMURA, TXT_TANIGUCHI, CMD_TEXT_MON,
    TXT_TEST, TXT_KAKEI, TXT_TSUCHIYA, CMD_TEXT_FADE,
    TXT_TEST, TXT_TA_NAKAMURA, TXT_YUDA, CMD_TEXT_MON,
    TXT_SPECIAL, TXT_HISHIDA, TXT_SAKAI, CMD_TEXT_FADE,
    TXT_SPECIAL, TXT_YAMAGUCHI, TXT_YAMAMOTO, CMD_TEXT,
    TXT_SPECIAL, TXT_TOMISAWA, TXT_KAWAMOTO, TXT_TO_OOTA, CMD_TEXT_MON,
    TXT_PRODUCERS, TXT_MIYAMOTO, CMD_TEXT_FADE,
    TXT_PRODUCERS, TXT_KAWAGUCHI, CMD_TEXT,
    TXT_PRODUCERS, TXT_ISHIHARA, CMD_TEXT_MON,
    TXT_US_STAFF, CMD_TEXT_FADE,
    TXT_US_COORD, TXT_TILDEN, CMD_TEXT_FADE,
    TXT_US_COORD, TXT_KAWAKAMI, TXT_HI_NAKAMURA, CMD_TEXT,
    TXT_US_COORD, TXT_GIESE, TXT_OSBORNE, CMD_TEXT,
    TXT_TRANS, TXT_OGASAWARA, CMD_TEXT_FADE,
    TXT_PROGRAMMERS, TXT_MURAKAWA, TXT_FUKUI, CMD_TEXT_FADE,
    TXT_SPECIAL, TXT_IWATA, CMD_TEXT_FADE,
    TXT_SPECIAL, TXT_HARADA, CMD_TEXT,
    TXT_TEST, TXT_PAAD, TXT_CLUB, CMD_TEXT_FADE,
    TXT_PRODUCER, TXT_IZUSHI, CMD_TEXT_FADE,
    TXT_EXECUTIVE, TXT_YAMAUCHI, CMD_TEXT_FADE_MON,
    CMD_COPYRIGHT, CMD_TEXT_FADE_MON,
    CMD_THE_END
};

static credits_state_t g_state = CREDITS_IDLE;
static int g_timer = 0;
static int g_post_wait = 0;
static int g_order_pos = 0;
static int g_row = 6;
static int g_fade_step = 0;
static int g_mon_index = 0;
static int g_mon_step = 0;
static uint8_t g_mon_text_band[10][SCREEN_WIDTH];
static int g_restart_requested = 0;
static int g_done_wait_timer = 0;
static const uint8_t kSolidBlackTile[16] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const uint8_t kCreditsMons[] = {
    SPECIES_VENUSAUR, SPECIES_ARBOK, SPECIES_RHYHORN, SPECIES_FEAROW, SPECIES_ABRA,
    SPECIES_GRAVELER, SPECIES_HITMONLEE, SPECIES_TANGELA, SPECIES_STARMIE, SPECIES_GYARADOS,
    SPECIES_DITTO, SPECIES_OMASTAR, SPECIES_VILEPLUME, SPECIES_NIDOKING, SPECIES_PARASECT
};

static const uint8_t kCreditsFadeBGP[4] = { 0xC0, 0xD0, 0xE0, 0xF0 };

#define CREDITS_START_DELAY_TICKS      134
#define CREDITS_FADE_STEP_TICKS          5
#define CREDITS_POST_TEXT_FADE_MON      94
#define CREDITS_POST_TEXT_MON          115
#define CREDITS_POST_TEXT_FADE         126
#define CREDITS_POST_TEXT              147
#define CREDITS_THE_END_PREWAIT         17

#define CREDITS_POST_END_WAIT_TICKS    628

static const uint8_t kCopyrightRow1[] = {
    0x60,0x61,0x62,0x61,0x63,0x61,0x64,0x7F,0x65,0x66,0x67,0x68,0x69,0x6A
};

static const uint8_t kTheEndRow1[] = { 0x60,' ',0x62,' ',0x64,' ',' ',0x64,' ',0x66,' ',0x68 };
static const uint8_t kTheEndRow2[] = { 0x61,' ',0x63,' ',0x65,' ',' ',0x65,' ',0x67,' ',0x69 };

#define CREDITS_BLACK_TILE 0x5F

static void credits_put(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void credits_put_view(int col, int row, uint8_t tile) {
    int view_w = Display_FrameWidth() / TILE_PX;
    if ((unsigned)col >= (unsigned)view_w || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + col + 2] = tile;
}

static int ascii_to_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == ' ') return BLANK_TILE_SLOT;
    if (c == '-') return Font_CharToTile(0xE3);
    if (c == '.') return Font_CharToTile(0xE8);
    return BLANK_TILE_SLOT;
}

static void credits_clear_middle_white(void) {
    for (int r = 4; r < 14; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) credits_put(c, r, BLANK_TILE_SLOT);
    }
}

static void credits_clear_full_screen_white(void) {
    for (int r = 0; r < SCREEN_HEIGHT; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) credits_put(c, r, BLANK_TILE_SLOT);
    }
}

static void credits_clear_full_bg_map(void) {
    for (int r = 0; r < SCROLL_MAP_H; r++) {
        for (int c = 0; c < SCROLL_MAP_W; c++) {
            gScrollTileMap[r * SCROLL_MAP_W + c] = BLANK_TILE_SLOT;
        }
    }
}

static void credits_draw_black_bars(void) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) credits_put(c, r, CREDITS_BLACK_TILE);
    }
    for (int r = 14; r < SCREEN_HEIGHT; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) credits_put(c, r, CREDITS_BLACK_TILE);
    }
}

static void credits_draw_text_line(int col, int row, const char *s) {
    while (*s && col < SCREEN_WIDTH) {
        if (*s == '#') {
            if (col + 3 < SCREEN_WIDTH) {
                credits_put(col++, row, (uint8_t)Font_CharToTile(0x8F));
                credits_put(col++, row, (uint8_t)Font_CharToTile(0x8E));
                credits_put(col++, row, (uint8_t)Font_CharToTile(0x8A));
                credits_put(col++, row, (uint8_t)Font_CharToTile(0xBA));
            }
        } else {
            credits_put(col++, row, (uint8_t)ascii_to_tile((unsigned char)*s));
        }
        s++;
    }
}

static void credits_draw_tile_row(int col, int row, const uint8_t *tiles, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t t = tiles[i];
        if (t == ' ' || t == 0x7F) t = BLANK_TILE_SLOT;
        credits_put(col + i, row, t);
    }
}

static void credits_load_copyright_tiles(void) {
    for (int i = 0; i < SPLASH_LEGAL_TILES; i++) {
        Display_LoadTile((uint8_t)(0x60 + i), gSplashLegalTiles[i]);
    }
}

static void credits_load_the_end_tiles(void) {
    for (int i = 0; i < CREDITS_THE_END_TILES; i++) {
        Display_LoadTile((uint8_t)(0x60 + i), gCreditsTheEndTiles[i]);
    }
}

static void credits_hide_all_sprites(void) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        wShadowOAM[i].y = 0;
        wShadowOAM[i].x = 0;
        wShadowOAM[i].tile = 0;
        wShadowOAM[i].flags = 0;
    }
}

static void credits_shift_font_color_index(void) {

    uint8_t tile[16];
    int tid;

    for (tid = 0x80; tid <= 0xFF; tid++) {
        Display_GetTile((uint8_t)tid, tile);
        for (int i = 0; i < 16; i += 2) tile[i] = 0;
        Display_LoadTile((uint8_t)tid, tile);
    }
    for (tid = 0x60; tid <= 0x7D; tid++) {
        Display_GetTile((uint8_t)tid, tile);
        for (int i = 0; i < 16; i += 2) tile[i] = 0;
        Display_LoadTile((uint8_t)tid, tile);
    }
    Display_GetTile(0x7F, tile);
    for (int i = 0; i < 16; i += 2) tile[i] = 0;
    Display_LoadTile(0x7F, tile);
}

static void credits_queue_fade(void) {

    Display_SetPalette(kCreditsFadeBGP[0], kCreditsFadeBGP[0], kCreditsFadeBGP[0]);
    g_fade_step = 1;
    g_timer = CREDITS_FADE_STEP_TICKS;
    g_state = CREDITS_FADE_WAIT;
}

static void credits_capture_middle_band(void) {
    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) {
            g_mon_text_band[r][c] =
                gScrollTileMap[(r + 4 + 2) * SCROLL_MAP_W + (c + 2) + Map_UiColOfs()];
        }
    }
}

static void credits_draw_mon_frame(int step) {
    int view_w = Display_FrameWidth() / TILE_PX;
    int content_x = (view_w - SCREEN_WIDTH) / 2;
    int mon_x = (view_w > SCREEN_WIDTH) ? view_w : SCREEN_WIDTH;

    if (view_w > SCREEN_WIDTH) {
        for (int r = 0; r < 10; r++) {
            for (int c = 0; c < view_w; c++) {
                int src = c + step;
                uint8_t t = BLANK_TILE_SLOT;
                if (src >= content_x && src < content_x + SCREEN_WIDTH) {
                    t = g_mon_text_band[r][src - content_x];
                } else if (src >= mon_x && src < mon_x + 7 && r >= 2 && r < 9) {
                    t = (uint8_t)(0x20 + (r - 2) * 7 + (src - mon_x));
                }
                credits_put_view(c, r + 4, t);
            }
        }
        return;
    }

    int window_x = SCREEN_WIDTH;
    if (step >= 8) window_x = 27 - step;

    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) {
            int src = (c + step) & 31;
            uint8_t t = BLANK_TILE_SLOT;
            if (src < SCREEN_WIDTH) {
                t = g_mon_text_band[r][src];
            } else if (src < SCREEN_WIDTH + 7 && r >= 2 && r < 9) {
                t = (uint8_t)(0x20 + (r - 2) * 7 + (src - SCREEN_WIDTH));
            }
            if (c >= window_x) t = BLANK_TILE_SLOT;
            credits_put(c, r + 4, t);
        }
    }
}

static void credits_clear_window_layer(void) {
    for (int r = 0; r < SCREEN_HEIGHT; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) gWindowTileMap[r][c] = 0;
    }
}

static void credits_load_mon_tiles(uint8_t species) {
    uint8_t dex = Species_Dex(species);
    for (int i = 0; i < 49; i++) {
        Display_LoadTile((uint8_t)(0x20 + i), MonPic_FrontTile(dex, i));
    }
}

static void credits_begin_mon_anim(void) {
    uint8_t species;
    if (g_mon_index >= (int)(sizeof(kCreditsMons) / sizeof(kCreditsMons[0]))) {
        g_mon_index = 0;
    }
    species = kCreditsMons[g_mon_index++];

    credits_capture_middle_band();
    credits_load_mon_tiles(species);
    g_mon_step = 0;
    credits_clear_window_layer();
    hWY = SCREEN_HEIGHT_PX;
    Display_SetAuthoredBleedRows(4, 10);
    credits_draw_mon_frame(0);

    Display_SetPalette(0xFC, 0xFC, 0xFC);
    g_timer = 0;
    g_state = CREDITS_MON_ANIM_WAIT;
}

void CreditsScripts_OnMapLoad(void) {
    if (g_state == CREDITS_IDLE) return;
    g_state = CREDITS_IDLE;
    g_timer = 0;
    g_post_wait = 0;
    g_order_pos = 0;
    g_row = 6;
    g_mon_index = 0;
    g_mon_step = 0;
    g_restart_requested = 0;
    g_done_wait_timer = 0;
    Display_SetAuthoredBleedRows(0, 0);
}

static void credits_start_common(int start_delay) {
    g_state = CREDITS_START_WAIT;
    g_timer = start_delay;
    g_order_pos = 0;
    g_row = 6;
    g_post_wait = 0;
    g_mon_index = 0;
    g_mon_step = 0;
    g_restart_requested = 0;
    g_done_wait_timer = 0;
    gScrollPxX = 0;
    gScrollPxY = 0;
    Display_SetBandXPx(-1, 0, 0);
    Display_SetAuthoredBleedRows(0, 0);
    credits_clear_window_layer();
    credits_shift_font_color_index();
    Display_LoadTile(CREDITS_BLACK_TILE, kSolidBlackTile);
    credits_hide_all_sprites();
    credits_clear_full_bg_map();
    credits_clear_full_screen_white();
    credits_draw_black_bars();
    Display_SetPalette(0xC0, 0xC0, 0xC0);

    if (GbcColor_IsEnabled()) {
        static const uint16_t kGrey[4] = {
            0x7FFF,
            0x56B5,
            0x294A,
            0x0000,
        };
        Display_SetPositionAttrMode(0);
        for (int i = 0; i < 8; i++) Display_SetBGColorPalette(i, kGrey);
        Display_SetOBJColorPalette(0, kGrey);
        Display_SetOBJColorPalette(1, kGrey);
        Display_ClearAttrBoxes(0);
        Display_SetColorMode(1);
    }
    hWY = SCREEN_HEIGHT_PX;
    hWX = 7;
    Display_SetWindowOverSprites(1);
    Music_Stop();
    Music_Play(MUSIC_CREDITS);
}

void CreditsScripts_Start(void) {

    credits_start_common(CREDITS_START_DELAY_TICKS);
}

void CreditsScripts_StartImmediate(void) {
    credits_start_common(0);
}

void CreditsScripts_Tick(void) {
    int token;
    int mon_last_step = (Display_FrameWidth() / TILE_PX > SCREEN_WIDTH)
                      ? Display_FrameWidth() / TILE_PX + 7
                      : 27;
    Display_SetAuthoredBleedRows(g_state == CREDITS_MON_ANIM_WAIT ? 4 : 0,
                                 g_state == CREDITS_MON_ANIM_WAIT ? 10 : 0);
    if (g_state != CREDITS_IDLE && g_state != CREDITS_DONE) {
        gScrollPxX = 0;
        gScrollPxY = 0;
        credits_hide_all_sprites();
    }
    switch (g_state) {
    case CREDITS_START_WAIT:
        if (g_timer > 0) {
            g_timer--;
            return;
        }
        g_state = CREDITS_PAGE_SETUP;
        return;

    case CREDITS_PAGE_SETUP:
        credits_clear_window_layer();
        hWY = SCREEN_HEIGHT_PX;
        hWX = 7;
        Display_SetWindowOverSprites(1);
        credits_draw_black_bars();
        credits_clear_middle_white();
        g_row = 6;
        g_state = CREDITS_PAGE_PARSE;

    case CREDITS_PAGE_PARSE:
        while (g_order_pos < (int)(sizeof(kCreditsOrder) / sizeof(kCreditsOrder[0]))) {
            token = kCreditsOrder[g_order_pos++];
            if (token >= 0 && token < (int)(sizeof(kCredText) / sizeof(kCredText[0]))) {
                int col = 9 + kCredText[token].x_off;
                const char *line = kCredText[token].text;
                if (token == TXT_VERSION && strcmp(GameVersion_Current(), "blue") == 0)
                    line = "BLUE VERSION STAFF";
                credits_draw_text_line(col, g_row, line);
                g_row += 2;
                continue;
            }
            if (token == CMD_TEXT_FADE_MON) {
                g_post_wait = CREDITS_POST_TEXT_FADE_MON;
                credits_queue_fade();
                return;
            }
            if (token == CMD_TEXT_MON) {
                g_timer = CREDITS_POST_TEXT_MON;
                g_state = CREDITS_POST_WAIT;
                return;
            }
            if (token == CMD_TEXT_FADE) {
                g_post_wait = CREDITS_POST_TEXT_FADE;
                credits_queue_fade();
                return;
            }
            if (token == CMD_TEXT) {
                g_timer = CREDITS_POST_TEXT;
                g_state = CREDITS_POST_WAIT;
                return;
            }
            if (token == CMD_COPYRIGHT) {
                credits_load_copyright_tiles();
                credits_draw_tile_row(2, 7, kCopyrightRow1, (int)sizeof(kCopyrightRow1));

                credits_draw_tile_row(2, 9, kCopyrightRow2, (int)kCopyrightRow2_count);
                credits_draw_tile_row(2, 11, kCopyrightRow3, (int)kCopyrightRow3_count);
                continue;
            }
            if (token == CMD_THE_END) {
                g_timer = CREDITS_THE_END_PREWAIT;
                g_state = CREDITS_THE_END_WAIT;
                return;
            }
        }
        g_done_wait_timer = CREDITS_POST_END_WAIT_TICKS;
        g_state = CREDITS_DONE;
        return;

    case CREDITS_FADE_WAIT:
        if (g_timer > 0) {
            g_timer--;
            return;
        }
        if (g_fade_step < 4) {
            uint8_t bgp = kCreditsFadeBGP[g_fade_step];
            Display_SetPalette(bgp, bgp, bgp);
            g_fade_step++;
            g_timer = CREDITS_FADE_STEP_TICKS;
            return;
        }

        g_timer = g_post_wait;
        g_state = CREDITS_POST_WAIT;
        return;

    case CREDITS_POST_WAIT:
        if (g_timer > 0) {
            g_timer--;
            return;
        }
        if (kCreditsOrder[g_order_pos - 1] == CMD_TEXT_MON ||
            kCreditsOrder[g_order_pos - 1] == CMD_TEXT_FADE_MON) {
            credits_begin_mon_anim();
            return;
        }
        g_state = CREDITS_PAGE_SETUP;
        return;

    case CREDITS_MON_ANIM_WAIT:
        if (g_timer > 0) {
            g_timer--;
            return;
        }
        g_mon_step++;
        credits_draw_mon_frame(g_mon_step);
        if (g_mon_step < mon_last_step) {
            g_timer = 0;
            return;
        }
        Display_SetBandXPx(-1, 0, 0);
        Display_SetAuthoredBleedRows(0, 0);
        credits_clear_window_layer();
        hWY = SCREEN_HEIGHT_PX;
        hWX = 7;
        Display_SetWindowOverSprites(1);
        Display_SetPalette(0xC0, 0xC0, 0xC0);
        g_state = CREDITS_PAGE_SETUP;
        return;

    case CREDITS_THE_END_WAIT:
        if (g_timer > 0) {
            g_timer--;
            return;
        }
        credits_clear_middle_white();
        credits_load_the_end_tiles();
        credits_draw_tile_row(4, 8, kTheEndRow1, (int)sizeof(kTheEndRow1));
        credits_draw_tile_row(4, 9, kTheEndRow2, (int)sizeof(kTheEndRow2));

        Display_SetPalette(kCreditsFadeBGP[0], kCreditsFadeBGP[0], kCreditsFadeBGP[0]);
        g_fade_step = 1;
        g_timer = CREDITS_FADE_STEP_TICKS;
        g_state = CREDITS_THE_END_FADE_WAIT;
        return;

    case CREDITS_THE_END_FADE_WAIT:
        if (g_timer > 0) {
            g_timer--;
            return;
        }
        if (g_fade_step < 4) {
            uint8_t bgp = kCreditsFadeBGP[g_fade_step];
            Display_SetPalette(bgp, bgp, bgp);
            g_fade_step++;
            g_timer = CREDITS_FADE_STEP_TICKS;
        } else {
            g_done_wait_timer = CREDITS_POST_END_WAIT_TICKS;
            g_state = CREDITS_DONE;
        }
        return;

    case CREDITS_DONE:
        if (g_done_wait_timer > 0) {
            g_done_wait_timer--;
            return;
        }
        if (hJoyPressed & (PAD_A | PAD_B)) {

            Music_Stop();
            JohtoMusic_Stop();

            g_state = CREDITS_IDLE;
            g_restart_requested = 1;
        }
        return;
    case CREDITS_IDLE:
    default:
        return;
    }
}

int CreditsScripts_IsActive(void) {
    return g_state != CREDITS_IDLE;
}

int CreditsScripts_ConsumeRestartRequest(void) {
    int v = g_restart_requested;
    g_restart_requested = 0;
    if (v) g_state = CREDITS_IDLE;
    return v;
}
