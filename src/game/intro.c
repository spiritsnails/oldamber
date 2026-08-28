
#include "intro.h"
#include "text.h"
#include "../data/font_data.h"
#include "rom_text.h"
#include "constants.h"
#include "inventory.h"
#include "pokemon.h"
#include "music.h"
#include "naming_screen.h"
#include "amberscript_core.h"
#include "amberscript_mapbank.h"
#include "overworld.h"
#include "player.h"
#include "gbc_color.h"
#include "../data/gbc_palettes.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/event_constants.h"
#include "assetpack_bind.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern uint8_t gScrollTileMap[];

#define MAP_REDS_HOUSE_2F  0x26
#define BEDROOM_X           3
#define BEDROOM_Y           6

static const uint8_t kNameAsh[]  = {0x80,0x92,0x87, 0x50};
static const uint8_t kNameGary[] = {0x86,0x80,0x91,0x98, 0x50};

#define NUM_PRESET_NAMES 3

static uint8_t     s_preset_player[NUM_PRESET_NAMES][5];
static uint8_t     s_preset_rival[NUM_PRESET_NAMES][5];
static char        s_preset_player_ascii[NUM_PRESET_NAMES][8];
static char        s_preset_rival_ascii[NUM_PRESET_NAMES][8];
static const char *s_preset_player_p[NUM_PRESET_NAMES];
static const char *s_preset_rival_p[NUM_PRESET_NAMES];
static int         s_presets_ready = 0;

static void preset_decode(const uint8_t *blob, uint32_t len,
                          uint8_t raw[NUM_PRESET_NAMES][5],
                          char ascii[NUM_PRESET_NAMES][8],
                          const char *out_p[NUM_PRESET_NAMES]) {
    uint32_t i = 0;
    int entry = -1;
    while (i < len && entry < NUM_PRESET_NAMES) {
        int n = 0;
        uint8_t buf[8];
        while (i < len && blob[i] != 0x50 && n < 7) buf[n++] = blob[i++];
        while (i < len && blob[i] != 0x50) i++;
        if (i < len) i++;
        if (entry >= 0) {
            for (int k = 0; k < 5; k++) raw[entry][k] = 0x50;
            for (int k = 0; k < n && k < 5; k++) raw[entry][k] = buf[k];
            int a = 0;
            for (int k = 0; k < n && a < 7; k++) {
                uint8_t c = buf[k];
                ascii[entry][a++] = (c >= 0x80 && c <= 0x99) ? (char)('A' + c - 0x80)
                                  : (c == 0x7F) ? ' ' : '?';
            }
            ascii[entry][a] = '\0';
            out_p[entry] = ascii[entry];
        }
        entry++;
    }
}

static void presets_init(void) {
    if (s_presets_ready) return;
    s_presets_ready = 1;
    preset_decode(gDefaultNamesPlayer, gDefaultNamesPlayer_count,
                  s_preset_player, s_preset_player_ascii, s_preset_player_p);
    preset_decode(gDefaultNamesRival, gDefaultNamesRival_count,
                  s_preset_rival, s_preset_rival_ascii, s_preset_rival_p);
}

#define kPresetNamesPlayer      (presets_init(), s_preset_player)
#define kPresetNamesRival       (presets_init(), s_preset_rival)
#define kPresetNamesPlayerASCII (presets_init(), s_preset_player_p)
#define kPresetNamesRivalASCII  (presets_init(), s_preset_rival_p)

#define NIDORINO_DEX 33
#define NIDORINA_DEX 30

static void play_nidorina_cry(void) {
    Audio_PlayCry(gDexToSpecies[NIDORINA_DEX]);
}

static const uint8_t kFadeOutToWhite[3][3] = {
    {0x90, 0x80, 0x90}, {0x40, 0x40, 0x40}, {0x00, 0x00, 0x00},
};
static const uint8_t kFadeInFromWhite[3][3] = {
    {0x40, 0x40, 0x40}, {0x90, 0x80, 0x90}, {0xE4, 0xD0, 0xE0},
};

static const uint8_t kIntroPicFadeIn[6] = {
    0x54, 0xA8, 0xFC, 0xF8, 0xF4, 0xE4,

};

typedef enum {
    IS_IDLE = 0,
    IS_OAK_PIC_DRAW,
    IS_OAK_PIC_FADEIN,
    IS_SPEECH1,
    IS_FADE_OUT_1,
    IS_FADING_OUT,
    IS_NIDORINO_DRAW,
    IS_NIDORINO_SLIDE,
    IS_SPEECH2A,
    IS_SPEECH2B,
    IS_FADE_OUT_2,
    IS_RED_PIC1_DRAW,
    IS_RED_PIC1_SLIDE,
    IS_NAME_PROMPT,
    IS_NAME_SLIDE_RIGHT,
    IS_NAME_MENU,
    IS_NAME_SLIDE_BACK,
    IS_NAME_ENTRY,
    IS_NAME_CONFIRM,
    IS_FADE_OUT_3,
    IS_RIVAL_PIC_DRAW,
    IS_RIVAL_PIC_FADEIN,
    IS_RIVAL_PROMPT,
    IS_RIVAL_SLIDE_RIGHT,
    IS_RIVAL_MENU,
    IS_RIVAL_SLIDE_BACK,
    IS_RIVAL_ENTRY,
    IS_RIVAL_CONFIRM,
    IS_FADE_OUT_4,
    IS_RED_PIC2_DRAW,
    IS_RED_PIC2_FADEIN,
    IS_SPEECH3,
    IS_SHRINK_SFX,
    IS_SHRINK1_DRAW,
    IS_SHRINK2_DRAW,
    IS_MUSIC_STOP,
    IS_SETTLE,
    IS_FINAL_FADE_OUT,
    IS_POST_OAK_HOLD1,
    IS_POST_OAK_HOLD2,
    IS_BIND_MAP,
    IS_DONE,
} IntroState;

static IntroState gState   = IS_IDLE;
static int        gActive  = 0;
static int        gWaiting = 0;
static int        gNameOpened = 0;
static int        gRivalOpened = 0;
static int        gFadeStep  = 0;
static int        gFadeTimer = 0;
static int        gDelay     = 0;
static int        gSlidePx   = 0;
static int        gMenuCursor = 0;
static int        gMenuDrawn  = 0;
static IntroState gAfterWait = IS_IDLE;

#define PIC_TILE_BASE 25
#define PIC_COL 6
#define PIC_ROW 4

static void draw_pic_at_col(const uint8_t (*pic)[16], int col) {
    for (int i = 0; i < 49; i++)
        Display_LoadTile((uint8_t)(PIC_TILE_BASE + i), pic[i]);
    for (int ty = 0; ty < 7; ty++)
        for (int tx = 0; tx < 7; tx++)
            gScrollTileMap[(PIC_ROW + ty + 2) * SCROLL_MAP_W + (col + tx + 2) + Map_UiColOfs()] =
                (uint8_t)(PIC_TILE_BASE + ty * 7 + tx);
}

static void draw_pic_centered(const uint8_t (*pic)[16]) {
    draw_pic_at_col(pic, PIC_COL);
}

static uint8_t intro_reverse_bits(uint8_t v) {
    v = (uint8_t)(((v & 0xF0u) >> 4) | ((v & 0x0Fu) << 4));
    v = (uint8_t)(((v & 0xCCu) >> 2) | ((v & 0x33u) << 2));
    v = (uint8_t)(((v & 0xAAu) >> 1) | ((v & 0x55u) << 1));
    return v;
}

static void draw_nidorino_pic_flipped(void) {
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int src = ty * 7 + (6 - tx);
            uint8_t tile[16];
            for (int row = 0; row < 8; row++) {
                tile[row * 2 + 0] = intro_reverse_bits(gPokemonFrontSprite[NIDORINO_DEX][src][row * 2 + 0]);
                tile[row * 2 + 1] = intro_reverse_bits(gPokemonFrontSprite[NIDORINO_DEX][src][row * 2 + 1]);
            }
            uint8_t tid = (uint8_t)(PIC_TILE_BASE + ty * 7 + tx);
            Display_LoadTile(tid, tile);
            gScrollTileMap[(PIC_ROW + ty + 2) * SCROLL_MAP_W + (PIC_COL + tx + 2) + Map_UiColOfs()] = tid;
        }
    }
}

static void clear_screen(void) {

    memset(gScrollTileMap, BLANK_TILE_SLOT, (size_t)SCROLL_MAP_W * SCROLL_MAP_H);
}

#define NAME_MENU_COL   0
#define NAME_MENU_ROW   0
#define NAME_MENU_W    11
#define NAME_MENU_H    12
#define NAME_MENU_ITEMS (1 + NUM_PRESET_NAMES)

static void put_tile(int col, int row, uint8_t tile) {
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static uint8_t ascii_to_pokechar(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0x80 + (c - 'A'));
    if (c == ' ') return 0x7F;
    return 0x7F;
}

static void put_ascii_str(int col, int row, const char *s) {
    for (int i = 0; s[i]; i++)
        put_tile(col + i, row, (uint8_t)Font_CharToTile(ascii_to_pokechar(s[i])));
}

#define NAME_MENU_CURSOR_CHAR 0xED
static void put_cursor(int col, int row, int visible) {
    put_tile(col, row, (uint8_t)Font_CharToTile(visible ? NAME_MENU_CURSOR_CHAR : 0x7F));
}

static void draw_menu_box(int col, int row, int w, int h) {
    put_tile(col,         row,         (uint8_t)Font_CharToTile(0x79));
    for (int c = 1; c < w - 1; c++)
        put_tile(col + c, row,         (uint8_t)Font_CharToTile(0x7A));
    put_tile(col + w - 1, row,         (uint8_t)Font_CharToTile(0x7B));
    for (int r = 1; r < h - 1; r++) {
        put_tile(col,         row + r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = 1; c < w - 1; c++)
            put_tile(col + c, row + r, (uint8_t)Font_CharToTile(0x7F));
        put_tile(col + w - 1, row + r, (uint8_t)Font_CharToTile(0x7C));
    }
    put_tile(col,             row + h - 1, (uint8_t)Font_CharToTile(0x7D));
    for (int c = 1; c < w - 1; c++)
        put_tile(col + c,     row + h - 1, (uint8_t)Font_CharToTile(0x7A));
    put_tile(col + w - 1,     row + h - 1, (uint8_t)Font_CharToTile(0x7E));
}

#define NAME_MENU_LIST_ROW (NAME_MENU_ROW + 2)
#define NAME_MENU_LIST_COL (NAME_MENU_COL + 2)

#define NAME_MENU_ROW_SPACING 2

static void draw_name_menu(const char *const *names) {
    draw_menu_box(NAME_MENU_COL, NAME_MENU_ROW, NAME_MENU_W, NAME_MENU_H);
    put_ascii_str(NAME_MENU_COL + 3, NAME_MENU_ROW, "NAME");
    put_ascii_str(NAME_MENU_LIST_COL, NAME_MENU_LIST_ROW, "NEW NAME");
    for (int i = 0; i < NUM_PRESET_NAMES; i++)
        put_ascii_str(NAME_MENU_LIST_COL, NAME_MENU_LIST_ROW + (i + 1) * NAME_MENU_ROW_SPACING, names[i]);
    gMenuCursor = 0;
    put_cursor(NAME_MENU_LIST_COL - 1, NAME_MENU_LIST_ROW, 1);
}

static void update_menu_cursor(int old_row, int new_row) {
    put_cursor(NAME_MENU_LIST_COL - 1, NAME_MENU_LIST_ROW + old_row * NAME_MENU_ROW_SPACING, 0);
    put_cursor(NAME_MENU_LIST_COL - 1, NAME_MENU_LIST_ROW + new_row * NAME_MENU_ROW_SPACING, 1);
}

static void init_player_data(void) {

    extern void WRAMClear(void);
    uint8_t saved_options = wOptions;
    WRAMClear();
    wOptions = saved_options;

    AmberScript_MapBank_ResetAll();

    wPlayerID = (uint16_t)(rand() & 0xFFFF);

    memset(wPlayerName, 0x50, NAME_LENGTH);
    memset(wRivalName,  0x50, NAME_LENGTH);
    memcpy(wPlayerName, kNameAsh,  sizeof(kNameAsh));
    memcpy(wRivalName,  kNameGary, sizeof(kNameGary));

    wPartyCount    = 0;
    wNumBagItems   = 0;
    wBagItems[0]   = 0xFF;
    wNumBoxItems   = 0;
    wBoxItems[0]   = 0xFF;

    wPlayerMoney[0] = 0x00;
    wPlayerMoney[1] = 0x30;
    wPlayerMoney[2] = 0x00;

    Inventory_AddTo(&wNumBoxItems, wBoxItems, PC_ITEM_CAPACITY, ITEM_POTION, 1);

    wObtainedBadges = 0;

    printf("[intro] Player data initialized: ASH / GARY, \xA5""3000, 1 POTION\n");
}

static void begin_fade_out_white(IntroState resume) {
    gFadeStep = 0;
    gFadeTimer = 8;
    Display_SetPalette(kFadeOutToWhite[0][0], kFadeOutToWhite[0][1], kFadeOutToWhite[0][2]);
    gState = IS_FADING_OUT;
    gAfterWait = resume;
}

static int pump_fade(const uint8_t table[][3], int steps) {

    if (--gFadeTimer > 0) return 0;
    gFadeStep++;
    if (gFadeStep < steps) {
        Display_SetPalette(table[gFadeStep][0], table[gFadeStep][1], table[gFadeStep][2]);
        gFadeTimer = 8;
        return 0;
    }
    return 1;
}

static int pump_intro_pic_fadein(void) {

    if (--gFadeTimer > 0) return 0;
    gFadeStep++;
    if (gFadeStep < 6) {
        Display_SetBGP(kIntroPicFadeIn[gFadeStep]);
        gFadeTimer = 10;
        return 0;
    }
    return 1;
}

static void begin_intro_pic_fadein(void) {
    gFadeStep = 0;
    gFadeTimer = 10;
    Display_SetBGP(kIntroPicFadeIn[0]);
}

#define SLIDE_PIC_ROWS 7
#define SLIDE_START_PX 112
static void begin_pic_slide(void) {
    Display_SetBandXPx(PIC_ROW, SLIDE_PIC_ROWS, SLIDE_START_PX);
}

static int pump_pic_slide(int *px) {
    *px -= 8;
    if (*px <= 0) {
        Display_SetBandXPx(-1, 0, 0);
        return 1;
    }
    Display_SetBandXPx(PIC_ROW, SLIDE_PIC_ROWS, *px);
    return 0;
}

#define NAME_SLIDE_PX (6 * 8)
#define NAME_SLIDE_HOLD 3
#define PIC_SLID_COL (PIC_COL + 6)

static int pump_pic_slide_out(int *px, int *hold) {
    if (*hold > 0) { (*hold)--; return 0; }
    if (*px >= NAME_SLIDE_PX) {
        Display_SetBandXPx(-1, 0, 0);
        return 1;
    }
    *px += 8;
    Display_SetBandXPx(PIC_ROW, SLIDE_PIC_ROWS, *px);
    *hold = NAME_SLIDE_HOLD;
    return 0;
}

static int pump_pic_slide_back(int *px, int *hold) {
    if (*hold > 0) { (*hold)--; return 0; }
    if (*px <= -NAME_SLIDE_PX) {
        Display_SetBandXPx(-1, 0, 0);
        return 1;
    }
    *px -= 8;
    Display_SetBandXPx(PIC_ROW, SLIDE_PIC_ROWS, *px);
    *hold = NAME_SLIDE_HOLD;
    return 0;
}

static void clear_name_menu_box(void) {
    for (int r = 0; r < NAME_MENU_H; r++)
        for (int c = 0; c < NAME_MENU_W; c++)
            put_tile(NAME_MENU_COL + c, NAME_MENU_ROW + r, BLANK_TILE_SLOT);
}

static void finish_pic_slide_out(const uint8_t (*pic)[16]) {
    draw_pic_at_col(pic, PIC_SLID_COL);
    for (int r = 0; r < SLIDE_PIC_ROWS; r++)
        for (int c = 0; c < (PIC_SLID_COL - PIC_COL); c++)
            put_tile(PIC_COL + c, PIC_ROW + r, BLANK_TILE_SLOT);
}

static void intro_apply_palettes(void) {
    if (!GbcColor_IsEnabled()) return;

    if (GbcColor_BattleAutoColor()) {
        GbcColor_ApplyAutoColorAll();
        return;
    }

    const uint16_t *rgb = GbcColor_SuperPalette(RSGB_PAL_MEWMON);
    if (rgb) {
        for (int i = 0; i < 8; i++) Display_SetBGColorPalette(i, rgb);
        Display_SetOBJColorPalette(0, rgb);
        Display_SetOBJColorPalette(1, rgb);
    }
    Display_SetPositionAttrMode(0);
    Display_ClearAttrBoxes(0);
    Display_SetColorMode(1);
}

void Intro_Start(void) {
    intro_apply_palettes();
    gActive  = 1;
    gWaiting = 0;
    gState   = IS_OAK_PIC_DRAW;
    gNameOpened = 0;
    gRivalOpened = 0;
    init_player_data();

    Music_Play(MUSIC_ROUTES2);
}

int Intro_IsActive(void) { return gActive; }

void Intro_Tick(void) {
    if (!gActive) return;

    if (gWaiting) {
        if (Text_IsOpen()) return;
        gWaiting = 0;
        gState = gAfterWait;
    }

    switch (gState) {
        case IS_OAK_PIC_DRAW:
            clear_screen();
            draw_pic_centered(gIntroOakPic);
            begin_intro_pic_fadein();
            gState = IS_OAK_PIC_FADEIN;
            break;
        case IS_OAK_PIC_FADEIN:
            if (pump_intro_pic_fadein()) gState = IS_SPEECH1;
            break;
        case IS_SPEECH1:
            Text_ShowASCII(RomText("_OakSpeechText1"));
            gWaiting = 1;
            gAfterWait = IS_FADE_OUT_1;
            break;
        case IS_FADE_OUT_1:
            begin_fade_out_white(IS_NIDORINO_DRAW);
            break;
        case IS_FADING_OUT:
            if (pump_fade(kFadeOutToWhite, 3)) gState = gAfterWait;
            break;
        case IS_NIDORINO_DRAW:
            clear_screen();

            Display_SetBGP(0xE4);
            draw_nidorino_pic_flipped();
            gSlidePx = SLIDE_START_PX;
            begin_pic_slide();
            gState = IS_NIDORINO_SLIDE;
            break;
        case IS_NIDORINO_SLIDE:
            if (pump_pic_slide(&gSlidePx)) gState = IS_SPEECH2A;
            break;
        case IS_SPEECH2A:

            Text_SetPendingSFXOnPrint(play_nidorina_cry);
            Text_ShowASCII(RomText("_OakSpeechText2A"));
            gWaiting = 1;
            gAfterWait = IS_SPEECH2B;
            break;
        case IS_SPEECH2B: {

            const char *p = RomText("_OakSpeechText2B");
            while (*p == '\f' || *p == '\n') p++;
            Text_ShowASCII(p);
            gWaiting = 1;
            gAfterWait = IS_FADE_OUT_2;
            break;
        }
        case IS_FADE_OUT_2:
            begin_fade_out_white(IS_RED_PIC1_DRAW);
            break;
        case IS_RED_PIC1_DRAW:
            clear_screen();
            Display_SetBGP(0xE4);
            draw_pic_centered(kHofRedFrontSprite);
            gSlidePx = SLIDE_START_PX;
            begin_pic_slide();
            gState = IS_RED_PIC1_SLIDE;
            break;
        case IS_RED_PIC1_SLIDE:
            if (pump_pic_slide(&gSlidePx)) gState = IS_NAME_PROMPT;
            break;
        case IS_NAME_PROMPT:

            Text_KeepTilesOnClose();
            Text_ShowASCII(RomText("_IntroducePlayerText"));
            gWaiting = 1;
            gAfterWait = IS_NAME_SLIDE_RIGHT;
            gSlidePx = 0;
            gDelay = 0;
            break;
        case IS_NAME_SLIDE_RIGHT:

            if (pump_pic_slide_out(&gSlidePx, &gDelay)) {
                finish_pic_slide_out(kHofRedFrontSprite);
                gState = IS_NAME_MENU;
            }
            break;
        case IS_NAME_MENU:
            if (!gMenuDrawn) {
                draw_name_menu(kPresetNamesPlayerASCII);
                gMenuDrawn = 1;
                break;
            }
            if (hJoyPressed & PAD_DOWN) {
                int old_row = gMenuCursor;
                gMenuCursor = (gMenuCursor + 1) % NAME_MENU_ITEMS;
                update_menu_cursor(old_row, gMenuCursor);
                break;
            }
            if (hJoyPressed & PAD_UP) {
                int old_row = gMenuCursor;
                gMenuCursor = (gMenuCursor + NAME_MENU_ITEMS - 1) % NAME_MENU_ITEMS;
                update_menu_cursor(old_row, gMenuCursor);
                break;
            }
            if (hJoyPressed & PAD_A) {
                gMenuDrawn = 0;
                if (gMenuCursor == 0) {
                    gState = IS_NAME_ENTRY;
                } else {

                    memcpy(wPlayerName, kPresetNamesPlayer[gMenuCursor - 1], 5);
                    clear_name_menu_box();

                    gSlidePx = 0;

                    gDelay = 10 + 3;
                    gState = IS_NAME_SLIDE_BACK;
                }
                break;
            }
            break;
        case IS_NAME_SLIDE_BACK:

            if (pump_pic_slide_back(&gSlidePx, &gDelay)) {
                clear_screen();
                draw_pic_centered(kHofRedFrontSprite);
                gState = IS_NAME_CONFIRM;
            }
            break;
        case IS_NAME_ENTRY:
            if (!gNameOpened) {
                NamingScreen_Open(NAME_PLAYER_SCREEN, 0, wPlayerName);
                gNameOpened = 1;
                break;
            }
            if (NamingScreen_IsOpen()) break;
            if (wPlayerName[0] == 0x00 || wPlayerName[0] == 0x50) {
                gNameOpened = 0;
                break;
            }

            clear_screen();
            draw_pic_centered(kHofRedFrontSprite);
            gState = IS_NAME_CONFIRM;
            break;
        case IS_NAME_CONFIRM:

            Text_ShowASCII(RomText("_YourNameIsText"));
            gWaiting = 1;
            gAfterWait = IS_FADE_OUT_3;
            break;
        case IS_FADE_OUT_3:
            begin_fade_out_white(IS_RIVAL_PIC_DRAW);
            break;
        case IS_RIVAL_PIC_DRAW:
            clear_screen();
            draw_pic_centered(gIntroRival1Pic);
            begin_intro_pic_fadein();
            gState = IS_RIVAL_PIC_FADEIN;
            break;
        case IS_RIVAL_PIC_FADEIN:
            if (pump_intro_pic_fadein()) gState = IS_RIVAL_PROMPT;
            break;
        case IS_RIVAL_PROMPT:

            Text_KeepTilesOnClose();
            Text_ShowASCII(RomText("_IntroduceRivalText"));
            gWaiting = 1;
            gAfterWait = IS_RIVAL_SLIDE_RIGHT;
            gSlidePx = 0;
            gDelay = 0;
            break;
        case IS_RIVAL_SLIDE_RIGHT:

            if (pump_pic_slide_out(&gSlidePx, &gDelay)) {
                finish_pic_slide_out(gIntroRival1Pic);
                gState = IS_RIVAL_MENU;
            }
            break;
        case IS_RIVAL_MENU:
            if (!gMenuDrawn) {
                draw_name_menu(kPresetNamesRivalASCII);
                gMenuDrawn = 1;
                break;
            }
            if (hJoyPressed & PAD_DOWN) {
                int old_row = gMenuCursor;
                gMenuCursor = (gMenuCursor + 1) % NAME_MENU_ITEMS;
                update_menu_cursor(old_row, gMenuCursor);
                break;
            }
            if (hJoyPressed & PAD_UP) {
                int old_row = gMenuCursor;
                gMenuCursor = (gMenuCursor + NAME_MENU_ITEMS - 1) % NAME_MENU_ITEMS;
                update_menu_cursor(old_row, gMenuCursor);
                break;
            }
            if (hJoyPressed & PAD_A) {
                gMenuDrawn = 0;
                if (gMenuCursor == 0) {
                    gState = IS_RIVAL_ENTRY;
                } else {

                    memcpy(wRivalName, kPresetNamesRival[gMenuCursor - 1], 5);
                    clear_name_menu_box();

                    gSlidePx = 0;
                    gDelay = 10 + 3;
                    gState = IS_RIVAL_SLIDE_BACK;
                }
                break;
            }
            break;
        case IS_RIVAL_SLIDE_BACK:

            if (pump_pic_slide_back(&gSlidePx, &gDelay)) {
                clear_screen();
                draw_pic_centered(gIntroRival1Pic);
                gState = IS_RIVAL_CONFIRM;
            }
            break;
        case IS_RIVAL_ENTRY:
            if (!gRivalOpened) {
                NamingScreen_Open(NAME_RIVAL_SCREEN, 0, wRivalName);
                gRivalOpened = 1;
                break;
            }
            if (NamingScreen_IsOpen()) break;
            if (wRivalName[0] == 0x00 || wRivalName[0] == 0x50) {
                gRivalOpened = 0;
                break;
            }

            clear_screen();
            draw_pic_centered(gIntroRival1Pic);
            gState = IS_RIVAL_CONFIRM;
            break;
        case IS_RIVAL_CONFIRM:
            Text_ShowASCII(RomText("_HisNameIsText"));
            gWaiting = 1;
            gAfterWait = IS_FADE_OUT_4;
            break;
        case IS_FADE_OUT_4:
            begin_fade_out_white(IS_RED_PIC2_DRAW);
            break;
        case IS_RED_PIC2_DRAW:
            clear_screen();
            draw_pic_centered(kHofRedFrontSprite);
            gFadeStep = 0;
            gFadeTimer = 8;
            Display_SetPalette(kFadeInFromWhite[0][0], kFadeInFromWhite[0][1], kFadeInFromWhite[0][2]);
            gState = IS_RED_PIC2_FADEIN;
            break;
        case IS_RED_PIC2_FADEIN:
            if (pump_fade(kFadeInFromWhite, 3)) gState = IS_SPEECH3;
            break;
        case IS_SPEECH3:

            Text_ShowASCII(RomText("_OakSpeechText3"));
            gWaiting = 1;
            gAfterWait = IS_SHRINK_SFX;
            break;
        case IS_SHRINK_SFX:
            Audio_PlaySFX_Shrink();
            gDelay = 4;
            gState = IS_SHRINK1_DRAW;
            break;
        case IS_SHRINK1_DRAW:

            if (--gDelay > 0) break;
            draw_pic_centered(gIntroShrinkPic1);
            gDelay = 4;
            gState = IS_SHRINK2_DRAW;
            break;
        case IS_SHRINK2_DRAW:
            if (--gDelay > 0) break;
            draw_pic_centered(gIntroShrinkPic2);

            Music_PlayDefaultFadeOutCurrent(MUSIC_PALLET_TOWN);
            gDelay = 20;
            gState = IS_MUSIC_STOP;
            break;
        case IS_MUSIC_STOP:

            if (--gDelay > 0) break;
            clear_screen();

            gPlayerFacing = 0;
            wXCoord = 0;
            wYCoord = 0;
            gCamX = -Map_CamHalfX();
            gCamY = -8;
            Player_SyncOAM();
            gDelay = 50;
            gState = IS_SETTLE;
            break;
        case IS_SETTLE:
            if (--gDelay > 0) break;
            gState = IS_FINAL_FADE_OUT;
            break;
        case IS_FINAL_FADE_OUT:

            gDelay = 20;
            begin_fade_out_white(IS_POST_OAK_HOLD1);
            break;
        case IS_POST_OAK_HOLD1:

            if (--gDelay > 0) break;
            gDelay = 20;
            gState = IS_POST_OAK_HOLD2;
            break;
        case IS_POST_OAK_HOLD2:

            gPlayerFacing = 0;
            wXCoord = 0;
            wYCoord = 0;
            gCamX = -Map_CamHalfX();
            gCamY = -8;
            Player_SyncOAM();
            gDelay = 20;
            gState = IS_BIND_MAP;
            break;
        case IS_BIND_MAP:

            if (--gDelay > 0) break;
            AmberScript_SetEnabled(1);
            {

                int rid = AmberScript_MapBank_GetOrAssignRealId("RedsHouse2F");
                wCurMap = (rid >= 0) ? (uint8_t)rid : (uint8_t)MAP_REDS_HOUSE_2F;
            }
            wXCoord  = BEDROOM_X;
            wYCoord  = BEDROOM_Y;

            gCamX = (int)wXCoord * 2 - Map_CamHalfX();
            gCamY = (int)wYCoord * 2 - 8;
            Player_SyncOAM();
            Display_LoadMapPalette();
            Display_SetPalette(0xE4, 0xD0, 0xE0);
            gActive = 0;
            gState  = IS_IDLE;
            break;
        default:
            break;
    }
}
