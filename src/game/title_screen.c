
#include "title_screen.h"
#include <stdio.h>
#include "assetpack_bind.h"
#include "overworld.h"
#include "constants.h"
#include "pokemon.h"
#include "music.h"
#include "player.h"
#include "../data/base_stats.h"
#include "../data/font_data.h"
#include "../data/pokemon_sprites.h"
#include "mon_pic.h"
#include "gbc_color.h"
#include "../data/gbc_palettes.h"

void TitleScreen_ApplyPalettes(void);
void TitleScreen_ApplyGameFreakPalettes(void);
void TitleScreen_ApplyNidorinoPalettes(void);
#include "gen2_species.h"
#include "../data/splash_screen_data.h"
#include "../data/title_screen_data.h"
#include "../data/intro_scene_data.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include <string.h>
#include "../platform/debug_log.h"

typedef enum {
    TS_SPLASH = 0,
    TS_GAMEFREAK_SETUP,
    TS_GAMEFREAK_STAR_FALL,
    TS_GAMEFREAK_FLASH,
    TS_GAMEFREAK_SMALL_STARS,
    TS_INTRO_CUTSCENE,
    TS_INTRO_FADE_TO_WHITE,
    TS_INTRO_POST_FADE,
    TS_BOUNCE,
    TS_BOUNCE_HOLD,
    TS_VERSION_SCROLL,
    TS_LOOP_WAIT,
    TS_SCROLL_OUT_MON,
    TS_WAIT_BALL,
    TS_SCROLL_IN_MON,
    TS_WAIT_CRY
} title_state_t;

#define kTitleMons      gTitleMons
#define TITLE_MON_COUNT ((int)gTitleMons_count)

static const int8_t kBounceDy[]    = { -4, 3, -3, 2, -2, 1, -1 };

static const uint8_t kBounceCount[] = { 17, 4, 4, 2, 2, 2, 2 };

#define TITLE_BOUNCE_HOLD_TICKS 38
#define BOUNCE_PHASE_COUNT ((int)(sizeof(kBounceDy) / sizeof(kBounceDy[0])))

static int gOpen    = 0;
static int gResult  = TITLE_SCREEN_PENDING;

static title_state_t gState = TS_SPLASH;
static int gTimer = 0;

static int gLogoPxY = 0;
static int gBouncePhase = 0;
static int gBounceLeft = 0;
static int gTitleSCY = 0;

static int gVersionScrollX = SCREEN_WIDTH;

static uint8_t gTitleMonSpecies = 0;
static int gClearSaveCombo = 0;
static int gMonBandPx = 0;

static int gMonScrollExtraPx = 0;

static int gMonScrollSubdiv  = 1;
static int gMonScrollSubTick = 0;
static int gMonScrollAcc     = 0;

static int gMonScrollLeaving = 0;

static int gMonTileColOffset = 0;
static uint8_t gMonScrollD = 0;
static const uint8_t *gMonScrollScript = NULL;
static int gMonScrollScriptPos = 0;
static int gMonScrollPhaseLeft = 0;
static int gMonScrollPhaseSpeed = 0;
static int gMonBBoxL = 0;
static int gMonBBoxR = 6;
static int gMonBBoxT = 0;
static int gMonBBoxB = 6;
static int gMonDrawDx = 0;
static int gMonDrawDy = 0;
static uint8_t gMonJustScrolledOut = STARTER1;
static int gBallY = 0x74;
static int gBallAnimPos = 0;
static int gBallAnimFramesLeft = 0;
static int gBallAnimStepHold = 0;
static int gBounceWindowActive = 0;
static uint8_t gGFObp0 = 0xE4;
static uint8_t gGFObp1 = 0xE4;
static int gGFFlashCyclesDone = 0;
static int gGFSmallStarsWave = 0;
static int gGFSmallStarsMoveIter = 0;
static int gGFSmallStarsMoveCount = 0;
static int gIntroSubstate = 0;
static int gIntroTimer = 0;
static int gIntroMoveType = 0;
static int gIntroMoveSteps = 0;
static int gIntroAnimStep = 0;
static const int8_t *gIntroAnim = NULL;
static int gIntroNidorinoBaseTile = 0;
static int gIntroSCX = 0;
static int gIntroFadeStep = 0;

#define TITLE_WAIT_CHECK_200_FRAMES  209
#define TITLE_WAIT_CHECK_1_FRAME        1

#define TS_STAR_CPAL            (OAM_FLAG_PALETTE | 1)

#define TS_GF_STAR_CPAL_BASE    4

#define TITLE_BG_LOGO_BASE      0x00
#define TITLE_BG_VERSION_BASE   0x70
#define TITLE_BG_COPY_BASE      0x88
#define TITLE_BG_MON_BASE       0xA0
#define TITLE_BG_SOLID_DARK     0x7F
#define TITLE_BG_LEGAL_BASE     0x30
#define TITLE_BG_INTRO_BLANK    0x00
#define TITLE_BG_INTRO_BACK_BASE 0x01
#define TITLE_OBJ_PLAYER_BASE   0x00
#define TITLE_OBJ_GF_SHOOT_BIG0 0xA0
#define TITLE_OBJ_GF_SHOOT_BIG1 0xA1
#define TITLE_OBJ_GF_SMALL_STAR 0xA2
#define TITLE_OBJ_GF_BASE       0x80
#define TITLE_OBJ_INTRO_FRONT_BASE 0x00
#define TITLE_PLAYER_COLS       5
#define TITLE_PLAYER_ROWS       7
#define TITLE_PLAYER_OAM_COUNT  (TITLE_PLAYER_COLS * TITLE_PLAYER_ROWS)
#define TITLE_MON_W             7
#define TITLE_MON_H             7
#define TITLE_MON_ROW           10
#define TITLE_MON_TARGET_X      5
#define TITLE_MON_BAND_ROW      9
#define TITLE_MON_BAND_ROWS     8
#define TITLE_BALL_OAM_INDEX    10
#define TITLE_BALL_Y_DEFAULT    0x74

#define TITLE_BALL_STEP_FRAMES  2
#define TITLE_BALL_STEPS        5

#define TITLE_WAIT_BALL_STARTER (TITLE_BALL_STEPS * TITLE_BALL_STEP_FRAMES)

#define PRE_LEGAL_FRAMES         180
#define PRE_GF_SETUP_FRAMES       64
#define PRE_GF_FLASH_HOLD_FRAMES  10
#define PRE_GF_FLASH_COUNT          3
#define PRE_GF_SMALL_MOVE_LOOPS     8
#define PRE_GF_SMALL_MOVE_DELAY     3
#define INTRO_WAIT_10_FRAMES       10
#define INTRO_WAIT_20_FRAMES       21
#define INTRO_WAIT_30_FRAMES       31
#define INTRO_WAIT_60_FRAMES       63
#define INTRO_MOVE_STEP_WAIT        2
#define INTRO_ANIM_STEP_WAIT        5
#define INTRO_FADE_STEP_FRAMES      8

#define INTRO_POST_FADE_FRAMES     10

static void ts_set_gf_obj_palettes(uint8_t obp0, uint8_t obp1);
static void ts_clear_mon_band_scroll(void);
static void ts_reset_ball_pose(void);
static void ts_begin_intro_fade_to_white(void);

static void ts_set(int col, int row, uint8_t tile) {
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void ts_set_window(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH) return;
    if (row < 0 || row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile;
}

static void ts_set_scroll(int col, int row, uint8_t tile) {

    if (col < -2) return;
    if (Display_ContentOriginX() > 0) {

        if (col > SCROLL_MAP_W - 3) return;
    } else if (col > SCREEN_WIDTH + 1) {
        return;
    }
    if (row < -2 || row > SCREEN_HEIGHT + 1) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void ts_clear(void) {
    uint8_t blank = (uint8_t)Font_CharToTile(0x7F);
    for (int i = 0; i < SCROLL_MAP_W * SCROLL_MAP_H; i++)
        gScrollTileMap[i] = blank;
}

static void ts_clear_window(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            gWindowTileMap[y][x] = 0;
        }
    }
}

static void ts_fill_window_row_range(int row_start, uint8_t tile) {
    if (row_start < 0) row_start = 0;
    if (row_start >= SCREEN_HEIGHT) return;
    for (int y = row_start; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            gWindowTileMap[y][x] = tile;
    }
}

static void ts_upload_title_tiles(void) {
    for (int i = 0; i < TITLE_LOGO_TILES; i++)
        Display_LoadTile((uint8_t)(TITLE_BG_LOGO_BASE + i), gTitleLogoTiles[i]);

    for (int i = 0; i < (int)gTitleRedVersionTiles_count; i++)
        Display_LoadTile((uint8_t)(TITLE_BG_VERSION_BASE + i), gTitleRedVersionTiles[i]);
    for (int i = 0; i < TITLE_COPYRIGHT_TILES; i++)
        Display_LoadTile((uint8_t)(TITLE_BG_COPY_BASE + i), gTitleCopyrightTiles[i]);
}

static void ts_upload_player_tiles(void) {
    for (int i = 0; i < TITLE_PLAYER_TILES; i++)
        Display_LoadSpriteTile((uint8_t)(TITLE_OBJ_PLAYER_BASE + i), gTitlePlayerTiles[i]);
}

static void ts_apply_splash_pals(void) {
    static const uint16_t kGrey[4] = {
        0x7FFF,
        0x56B5,
        0x294A,
        0x0000,
    };
    if (!GbcColor_IsEnabled()) return;
    Display_SetPositionAttrMode(0);
    for (int i = 0; i < 8; i++) Display_SetBGColorPalette(i, kGrey);
    Display_SetOBJColorPalette(0, kGrey);
    Display_SetOBJColorPalette(1, kGrey);
    Display_ClearAttrBoxes(0);
    Display_SetColorMode(1);
}

static void ts_upload_splash_tiles(void) {
    static const uint8_t kBlankTile[16] = {0};
    ts_apply_splash_pals();
    static const uint8_t kSolidDarkTile[16] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    for (int i = 0; i < SPLASH_LEGAL_TILES; i++)
        Display_LoadTile((uint8_t)(TITLE_BG_LEGAL_BASE + i), gSplashLegalTiles[i]);
    Display_LoadTile((uint8_t)TITLE_BG_SOLID_DARK, kSolidDarkTile);

    for (int i = 0; i < SPLASH_GAMEFREAK_PRESENTS_TILES; i++)
        Display_LoadSpriteTile((uint8_t)(TITLE_OBJ_GF_BASE + i), gSplashGameFreakPresentsTiles[i]);
    for (int i = 0; i < SPLASH_GAMEFREAK_LOGO_TILES; i++)
        Display_LoadSpriteTile((uint8_t)(TITLE_OBJ_GF_BASE + 13 + i), gSplashGameFreakLogoTiles[i]);
    Display_LoadSpriteTile((uint8_t)(TITLE_OBJ_GF_BASE + 19), kBlankTile);

    Display_LoadSpriteTile((uint8_t)TITLE_OBJ_GF_SHOOT_BIG0, gSplashShootingStarTiles[0]);
    Display_LoadSpriteTile((uint8_t)TITLE_OBJ_GF_SHOOT_BIG1, gSplashShootingStarTiles[1]);
    Display_LoadSpriteTile((uint8_t)TITLE_OBJ_GF_SMALL_STAR, gSplashFallingStarTiles[0]);
}

static void ts_clear_oam(void) {
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
}

static void ts_place_player_oam(void) {
    int idx = 0;
    int y = 0x60;
    for (int row = 0; row < TITLE_PLAYER_ROWS; row++, y += 8) {
        int x = 0x5A;
        for (int col = 0; col < TITLE_PLAYER_COLS; col++, x += 8) {
            if (idx >= MAX_SPRITES) return;
            wShadowOAM[idx].y = (uint8_t)y;
            wShadowOAM[idx].x = (uint8_t)x;
            wShadowOAM[idx].tile = (uint8_t)(TITLE_OBJ_PLAYER_BASE + idx);
            wShadowOAM[idx].flags = 0;
            idx++;
        }
    }
    if (TITLE_BALL_OAM_INDEX < MAX_SPRITES)
        wShadowOAM[TITLE_BALL_OAM_INDEX].y = (uint8_t)gBallY;
}

static void ts_draw_logo_tiles(int col, int row) {
    int tile = TITLE_BG_LOGO_BASE;
    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 16; c++, tile++) {
            int x = col + c;
            int y = row + r;
            if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) continue;
            ts_set(x, y, (uint8_t)tile);
        }
    }
}

static void ts_draw_red_version_tiles(int col, int row) {

    const int n = (int)gTitleVersionTileMap_count;
    int base = -1;
    for (int i = 0; i < n; i++) {
        if (gTitleVersionTileMap[i] != 0x7F) { base = gTitleVersionTileMap[i]; break; }
    }
    if (base < 0) return;
    int8_t kRedVersionMap[16];
    for (int i = 0; i < n && i < 16; i++) {
        kRedVersionMap[i] = (gTitleVersionTileMap[i] == 0x7F)
                          ? (int8_t)-1
                          : (int8_t)(gTitleVersionTileMap[i] - base);
    }
    for (int i = 0; i < n && i < 16; i++) {

        int x = col + i;
        if (x < 0 || x >= Display_AuthoredRightEdgeCol() ||
            row < 0 || row >= SCREEN_HEIGHT)
            continue;
        if (kRedVersionMap[i] < 0) {
            ts_set(x, row, (uint8_t)Font_CharToTile(0x7F));
        } else {
            ts_set(x, row, (uint8_t)(TITLE_BG_VERSION_BASE + kRedVersionMap[i]));
        }
    }
}

static void ts_draw_copyright_tiles(void) {

    static const uint8_t kMap[] = {
        0, 1, 2, 1, 3, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
    };
    const int col = 2;
    const int row = 17;
    for (int i = 0; i < (int)(sizeof(kMap) / sizeof(kMap[0])); i++)
        ts_set(col + i, row, (uint8_t)(TITLE_BG_COPY_BASE + kMap[i]));
}

static void ts_draw_copyright_tiles_window(void) {
    static const uint8_t kMap[] = {
        0, 1, 2, 1, 3, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
    };
    const int col = 2;
    const int row = 17;
    for (int i = 0; i < (int)(sizeof(kMap) / sizeof(kMap[0])); i++)
        ts_set_window(col + i, row, (uint8_t)(TITLE_BG_COPY_BASE + kMap[i]));
}

static void ts_load_title_mon_tiles(uint8_t species) {
    uint8_t dex = Species_Dex(species);
    if (!MonPic_Exists(dex)) dex = 1;

    int nat_w = MonPic_FrontW(dex);
    int nat_h = MonPic_FrontH(dex);
    if (nat_w < 1 || nat_w > 7) nat_w = 7;
    if (nat_h < 1 || nat_h > 7) nat_h = 7;

    int min_c = TITLE_MON_W, max_c = -1;
    int min_r = TITLE_MON_H, max_r = -1;
    for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {
        const uint8_t *st = MonPic_FrontTile(dex, i);
        Display_LoadTile((uint8_t)(TITLE_BG_MON_BASE + i), st);
        int nonzero = 0;
        for (int b = 0; b < 16; b++) {
            if (st[b] != 0) {
                nonzero = 1;
                break;
            }
        }
        if (nonzero) {
            int r = i / TITLE_MON_W;
            int c = i % TITLE_MON_W;
            if (c < min_c) min_c = c;
            if (c > max_c) max_c = c;
            if (r < min_r) min_r = r;
            if (r > max_r) max_r = r;
        }
    }

    if (max_c >= min_c && max_r >= min_r) {
        gMonBBoxL = min_c;
        gMonBBoxR = max_c;
        gMonBBoxT = min_r;
        gMonBBoxB = max_r;
    } else {
        gMonBBoxL = 0;
        gMonBBoxR = TITLE_MON_W - 1;
        gMonBBoxT = 0;
        gMonBBoxB = TITLE_MON_H - 1;
    }

    {
        int asm_xoff = (8 - nat_w) / 2;
        int asm_yoff = (7 - nat_h);
        gMonDrawDx = asm_xoff - gMonBBoxL;
        gMonDrawDy = asm_yoff - gMonBBoxT;
    }
}

static void ts_draw_title_mon_tiles(int col, int row) {
    for (int r = gMonBBoxT; r <= gMonBBoxB; r++) {
        for (int c = gMonBBoxL; c <= gMonBBoxR; c++) {
            int x = col + c + gMonDrawDx;
            int y = row + r + gMonDrawDy;
            ts_set_scroll(x, y, (uint8_t)(TITLE_BG_MON_BASE + r * TITLE_MON_W + c));
        }
    }
}

static void ts_draw_title_mon_tiles_window(int col, int row) {
    for (int r = gMonBBoxT; r <= gMonBBoxB; r++) {
        for (int c = gMonBBoxL; c <= gMonBBoxR; c++) {
            int tx = col + c + gMonDrawDx;
            int ty = row + r + gMonDrawDy;
            if (tx < 0 || tx >= SCREEN_WIDTH || ty < 0 || ty >= SCREEN_HEIGHT)
                continue;
            ts_set_window(tx, ty, (uint8_t)(TITLE_BG_MON_BASE + r * TITLE_MON_W + c));
        }
    }
}

static const uint8_t kTitleScrollIn[]  = { 0xA2, 0x94, 0x84, 0x63, 0x52, 0x31, 0x11, 0x00 };
static const uint8_t kTitleScrollOut[] = { 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x83, 0x93, 0x00 };

static const uint8_t kTitleBallYTable[] = {
    0x00, 0x71, 0x6F, 0x6E, 0x6D, 0x6C, 0x6D, 0x6E, 0x6F, 0x71, 0x74, 0x00
};

enum {
    INTRO_MOVE_NIDORINO_RIGHT = 0,
    INTRO_MOVE_GENGAR_RIGHT,
    INTRO_MOVE_GENGAR_LEFT,
};

static const int8_t kIntroAnim1[] = {  0,  0, -2,  2, -1,  2,  1,  2,  2,  2, 80 };
static const int8_t kIntroAnim2[] = {  0,  0, -2, -2, -1, -2,  1, -2,  2, -2, 80 };
static const int8_t kIntroAnim3[] = {  0,  0, -12, 6, -8, 6,  8,  6, 12,  6, 80 };
static const int8_t kIntroAnim4[] = {  0,  0, -8, -4, -4, -4,  4, -4,  8, -4, 80 };
static const int8_t kIntroAnim5[] = {  0,  0, -8,  4, -4,  4,  4,  4,  8,  4, 80 };
static const int8_t kIntroAnim6[] = {  0,  0,  2,  0,  2,  0,  0,  0, 80 };
static const int8_t kIntroAnim7[] = { -8,-16, -7,-14, -6,-12, -4,-10, 80 };

static void ts_apply_scy(void) {
    gScrollPxX = 0;
    gScrollPxY = -gTitleSCY;
}

static void ts_apply_intro_scx(void) {
    gScrollPxX = -gIntroSCX;
    gScrollPxY = 0;
}

static void ts_upload_intro_tiles(void) {
    static const uint8_t kBlankTile[16] = {0};
    TitleScreen_ApplyNidorinoPalettes();
    Display_LoadTile((uint8_t)TITLE_BG_INTRO_BLANK, kBlankTile);
    for (int i = 0; i < INTRO_GENGAR_BACK_TILES; i++) {
        Display_LoadTile((uint8_t)(TITLE_BG_INTRO_BACK_BASE + i), gIntroGengarBackTiles[i]);
    }
    for (int i = 0; i < INTRO_NIDORINO_FRONT_TILES; i++) {
        Display_LoadSpriteTile((uint8_t)(TITLE_OBJ_INTRO_FRONT_BASE + i), gIntroNidorinoFrontTiles[i]);
    }
}

static void ts_draw_intro_black_bars(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            ts_set(x, y, (uint8_t)TITLE_BG_INTRO_BLANK);
    }

    ts_clear_window();
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        if (y < 4 || y >= 14) {
            for (int x = 0; x < SCREEN_WIDTH; x++)
                ts_set_window(x, y, (uint8_t)TITLE_BG_SOLID_DARK);
        }
    }
    hWY = 0;
    Display_SetWindowOverSprites(1);
}

static void ts_draw_intro_gengar(const uint8_t *tilemap49) {

    const int base_col = 13;
    const int base_row = 7;
    for (int i = 0; i < INTRO_GENGAR_TILEMAP_TILES; i++) {
        int r = i / 7;
        int c = i % 7;
        ts_set(base_col + c, base_row + r, tilemap49[i]);
    }
}

static void ts_intro_set_nidorino_base_tile(int base) {
    gIntroNidorinoBaseTile = base;
    for (int i = 0; i < 36 && i < MAX_SPRITES; i++) {
        wShadowOAM[i].tile = (uint8_t)(TITLE_OBJ_INTRO_FRONT_BASE + gIntroNidorinoBaseTile + i);
    }
}

static void ts_intro_init_nidorino_oam(void) {

    int idx = 0;
    for (int col = 0; col < 6; col++) {
        int x = col * 8;
        int y = 80;
        for (int row = 0; row < 6; row++) {
            y += 8;
            if (idx >= MAX_SPRITES) return;
            wShadowOAM[idx].y = (uint8_t)y;
            wShadowOAM[idx].x = (uint8_t)x;
            wShadowOAM[idx].tile = (uint8_t)(TITLE_OBJ_INTRO_FRONT_BASE + gIntroNidorinoBaseTile + idx);
            wShadowOAM[idx].flags = OAM_FLAG_PRIORITY;
            idx++;
        }
    }
}

static void ts_intro_update_nidorino_delta(int8_t dy, int8_t dx) {
    for (int i = 0; i < 36 && i < MAX_SPRITES; i++) {
        wShadowOAM[i].y = (uint8_t)(wShadowOAM[i].y + dy);
        wShadowOAM[i].x = (uint8_t)(wShadowOAM[i].x + dx);
    }
}

static void ts_intro_start_move(int steps, int move_type) {
    gIntroMoveSteps = steps;
    gIntroMoveType = move_type;
    gIntroTimer = INTRO_MOVE_STEP_WAIT;
}

static int ts_intro_tick_move(void) {
    if (gIntroMoveSteps <= 0) return 1;
    if (--gIntroTimer > 0) return 0;

    if (gIntroMoveType == INTRO_MOVE_NIDORINO_RIGHT) {
        ts_intro_update_nidorino_delta(0, 2);

        gIntroSCX += 2;
    } else if (gIntroMoveType == INTRO_MOVE_GENGAR_LEFT) {
        gIntroSCX += 2;
    } else {
        gIntroSCX -= 2;
    }
    ts_apply_intro_scx();
    gIntroMoveSteps--;
    gIntroTimer = INTRO_MOVE_STEP_WAIT;
    return gIntroMoveSteps <= 0;
}

static void ts_intro_start_anim(const int8_t *anim) {
    gIntroAnim = anim;
    gIntroAnimStep = 0;
    gIntroTimer = INTRO_ANIM_STEP_WAIT;
}

static int ts_intro_tick_anim(void) {
    if (!gIntroAnim) return 1;
    if (--gIntroTimer > 0) return 0;
    if (gIntroAnim[gIntroAnimStep] == 80) return 1;

    int8_t dy = gIntroAnim[gIntroAnimStep++];
    int8_t dx = gIntroAnim[gIntroAnimStep++];
    ts_intro_update_nidorino_delta(dy, dx);
    gIntroTimer = INTRO_ANIM_STEP_WAIT;
    return 0;
}

static void ts_intro_begin(void) {

    Music_Play(MUSIC_INTRO_BATTLE);
    ts_clear();
    ts_clear_window();
    ts_clear_mon_band_scroll();
    ts_clear_oam();
    ts_reset_ball_pose();
    gBounceWindowActive = 0;
    hWY = SCREEN_HEIGHT_PX;
    gIntroSCX = 0;
    ts_apply_intro_scx();
    ts_set_gf_obj_palettes(0xE4, 0xE4);
    ts_upload_intro_tiles();
    ts_draw_intro_black_bars();
    ts_draw_intro_gengar(gIntroGengarTilemap1);
    ts_intro_set_nidorino_base_tile(0);
    ts_intro_init_nidorino_oam();
    gIntroSubstate = 0;
    ts_intro_start_move(80 / 2, INTRO_MOVE_NIDORINO_RIGHT);
}

static int ts_intro_tick(void) {
    switch (gIntroSubstate) {
    case 0:
        if (ts_intro_tick_move()) {
            Audio_PlaySFX_IntroHip();
            ts_intro_set_nidorino_base_tile(0);
            ts_intro_start_anim(kIntroAnim1);
            gIntroSubstate = 1;
        }
        break;
    case 1:
        if (ts_intro_tick_anim()) {
            Audio_PlaySFX_IntroHop();
            ts_intro_start_anim(kIntroAnim2);
            gIntroSubstate = 2;
        }
        break;
    case 2:
        if (ts_intro_tick_anim()) {
            gIntroTimer = INTRO_WAIT_10_FRAMES;
            gIntroSubstate = 3;
        }
        break;
    case 3:
        if (--gIntroTimer <= 0) {
            Audio_PlaySFX_IntroHip();
            ts_intro_start_anim(kIntroAnim1);
            gIntroSubstate = 4;
        }
        break;
    case 4:
        if (ts_intro_tick_anim()) {
            Audio_PlaySFX_IntroHop();
            ts_intro_start_anim(kIntroAnim2);
            gIntroSubstate = 5;
        }
        break;
    case 5:
        if (ts_intro_tick_anim()) {
            gIntroTimer = INTRO_WAIT_30_FRAMES;
            gIntroSubstate = 6;
        }
        break;
    case 6:
        if (--gIntroTimer <= 0) {
            ts_draw_intro_black_bars();
            ts_draw_intro_gengar(gIntroGengarTilemap2);
            Audio_PlaySFX_IntroRaise();
            ts_intro_start_move(8 / 2, INTRO_MOVE_GENGAR_LEFT);
            gIntroSubstate = 7;
        }
        break;
    case 7:
        if (ts_intro_tick_move()) {
            gIntroTimer = INTRO_WAIT_30_FRAMES;
            gIntroSubstate = 8;
        }
        break;
    case 8:
        if (--gIntroTimer <= 0) {
            ts_draw_intro_black_bars();
            ts_draw_intro_gengar(gIntroGengarTilemap3);
            Audio_PlaySFX_IntroCrash();
            ts_intro_start_move(16 / 2, INTRO_MOVE_GENGAR_RIGHT);
            gIntroSubstate = 9;
        }
        break;
    case 9:
        if (ts_intro_tick_move()) {
            Audio_PlaySFX_IntroHip();
            ts_intro_set_nidorino_base_tile(36);
            ts_intro_start_anim(kIntroAnim3);
            gIntroSubstate = 10;
        }
        break;
    case 10:
        if (ts_intro_tick_anim()) {
            gIntroTimer = INTRO_WAIT_30_FRAMES;
            gIntroSubstate = 11;
        }
        break;
    case 11:
        if (--gIntroTimer <= 0) {
            ts_intro_start_move(8 / 2, INTRO_MOVE_GENGAR_LEFT);
            gIntroSubstate = 12;
        }
        break;
    case 12:
        if (ts_intro_tick_move()) {
            ts_draw_intro_black_bars();
            ts_draw_intro_gengar(gIntroGengarTilemap1);
            gIntroTimer = INTRO_WAIT_60_FRAMES;
            gIntroSubstate = 13;
        }
        break;
    case 13:
        if (--gIntroTimer <= 0) {
            Audio_PlaySFX_IntroHip();
            ts_intro_set_nidorino_base_tile(0);
            ts_intro_start_anim(kIntroAnim4);
            gIntroSubstate = 14;
        }
        break;
    case 14:
        if (ts_intro_tick_anim()) {
            Audio_PlaySFX_IntroHop();
            ts_intro_start_anim(kIntroAnim5);
            gIntroSubstate = 15;
        }
        break;
    case 15:
        if (ts_intro_tick_anim()) {
            gIntroTimer = INTRO_WAIT_20_FRAMES;
            gIntroSubstate = 16;
        }
        break;
    case 16:
        if (--gIntroTimer <= 0) {
            ts_intro_set_nidorino_base_tile(36);
            ts_intro_start_anim(kIntroAnim6);
            gIntroSubstate = 17;
        }
        break;
    case 17:
        if (ts_intro_tick_anim()) {
            gIntroTimer = INTRO_WAIT_30_FRAMES;
            gIntroSubstate = 18;
        }
        break;
    case 18:
        if (--gIntroTimer <= 0) {
            Audio_PlaySFX_IntroLunge();
            ts_intro_set_nidorino_base_tile(72);
            ts_intro_start_anim(kIntroAnim7);
            gIntroSubstate = 19;
        }
        break;
    case 19:
        if (ts_intro_tick_anim())
            return 1;
        break;
    }
    return 0;
}

static void ts_begin_intro_fade_to_white(void) {

    static const uint8_t kFadeOutToWhite[3][3] = {
        { 0x90, 0x80, 0x90 },
        { 0x40, 0x40, 0x40 },
        { 0x00, 0x00, 0x00 },
    };
    gState = TS_INTRO_FADE_TO_WHITE;
    gIntroFadeStep = 0;
    Display_SetPalette(kFadeOutToWhite[gIntroFadeStep][0],
                       kFadeOutToWhite[gIntroFadeStep][1],
                       kFadeOutToWhite[gIntroFadeStep][2]);
    gTimer = INTRO_FADE_STEP_FRAMES;
}

static int ts_floor_div8(int v) {
    return (v >= 0) ? (v / 8) : -(((-v) + 7) / 8);
}

static int ts_exit_surplus(void) {
    int ox = Display_ContentOriginX();
    return ox > 0 ? ox + TITLE_MON_W * 8 : 0;
}

static void ts_set_mon_band_scx(uint8_t scx) {

    int total_px = -(int)((int8_t)scx) + gMonScrollExtraPx;
    int coarse = ts_floor_div8(total_px);
    int fine = total_px - coarse * 8;

    if (Display_ContentOriginX() > 0) {
        const int min_col = -2;
        int excess = min_col - (TITLE_MON_TARGET_X + coarse);
        if (excess > 0) { coarse += excess; fine -= excess * 8; }
    }

    gMonTileColOffset = coarse;
    gMonBandPx = fine;
    Display_SetBandXPx(TITLE_MON_BAND_ROW, TITLE_MON_BAND_ROWS, fine);
}

static void ts_clear_mon_band_scroll(void) {
    gMonBandPx = 0;
    gMonTileColOffset = 0;
    Display_SetBandXPx(-1, 0, 0);
}

static void ts_reset_ball_pose(void) {
    gBallY = TITLE_BALL_Y_DEFAULT;
    gBallAnimPos = 0;
    gBallAnimFramesLeft = 0;
    gBallAnimStepHold = 0;
}

static void ts_start_ball_toss_anim(void) {
    gBallAnimPos = 1;
    gBallAnimFramesLeft = 5;
    gBallAnimStepHold = 0;
}

static void ts_tick_ball_toss_anim(void) {
    if (gBallAnimFramesLeft <= 0) return;
    if (gBallAnimStepHold > 0) {
        gBallAnimStepHold--;
        return;
    }

    if (gBallAnimPos > 0 &&
        gBallAnimPos < (int)(sizeof(kTitleBallYTable) / sizeof(kTitleBallYTable[0])) &&
        kTitleBallYTable[gBallAnimPos] != 0x00) {
        gBallY = (int)kTitleBallYTable[gBallAnimPos];
        gBallAnimPos++;
        gBallAnimStepHold = TITLE_BALL_STEP_FRAMES - 1;
    } else {
        gBallAnimFramesLeft = 0;
        return;
    }

    gBallAnimFramesLeft--;
    if (gBallAnimFramesLeft <= 0) {

        gBallY = TITLE_BALL_Y_DEFAULT;
    }
}

static void ts_stage_mon_offscreen_right(void) {

    gMonScrollExtraPx = Display_ContentOriginX();
    gMonScrollLeaving = 0;
    ts_set_mon_band_scx(0x88);
    gMonBandPx = 0;
    Display_SetBandXPx(-1, 0, 0);
}

static void ts_start_mon_scroll_in(void) {
    gMonScrollD = 0x88;

    gMonScrollExtraPx = Display_ContentOriginX();
    gMonScrollLeaving = 0;

    gMonScrollSubdiv  = (Display_ContentOriginX() > 0) ? 2 : 1;
    gMonScrollSubTick = 0;
    gMonScrollAcc     = 0;
    gMonScrollScript = kTitleScrollIn;
    gMonScrollScriptPos = 0;
    gMonScrollPhaseLeft = 0;
    gMonScrollPhaseSpeed = 0;
    ts_set_mon_band_scx(gMonScrollD);
}

static void ts_start_mon_scroll_out(void) {
    gMonScrollD = 0x00;
    gMonScrollExtraPx = 0;
    gMonScrollLeaving = 1;

    gMonScrollSubdiv  = (Display_ContentOriginX() > 0) ? 2 : 1;
    gMonScrollSubTick = 0;
    gMonScrollAcc     = 0;
    gMonScrollScript = kTitleScrollOut;
    gMonScrollScriptPos = 0;
    gMonScrollPhaseLeft = 0;
    gMonScrollPhaseSpeed = 0;
    ts_set_mon_band_scx(gMonScrollD);
}

static int ts_tick_mon_scroll_script(void) {
    if (!gMonScrollScript) return 1;

    if (gMonScrollExtraPx > 0) {
        int div  = gMonScrollSubdiv > 0 ? gMonScrollSubdiv : 1;
        int step = 8 / div;
        if (step < 1) step = 1;
        if (step > gMonScrollExtraPx) step = gMonScrollExtraPx;
        gMonScrollExtraPx -= step;
        ts_set_mon_band_scx(gMonScrollD);
        return 0;
    }

    if (gMonScrollPhaseLeft <= 0) {
        uint8_t cmd = gMonScrollScript[gMonScrollScriptPos++];
        if (cmd == 0x00) {

            if (gMonScrollLeaving && gMonScrollExtraPx > -ts_exit_surplus()) {
                int div  = gMonScrollSubdiv > 0 ? gMonScrollSubdiv : 1;
                int step = (gMonScrollPhaseSpeed > 0 ? gMonScrollPhaseSpeed : 8) / div;
                if (step < 1) step = 1;
                if (gMonScrollExtraPx - step < -ts_exit_surplus())
                    step = gMonScrollExtraPx + ts_exit_surplus();
                gMonScrollExtraPx -= step;
                gMonScrollScriptPos--;
                ts_set_mon_band_scx(gMonScrollD);
                return 0;
            }

            ts_set_mon_band_scx(gMonScrollD);
            return 1;
        }
        gMonScrollPhaseSpeed = (cmd >> 4) & 0x0F;
        gMonScrollPhaseLeft  = cmd & 0x0F;
        if (gMonScrollPhaseLeft <= 0) gMonScrollPhaseLeft = 1;
    }

    ts_set_mon_band_scx(gMonScrollD);

    {
        int div = gMonScrollSubdiv > 0 ? gMonScrollSubdiv : 1;
        int step;
        gMonScrollAcc += gMonScrollPhaseSpeed;
        step = gMonScrollAcc / div;
        gMonScrollAcc -= step * div;
        gMonScrollD = (uint8_t)(gMonScrollD + step);
        if (++gMonScrollSubTick >= div) {
            gMonScrollSubTick = 0;
            gMonScrollPhaseLeft--;
        }
    }
    return 0;
}

static uint8_t ts_legal_map_tile(uint8_t asm_tile) {
    if (asm_tile >= 0x60 && asm_tile <= 0x7B)
        return (uint8_t)(TITLE_BG_LEGAL_BASE + (asm_tile - 0x60));
    if (asm_tile == 0x7F)
        return (uint8_t)Font_CharToTile(0x7F);
    return (uint8_t)Font_CharToTile(0x7F);
}

static void ts_draw_legal_line_asm(int row, const uint8_t *tiles, int count) {
    const int col = 2;
    for (int i = 0; i < count; i++)
        ts_set(col + i, row, ts_legal_map_tile(tiles[i]));
}

static void ts_draw_splash(void) {

    static const uint8_t kLegalLine1[] = {
        0x60,0x61,0x62,0x61,0x63,0x61,0x64,0x7F,0x65,0x66,0x67,0x68,0x69,0x6A
    };

    ts_clear();
    ts_clear_window();
    ts_clear_oam();
    ts_clear_mon_band_scroll();
    ts_reset_ball_pose();
    gBounceWindowActive = 0;
    gTitleSCY = 0;
    ts_apply_scy();
    hWY = SCREEN_HEIGHT_PX;
    ts_draw_legal_line_asm(7, kLegalLine1, (int)(sizeof(kLegalLine1) / sizeof(kLegalLine1[0])));

    ts_draw_legal_line_asm(9, kLegalLine2, (int)kLegalLine2_count);
    ts_draw_legal_line_asm(11, kLegalLine3, (int)kLegalLine3_count);
}

static void ts_place_gamefreak_oam(void) {

    static const struct {
        uint8_t x, y, tile;
    } kGF[] = {
        {10,  9, 0x8D}, {11,  9, 0x8E}, {10, 10, 0x8F}, {11, 10, 0x90},
        {10, 11, 0x91}, {11, 11, 0x92},
        { 6, 12, 0x80}, { 7, 12, 0x81}, { 8, 12, 0x82}, { 9, 12, 0x83},
        {10, 12, 0x93}, {11, 12, 0x84}, {12, 12, 0x85}, {13, 12, 0x83},
        {14, 12, 0x81}, {15, 12, 0x86},
    };
    int idx = 24;
    for (int i = 0; i < (int)(sizeof(kGF) / sizeof(kGF[0])) && idx < MAX_SPRITES; i++, idx++) {

        wShadowOAM[idx].x = (uint8_t)(kGF[i].x * 8);
        wShadowOAM[idx].y = (uint8_t)(kGF[i].y * 8);
        wShadowOAM[idx].tile = (uint8_t)(TITLE_OBJ_GF_BASE + (kGF[i].tile - 0x80));
        wShadowOAM[idx].flags = 0;
    }
}

static void ts_set_gf_obj_palettes(uint8_t obp0, uint8_t obp1) {
    gGFObp0 = obp0;
    gGFObp1 = obp1;
    Display_SetPalette(0xE4, gGFObp0, gGFObp1);
    TitleScreen_ApplyGameFreakPalettes();
}

static void ts_init_big_star_oam(void) {
    fflush(stdout);

    static const struct {
        uint8_t x, y, tile, flags;
    } kBigStar[] = {

        {20 * 8, 0 * 8, TITLE_OBJ_GF_SHOOT_BIG0, TS_STAR_CPAL},
        {21 * 8, 0 * 8, TITLE_OBJ_GF_SHOOT_BIG0, TS_STAR_CPAL | OAM_FLAG_FLIP_X},
        {20 * 8, 1 * 8, TITLE_OBJ_GF_SHOOT_BIG1, TS_STAR_CPAL},
        {21 * 8, 1 * 8, TITLE_OBJ_GF_SHOOT_BIG1, TS_STAR_CPAL | OAM_FLAG_FLIP_X},
    };
    for (int i = 0; i < 4; i++) {
        wShadowOAM[i].x = kBigStar[i].x;
        wShadowOAM[i].y = kBigStar[i].y;
        wShadowOAM[i].tile = kBigStar[i].tile;
        wShadowOAM[i].flags = kBigStar[i].flags;
    }
}

static void ts_clear_big_star_oam(void) {
    for (int i = 0; i < 4; i++)
        wShadowOAM[i].y = (uint8_t)(SCREEN_HEIGHT_PX + OAM_Y_OFS);
}

static void ts_init_small_stars_oam(void) {
    for (int i = 0; i < 24 && i < MAX_SPRITES; i++) {
        wShadowOAM[i].y = 0;
        wShadowOAM[i].x = 0;
        wShadowOAM[i].tile = (uint8_t)TITLE_OBJ_GF_SMALL_STAR;
        wShadowOAM[i].flags = (uint8_t)(OAM_FLAG_PRIORITY | TS_STAR_CPAL);
    }
}

static uint8_t ts_gf_star_pal_for_x(uint8_t oam_x) {
    int col = ((int)oam_x - 8) / 8;
    int region;
    if      (col >= 5  && col <= 7)  region = 1;
    else if (col >= 8  && col <= 9)  region = 2;
    else if (col >= 12 && col <= 14) region = 3;
    else                             region = 0;

    return (uint8_t)(TS_GF_STAR_CPAL_BASE + region);
}

static void ts_inject_small_stars_wave(int wave_idx) {
    static const int16_t kWaves[6][8] = {
        {0x68,0x30, 0x68,0x40, 0x68,0x58, 0x68,0x78},
        {0x68,0x38, 0x68,0x48, 0x68,0x60, 0x68,0x70},
        {0x68,0x34, 0x68,0x4C, 0x68,0x54, 0x68,0x64},
        {0x68,0x3C, 0x68,0x5C, 0x68,0x6C, 0x68,0x74},
        {-1,-1, -1,-1, -1,-1, -1,-1},
        {-1,-1, -1,-1, -1,-1, -1,-1},
    };
    if (wave_idx < 0 || wave_idx >= 6) return;
    if (kWaves[wave_idx][0] != -1) {
        for (int i = 0; i < 4; i++) {
            int y = kWaves[wave_idx][i * 2 + 0];
            int x = kWaves[wave_idx][i * 2 + 1];
            int slot = 20 + i;
            if (slot >= MAX_SPRITES) break;
            wShadowOAM[slot].y = (uint8_t)y;
            wShadowOAM[slot].x = (uint8_t)x;

            if (GbcColor_IsEnabled() && !GbcColor_BattleAutoColor())
                wShadowOAM[slot].flags =
                    (uint8_t)(OAM_FLAG_PRIORITY | OAM_FLAG_PALETTE |
                              ts_gf_star_pal_for_x((uint8_t)x));
        }
        if (gGFSmallStarsMoveCount < 24) {
            gGFSmallStarsMoveCount += 6;
            if (gGFSmallStarsMoveCount > 24) gGFSmallStarsMoveCount = 24;
        }
    }
}

static void ts_move_down_small_stars_step(void) {
    for (int i = 0; i < gGFSmallStarsMoveCount; i++) {
        int slot = 23 - i;
        if (slot < 0 || slot >= MAX_SPRITES) continue;
        wShadowOAM[slot].y++;
    }
    ts_set_gf_obj_palettes(gGFObp0, (uint8_t)(gGFObp1 ^ 0xA0));
}

static void ts_shift_small_stars_fifo(void) {
    if (MAX_SPRITES < 24) return;
    memmove(&wShadowOAM[0], &wShadowOAM[4], sizeof(wShadowOAM[0]) * 20);
}

static void ts_draw_gamefreak_screen(void) {
    ts_clear();
    ts_clear_window();
    ts_clear_mon_band_scroll();
    ts_reset_ball_pose();
    gBounceWindowActive = 0;
    gTitleSCY = 0;
    ts_apply_scy();
    hWY = SCREEN_HEIGHT_PX;

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            ts_set(x, y, (uint8_t)TITLE_BG_SOLID_DARK);
    }
    for (int y = 14; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            ts_set(x, y, (uint8_t)TITLE_BG_SOLID_DARK);
    }
}

static void ts_draw_title_base(void) {

    TitleScreen_ApplyPalettes();

    ts_clear();

    ts_draw_logo_tiles(2, 1);

    if (gBounceWindowActive) {

        ts_clear_window();

        ts_fill_window_row_range(8, (uint8_t)Font_CharToTile(0x7F));
        ts_draw_title_mon_tiles_window(TITLE_MON_TARGET_X, TITLE_MON_ROW);
        ts_draw_copyright_tiles_window();
        hWY = 0x40;
        Display_SetWindowOverSprites(0);
    } else {
        ts_clear_window();
        ts_draw_red_version_tiles(gVersionScrollX, 8);
        ts_draw_title_mon_tiles(TITLE_MON_TARGET_X + gMonTileColOffset, TITLE_MON_ROW);
        ts_draw_copyright_tiles();
        hWY = SCREEN_HEIGHT_PX;
        Display_SetWindowOverSprites(1);
    }
    ts_place_player_oam();
}

static int ts_is_starter(uint8_t species) {
    return species == STARTER1 || species == STARTER2 || species == STARTER3;
}

static int ts_check_interruption(void) {
    if ((hJoyHeld & (PAD_UP | PAD_SELECT | PAD_B)) == (PAD_UP | PAD_SELECT | PAD_B)) {
        gClearSaveCombo = 1;
        return 1;
    }
    if (hJoyPressed & (PAD_START | PAD_A))
        return 1;
    return 0;
}

static void ts_begin_interrupt(void) {
    Audio_PlayCry(gTitleMonSpecies);
    gState = TS_WAIT_CRY;
}

static void ts_pick_new_mon(void) {
    uint8_t prev = gTitleMonSpecies;
    uint8_t pick = prev;
    for (int tries = 0; tries < 32 && pick == prev; tries++) {
        uint8_t r = (uint8_t)(hRandomAdd ^ hRandomSub ^ hFrameCounter ^ (uint8_t)tries);
        pick = kTitleMons[r % TITLE_MON_COUNT];
    }
    gTitleMonSpecies = pick;
    ts_load_title_mon_tiles(gTitleMonSpecies);
}

static void ts_start_title_logo_bounce(void) {

    Font_Load();
    Display_SetPalette(0xE4, 0xE4, 0xE4);
    TitleScreen_ApplyPalettes();
    ts_upload_title_tiles();
    ts_upload_player_tiles();
    ts_load_title_mon_tiles(gTitleMonSpecies);
    gState = TS_BOUNCE;
    gBouncePhase = 0;
    gBounceLeft = kBounceCount[0];
    gLogoPxY = 0;
    gTitleSCY = 0x40;
    gVersionScrollX = Display_AuthoredRightEdgeCol();
    gBounceWindowActive = 1;
    ts_clear_oam();
    ts_clear_mon_band_scroll();
    ts_apply_scy();
    ts_draw_title_base();
}

typedef struct { int c, r, w, h, p; } ts_attr_blk_t;

static void ts_set_obj_pal_permuted(int slot, const uint16_t *base, uint8_t obp) {
    uint16_t out[4];
    if (!base) return;
    for (int i = 0; i < 4; i++) out[i] = base[(obp >> (2 * i)) & 3];
    Display_SetOBJColorPalette(slot, out);
}

static void ts_permute_obj_pal_in_place(int slot, uint8_t obp) {
    uint16_t base[4];
    for (int i = 0; i < 4; i++) base[i] = Display_GetOBJColorEntry(slot, i);
    ts_set_obj_pal_permuted(slot, base, obp);
}

static void ts_apply_sgb_pals(const uint8_t pal[4],
                              const ts_attr_blk_t *blk, int nblk,
                              uint8_t obj0_pal, uint8_t obj1_pal,
                              uint8_t autocolor_obj1_pal) {
    fflush(stdout);
    if (!GbcColor_IsEnabled()) return;
    if (GbcColor_BattleAutoColor()) {
        GbcColor_ApplyAutoColorAll();

        if (autocolor_obj1_pal != 0xFFu)
            GbcColor_AutoColorObjPal(1, gGFObp1);
    } else {
        for (int i = 0; i < 4; i++) {
            const uint16_t *rgb = GbcColor_SuperPalette(pal[i]);
            if (rgb) Display_SetBGColorPalette(i, rgb);
        }
        Display_SetPositionAttrMode(1);
        Display_ClearAttrBoxes(0);
        for (int i = 0; i < nblk; i++)
            Display_FillAttrBox(blk[i].c, blk[i].r, blk[i].w, blk[i].h,
                                (uint8_t)blk[i].p);
        if (obj0_pal != 0xFFu) {
            const uint16_t *rgb = GbcColor_SuperPalette(obj0_pal);
            if (rgb) Display_SetOBJColorPalette(0, rgb);
        }
        if (obj1_pal != 0xFFu) {
            const uint16_t *rgb = GbcColor_SuperPalette(obj1_pal);
            if (rgb) Display_SetOBJColorPalette(1, rgb);
        }
        Display_SetColorMode(1);
    }
}

void TitleScreen_ApplyPalettes(void) {
    static const uint8_t pal[4] = {
        RSGB_PAL_LOGO2, RSGB_PAL_LOGO1, RSGB_PAL_MEWMON, RSGB_PAL_PURPLEMON
    };
    static const ts_attr_blk_t blk[] = {
        { 0,  0, 20,  8, 0 },
        { 0,  8, 20,  2, 1 },
        { 0, 10, 20,  8, 2 },
    };

    ts_apply_sgb_pals(pal, blk, (int)(sizeof blk / sizeof blk[0]),
                      RSGB_PAL_MEWMON, RSGB_PAL_MEWMON,
                      0xFFu);
}

void TitleScreen_ApplyGameFreakPalettes(void) {
    static const uint8_t pal[4] = {
        RSGB_PAL_GAMEFREAK, RSGB_PAL_REDMON, RSGB_PAL_VIRIDIAN, RSGB_PAL_BLUEMON
    };
    static const ts_attr_blk_t blk[] = {
        {  5, 11, 3, 3, 1 },
        {  8, 11, 2, 3, 2 },
        { 12, 11, 3, 3, 3 },
    };

    ts_apply_sgb_pals(pal, blk, (int)(sizeof blk / sizeof blk[0]),

                       RSGB_PAL_GAMEFREAK,
                       RSGB_PAL_REDMON,
                       RSGB_PAL_REDMON);

    if (!GbcColor_IsEnabled()) return;

    if (GbcColor_BattleAutoColor()) {

        ts_permute_obj_pal_in_place(0, gGFObp0);
        return;
    }

    ts_set_obj_pal_permuted(0, GbcColor_SuperPalette(RSGB_PAL_GAMEFREAK), gGFObp0);
    for (int i = 0; i < 4; i++)
        ts_set_obj_pal_permuted(TS_GF_STAR_CPAL_BASE + i,
                                GbcColor_SuperPalette(pal[i]), gGFObp1);
}

void TitleScreen_ApplyNidorinoPalettes(void) {
    static const uint8_t pal[4] = {
        RSGB_PAL_PURPLEMON, RSGB_PAL_BLACK, 0, 0
    };
    static const ts_attr_blk_t blk[] = {
        { 0,  0, 20,  4, 1 },
        { 0,  4, 20, 10, 0 },
        { 0, 14, 20,  4, 1 },
    };

    ts_apply_sgb_pals(pal, blk, (int)(sizeof blk / sizeof blk[0]),
                      RSGB_PAL_PURPLEMON, RSGB_PAL_PURPLEMON,
                      0xFFu);
}

void TitleScreen_Open(void) {

    Audio_UseTitleScreenBank();

    gOpen = 1;
    gResult = TITLE_SCREEN_PENDING;
    gState = TS_SPLASH;
    gTimer = PRE_LEGAL_FRAMES;
    gLogoPxY = 0;
    gBouncePhase = 0;
    gBounceLeft = kBounceCount[0];
    gTitleSCY = 0;
    gVersionScrollX = Display_AuthoredRightEdgeCol();

    gTitleMonSpecies = (gTitleMons_count > 0) ? gTitleMons[0] : STARTER1;
    gClearSaveCombo = 0;
    gMonBandPx = 0;
    gMonTileColOffset = 0;
    gMonScrollD = 0;
    gMonScrollScript = NULL;
    gMonScrollScriptPos = 0;
    gMonScrollPhaseLeft = 0;
    gMonScrollPhaseSpeed = 0;
    gMonJustScrolledOut = STARTER1;
    gBounceWindowActive = 0;
    gGFObp0 = 0xE4;
    gGFObp1 = 0xE4;
    gGFFlashCyclesDone = 0;
    gGFSmallStarsWave = 0;
    gGFSmallStarsMoveIter = 0;
    gGFSmallStarsMoveCount = 0;
    gIntroSubstate = 0;
    gIntroTimer = 0;
    gIntroMoveType = 0;
    gIntroMoveSteps = 0;
    gIntroAnimStep = 0;
    gIntroAnim = NULL;
    gIntroNidorinoBaseTile = 0;
    gIntroSCX = 0;
    gIntroFadeStep = 0;
    ts_reset_ball_pose();
    ts_upload_player_tiles();
    ts_upload_splash_tiles();

    ts_draw_splash();
}

void TitleScreen_OpenAtTitle(void) {
    TitleScreen_Open();
    ts_start_title_logo_bounce();
}

int TitleScreen_IsOpen(void) { return gOpen; }
int TitleScreen_GetResult(void) { return gResult; }

void TitleScreen_Tick(void) {
    if (!gOpen) return;

    {
        int row0 = 0, rows = 0;
        if (gState == TS_VERSION_SCROLL) {
            row0 = 8; rows = 1;
        } else if (gState == TS_SCROLL_OUT_MON || gState == TS_SCROLL_IN_MON) {
            row0 = TITLE_MON_BAND_ROW; rows = TITLE_MON_BAND_ROWS;
        }
        Display_SetAuthoredBleedRows(row0, rows);
    }

    if (gState != TS_WAIT_CRY && ts_check_interruption()) {
        if (gState == TS_GAMEFREAK_STAR_FALL || gState == TS_GAMEFREAK_FLASH ||
            gState == TS_GAMEFREAK_SMALL_STARS) {
            ts_set_gf_obj_palettes(0xE4, 0xE4);
            gState = TS_INTRO_CUTSCENE;
            ts_intro_begin();
        } else if (gState == TS_INTRO_CUTSCENE) {
            ts_begin_intro_fade_to_white();
        } else if (gState >= TS_BOUNCE) {
            ts_begin_interrupt();
        }
    }

    switch (gState) {
    case TS_SPLASH:
        if (--gTimer <= 0) {
            gState = TS_GAMEFREAK_SETUP;
            gTimer = PRE_GF_SETUP_FRAMES;
            ts_draw_gamefreak_screen();
            ts_clear_oam();
            ts_place_gamefreak_oam();
            ts_set_gf_obj_palettes(0xF9, 0xA4);
        }
        break;

    case TS_GAMEFREAK_SETUP:
        if (--gTimer <= 0) {
            gState = TS_GAMEFREAK_STAR_FALL;

            Audio_PlaySFX_ShootingStar();

            ts_set_gf_obj_palettes(0xF9, 0xA4);
            ts_init_big_star_oam();
        }
        break;

    case TS_GAMEFREAK_STAR_FALL:
        for (int i = 0; i < 4; i++) {
            wShadowOAM[i].y = (uint8_t)(wShadowOAM[i].y + 4);
            wShadowOAM[i].x = (uint8_t)(wShadowOAM[i].x - 4);
        }
        {
            uint8_t y0 = wShadowOAM[0].y;
            if (y0 != 0x50 && y0 == 0xA0) {
                ts_clear_big_star_oam();
                gGFFlashCyclesDone = 1;
                gTimer = PRE_GF_FLASH_HOLD_FRAMES;
                ts_set_gf_obj_palettes((uint8_t)((gGFObp0 >> 2) | (gGFObp0 << 6)), gGFObp1);
                gState = TS_GAMEFREAK_FLASH;
            }
        }
        break;

    case TS_GAMEFREAK_FLASH:
        if (--gTimer <= 0) {
            if (gGFFlashCyclesDone >= PRE_GF_FLASH_COUNT) {
                gState = TS_GAMEFREAK_SMALL_STARS;
                gGFSmallStarsWave = 0;
                gGFSmallStarsMoveIter = 0;
                gGFSmallStarsMoveCount = 0;
                ts_init_small_stars_oam();
                ts_inject_small_stars_wave(gGFSmallStarsWave);
                gTimer = PRE_GF_SMALL_MOVE_DELAY;
            } else {
                gGFFlashCyclesDone++;
                gTimer = PRE_GF_FLASH_HOLD_FRAMES;
                ts_set_gf_obj_palettes((uint8_t)((gGFObp0 >> 2) | (gGFObp0 << 6)), gGFObp1);
            }
        }
        break;

    case TS_GAMEFREAK_SMALL_STARS:
        if (--gTimer <= 0) {
            ts_move_down_small_stars_step();
            gGFSmallStarsMoveIter++;
            if (gGFSmallStarsMoveIter >= PRE_GF_SMALL_MOVE_LOOPS) {
                ts_shift_small_stars_fifo();
                gGFSmallStarsWave++;
                if (gGFSmallStarsWave >= 6) {
                    ts_set_gf_obj_palettes(0xE4, 0xE4);
                    gState = TS_INTRO_CUTSCENE;
                    ts_intro_begin();
                    break;
                }
                ts_inject_small_stars_wave(gGFSmallStarsWave);
                gGFSmallStarsMoveIter = 0;
            }
            gTimer = PRE_GF_SMALL_MOVE_DELAY;
        }
        break;

    case TS_INTRO_CUTSCENE:
        if (ts_intro_tick())
            ts_begin_intro_fade_to_white();
        break;

    case TS_INTRO_FADE_TO_WHITE:
    {
        static const uint8_t kFadeOutToWhite[3][3] = {
            { 0x90, 0x80, 0x90 },
            { 0x40, 0x40, 0x40 },
            { 0x00, 0x00, 0x00 },
        };
        if (--gTimer <= 0) {
            gIntroFadeStep++;
            if (gIntroFadeStep >= 3) {

                gIntroSCX = 0;
                ts_apply_intro_scx();
                ts_clear_oam();
                gState = TS_INTRO_POST_FADE;
                gTimer = INTRO_POST_FADE_FRAMES;
            } else {
                Display_SetPalette(kFadeOutToWhite[gIntroFadeStep][0],
                                   kFadeOutToWhite[gIntroFadeStep][1],
                                   kFadeOutToWhite[gIntroFadeStep][2]);
                gTimer = INTRO_FADE_STEP_FRAMES;
            }
        }
        break;
    }

    case TS_INTRO_POST_FADE:
        if (--gTimer <= 0)
            ts_start_title_logo_bounce();
        break;

    case TS_BOUNCE:

        ts_upload_title_tiles();
        gTitleSCY += kBounceDy[gBouncePhase];
        ts_apply_scy();
        ts_draw_title_base();
        if (--gBounceLeft <= 0) {
            gBouncePhase++;
            if (gBouncePhase >= BOUNCE_PHASE_COUNT) {

                gState = TS_BOUNCE_HOLD;
                gTimer = TITLE_BOUNCE_HOLD_TICKS;
                gTitleSCY = 0;
                gBounceWindowActive = 0;
                ts_apply_scy();
            } else {
                gBounceLeft = kBounceCount[gBouncePhase];

                if (kBounceDy[gBouncePhase] == -3) {
                    DBG_PRINTF("[TSFX] crash: phase=%d dy=%d scy=%d\n",
                           gBouncePhase, kBounceDy[gBouncePhase], gTitleSCY);
                    fflush(stdout);
                    Audio_PlaySFX_IntroCrash();
                    DBG_PRINTF("[TSFX] crash -> accepted=%d\n", Audio_IsSFXPlaying());
                    fflush(stdout);
                }
            }
        }
        break;

    case TS_BOUNCE_HOLD:
        ts_draw_title_base();
        if (--gTimer <= 0) {

            Audio_PlaySFX_IntroWhoosh();
            gState = TS_VERSION_SCROLL;
            gTimer = 0;
            gVersionScrollX = Display_AuthoredRightEdgeCol();
        }
        break;

    case TS_VERSION_SCROLL:
        if ((gTimer++ & 1) == 0 && gVersionScrollX > 7)
            gVersionScrollX--;
        ts_draw_title_base();
        if (gVersionScrollX <= 7) {

            if (Audio_IsSFXPlaying())
                break;

            Music_Stop();
            Music_Play(MUSIC_TITLE);
            gState = TS_LOOP_WAIT;
            gTimer = TITLE_WAIT_CHECK_200_FRAMES;
            ts_clear_mon_band_scroll();
        }
        break;

    case TS_LOOP_WAIT:
        if (--gTimer <= 0) {

            gMonJustScrolledOut = gTitleMonSpecies;
            ts_start_mon_scroll_out();
            gState = TS_SCROLL_OUT_MON;
        }
        ts_draw_title_base();
        break;

    case TS_SCROLL_OUT_MON:
    {
        int done = ts_tick_mon_scroll_script();
        ts_draw_title_base();
        if (done) {
            ts_clear_mon_band_scroll();

            gState = TS_WAIT_BALL;
            gTimer = TITLE_WAIT_CHECK_1_FRAME;
            if (ts_is_starter(gMonJustScrolledOut)) {
                gTimer += TITLE_WAIT_BALL_STARTER;
                ts_start_ball_toss_anim();
            } else {
                ts_reset_ball_pose();
            }

            ts_pick_new_mon();
            ts_stage_mon_offscreen_right();

            ts_draw_title_base();
        }
        break;
    }

    case TS_WAIT_BALL:
        ts_tick_ball_toss_anim();
        ts_draw_title_base();
        if (--gTimer <= 0) {
            ts_start_mon_scroll_in();
            gState = TS_SCROLL_IN_MON;
        }
        break;

    case TS_SCROLL_IN_MON:
    {
        int done = ts_tick_mon_scroll_script();
        ts_draw_title_base();
        if (done) {
            ts_clear_mon_band_scroll();
            gState = TS_LOOP_WAIT;
            gTimer = TITLE_WAIT_CHECK_200_FRAMES;
        }
        break;
    }

    case TS_WAIT_CRY:
        ts_draw_title_base();
        if (!Audio_IsCryPlaying()) {
            ts_clear_mon_band_scroll();
            ts_clear_oam();
            gTitleSCY = 0;
            ts_apply_scy();

            gBounceWindowActive = 0;
            ts_clear_window();
            hWY = SCREEN_HEIGHT_PX;
            gOpen = 0;
            gResult = gClearSaveCombo ? TITLE_SCREEN_CLEAR_SAVE : TITLE_SCREEN_MAIN_MENU;
            Display_SetWindowOverSprites(1);
        }
        break;
    }
}
