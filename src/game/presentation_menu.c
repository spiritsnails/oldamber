
#include "presentation_menu.h"
#include "../platform/data_dir.h"
#include "gbc_color.h"
#include "speed_settings.h"
#include "battle/battle_exp.h"
#include "gen1color/gen1color_battle.h"
#include "battle/move_anim.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "menu.h"
#include "../data/font_data.h"
#include "../platform/display.h"
#include "../platform/sgb_border.h"
#include "../platform/ntsc_filter.h"
#include "../platform/crt_renderer.h"
#include "../platform/display_gl.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include <string.h>
#include <stdio.h>

extern int Game_GetScene(void);

#define CHAR_TERM   0x50
#define CHAR_SPACE  0x7F
#define CHAR_HOLLOW 0xEC
#define CHAR_FILLED 0xED

static void smset(int col, int row, uint8_t tile) {
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static int ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (int)Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (int)Font_CharToTile(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return (int)Font_CharToTile(0xF6 + (c - '0'));
    if (c == '/') return (int)Font_CharToTile(0xF3);
    if (c == '-') return (int)Font_CharToTile(0xE3);
    if (c == '.') return (int)Font_CharToTile(0xE8);

    if (c == ',') return (int)Font_CharToTile(0xF4);
    return (int)Font_CharToTile(CHAR_SPACE);
}

static void put(int col, int row, const char *s) {
    for (; *s; s++, col++) smset(col, row, (uint8_t)ascii_tile((unsigned char)*s));
}

static void put_marker(int col, int row, uint8_t ch) {
    smset(col, row, (uint8_t)Font_CharToTile(ch));
}

static void clear_row(int from_col, int to_col, int row) {
    for (int c = from_col; c <= to_col; c++) smset(c, row, (uint8_t)Font_CharToTile(CHAR_SPACE));
}

#define MARQUEE_HOLD 40
#define MARQUEE_STEP 10
static uint32_t s_marquee;

static int marquee_shift(int travel) {
    const int slide = travel * MARQUEE_STEP;
    const int span  = MARQUEE_HOLD + slide + MARQUEE_HOLD + slide;
    int t = (int)(s_marquee % (uint32_t)span);
    if (t < MARQUEE_HOLD) return 0;
    t -= MARQUEE_HOLD;
    if (t < slide) return t / MARQUEE_STEP;
    t -= slide;
    if (t < MARQUEE_HOLD) return travel;
    t -= MARQUEE_HOLD;
    return travel - 1 - (t / MARQUEE_STEP);
}

static void put_value(int col, int end_col, int row, const char *s) {
    const int avail = end_col - col + 1;
    if (avail <= 0) return;
    const int len = (int)strlen(s);
    const int off = (len > avail) ? marquee_shift(len - avail) : 0;
    for (int i = 0; i < avail && s[off + i]; i++)
        smset(col + i, row, (uint8_t)ascii_tile((unsigned char)s[off + i]));
}

static void draw_box(int l, int t, int r, int b) {
    smset(l, t, (uint8_t)Font_CharToTile(0x79));
    for (int c = l + 1; c < r; c++) smset(c, t, (uint8_t)Font_CharToTile(0x7A));
    smset(r, t, (uint8_t)Font_CharToTile(0x7B));
    for (int y = t + 1; y < b; y++) {
        smset(l, y, (uint8_t)Font_CharToTile(0x7C));
        for (int c = l + 1; c < r; c++) smset(c, y, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(r, y, (uint8_t)Font_CharToTile(0x7C));
    }
    smset(l, b, (uint8_t)Font_CharToTile(0x7D));
    for (int c = l + 1; c < r; c++) smset(c, b, (uint8_t)Font_CharToTile(0x7A));
    smset(r, b, (uint8_t)Font_CharToTile(0x7E));
}

typedef struct { const char *label; int value; } choice_t;

static const choice_t kColorChoices[] = {
    {"ON",  1},
    {"OFF", 0},
};
static const choice_t kCurveChoices[] = {
    {"LINEAR",   GBC_CURVE_LINEAR},
    {"GAMBATTE", GBC_CURVE_GAMBATTE},
    {"ACCURATE", GBC_CURVE_SAMEBOY_CURVE},
    {"MELLOW",   GBC_CURVE_SAMEBOY_MELLOW},
    {"HARDWARE", GBC_CURVE_SAMEBOY_HW},
    {"SOFT",     GBC_CURVE_SAMEBOY_SOFT},
    {"GBC LCD",  GBC_CURVE_LCD_PANEL},

    {"SGB",      GBC_CURVE_SAMEBOY_SGB},

    {"REDUCED",  GBC_CURVE_SAMEBOY_REDUCE},
    {"HARSH",    GBC_CURVE_SAMEBOY_HARSH},
    {"BOOSTED",  GBC_CURVE_SAMEBOY_BOOST},
};

static const choice_t kUiChoices[] = {
    {"GEN 1", G1C_UI_GEN1},
};

static const choice_t kSpriteChoices[] = {
    {"GEN 1",   G1C_SPRITES_GEN1},
};

#define PAL_CHOICE_GBC_AUTO 99

static const choice_t kPaletteChoices[] = {
    {"SGB",      G1C_MONPAL_SGB},
    {"GBC AUTO", PAL_CHOICE_GBC_AUTO},
};
static const choice_t kFieldChoices[] = {
    {"SGB",       GBC_OVERWORLD_RED_SGB},
    {"AUTOCOLOR", GBC_OVERWORLD_RED_AUTOCOLOR},
};

static const choice_t kAnimSpeedChoices[] = {
    {"NORMAL", SPEED_NORMAL},
    {"2X",     2},
    {"3X",     3},
    {"4X",     4},
    {"TURBO",  SPEED_UNCAPPED},
};
static const choice_t kMiscAnimChoices[] = {
    {"NORMAL", SPEED_NORMAL},
    {"2X",     2},
    {"3X",     3},
    {"4X",     4},
    {"TURBO",  SPEED_UNCAPPED},
};
static const choice_t kTransitionChoices[] = {
    {"NORMAL", SPEED_NORMAL},
    {"2X",     2},
    {"3X",     3},
    {"4X",     4},
    {"TURBO",  SPEED_UNCAPPED},
};

static const choice_t kBattleAllChoices[] = {
    {"NORMAL", SPEED_NORMAL},
    {"2X",     2},
    {"3X",     3},
    {"4X",     4},
    {"TURBO",  SPEED_UNCAPPED},
    {"CUSTOM", -1},
};
#define BATTLE_CUSTOM_INDEX 5
static const choice_t kHpBarChoices[] = {
    {"NORMAL",  SPEED_NORMAL},
    {"2X",      2},
    {"3X",      3},
    {"4X",      4},
    {"INSTANT", SPEED_UNCAPPED},
};
static const choice_t kTextSpeedChoices[] = {
    {"NORMAL",  SPEED_NORMAL},
    {"2X",      2},
    {"3X",      3},
    {"4X",      4},
    {"INSTANT", SPEED_UNCAPPED},
};

static const choice_t kRomScrollChoices[] = {
    {"OFF", 0},
    {"ON",  1},
};
static const choice_t kOverworldSpeedChoices[] = {
    {"NORMAL", SPEED_NORMAL},
    {"2X",     2},
    {"3X",     3},
    {"4X",     4},
    {"TURBO",  SPEED_UNCAPPED},
};

static const choice_t kCriesChoices[] = {
    {"GEN 1",   AUDIO_CRIES_GEN1},
};

static const choice_t kVolumeChoices[] = {
    {"OFF", 0},
    {"1",  1}, {"2",  2}, {"3", 3}, {"4", 4}, {"5",  5},
    {"6",  6}, {"7",  7}, {"8", 8}, {"9", 9}, {"10", 10},
};

#define N_COLOR   ((int)(sizeof kColorChoices   / sizeof kColorChoices[0]))
#define N_CURVE   ((int)(sizeof kCurveChoices   / sizeof kCurveChoices[0]))
#define N_UI      ((int)(sizeof kUiChoices      / sizeof kUiChoices[0]))
#define N_SPRITE  ((int)(sizeof kSpriteChoices  / sizeof kSpriteChoices[0]))
#define N_PALETTE ((int)(sizeof kPaletteChoices / sizeof kPaletteChoices[0]))
#define N_FIELD   ((int)(sizeof kFieldChoices   / sizeof kFieldChoices[0]))

static const char *const kFilterFiles[] = {
    "NearestNeighbor", "Bilinear", "SmoothBilinear",
    "LCD", "MonoLCD", "CRT", "FlatCRT",
    "Scale2x", "Scale4x", "AAScale2x", "AAScale4x", "HQ2x",
    "OmniScale", "OmniScaleLegacy", "AAOmniScaleLegacy",
};
static const choice_t kFilterChoices[] = {
    {"NEAREST",   0}, {"BILINEAR",  1}, {"SMOOTH",    2},
    {"LCD",       3}, {"MONO LCD",  4},

    {"SB CRT",    5}, {"SB FLAT CRT", 6},
    {"SCALE2X",   7}, {"SCALE4X",   8}, {"AA SCALE2X", 9}, {"AA SCALE4X", 10},
    {"HQ2X",     11},
    {"OMNISCALE", 12}, {"OMNI LEGACY", 13}, {"AA OMNI", 14},
};

static const choice_t kRendererChoices[] = {
    {"SDL",    0},
    {"OPENGL", 1},
};

#define WINSCALE_FULLSCREEN 0

static const choice_t kFastBootChoices[] = {
    {"OFF", 0}, {"ON", 1},
};

static const choice_t kWinScaleChoices[] = {
    {"2X",  2}, {"3X",  3}, {"4X",  4}, {"5X",  5},
    {"6X",  6}, {"7X",  7}, {"8X",  8},
    {"FULLSCREEN", WINSCALE_FULLSCREEN},
};
static const choice_t kScaleChoices[] = {
    {"INTEGER", DISPLAY_GL_SCALE_INTEGER},
    {"ASPECT",  DISPLAY_GL_SCALE_ASPECT},
    {"STRETCH", DISPLAY_GL_SCALE_STRETCH},
};

static const choice_t kHexChoices[] = {
    {"00",   0}, {"11",  17}, {"22",  34}, {"33",  51},
    {"44",  68}, {"55",  85}, {"66", 102}, {"77", 119},
    {"88", 136}, {"99", 153}, {"AA", 170}, {"BB", 187},
    {"CC", 204}, {"DD", 221}, {"EE", 238}, {"FF", 255},
};

static const choice_t kCrtChoices[] = {
    {"OFF",     CRT_PROFILE_OFF},
    {"CLEAN",   CRT_PROFILE_CLEAN},
    {"SGB TV",  CRT_PROFILE_SGB_CONSUMER},
    {"SGB PVM", CRT_PROFILE_SGB_RGB_PVM},
};

static const choice_t kCrtCurveChoices[] = {
    {"PROFILE", CRT_CURVE_PROFILE},
    {"OFF",     CRT_CURVE_OFF},
    {"SUBTLE",  CRT_CURVE_SUBTLE},
    {"TV",      CRT_CURVE_TV},
};
static const choice_t kNtscChoices[] = {
    {"OFF", 0},
    {"ON",  1},
};

static const choice_t kExpShareChoices[] = {
    {"OFF", 0},
    {"ON",  1},
};
static const choice_t kSgbBorderChoices[] = {
    {"OFF", 0},
    {"ON",  1},
};
static const choice_t kVSyncChoices[] = {
    {"OFF",      DISPLAY_GL_VSYNC_OFF},
    {"ON",       DISPLAY_GL_VSYNC_ON},
    {"ADAPTIVE", DISPLAY_GL_VSYNC_ADAPTIVE},
};

static const choice_t kRenderFpsChoices[] = {
    {"30 FPS",   30},
    {"60 FPS",   60},
    {"75 FPS",   75},
    {"90 FPS",   90},
    {"120 FPS", 120},
    {"144 FPS", 144},
    {"165 FPS", 165},
    {"180 FPS", 180},
    {"240 FPS", 240},
    {"360 FPS", 360},
};
static const choice_t kBlendChoices[] = {
    {"OFF",      DISPLAY_GL_BLEND_DISABLED},
    {"SIMPLE",   DISPLAY_GL_BLEND_SIMPLE},
    {"ACCURATE", DISPLAY_GL_BLEND_ACCURATE},
};

static const choice_t kMonoPalChoices[] = {
    {"PORT",   DISPLAY_MONO_PAL_PORT},
    {"GREY",   DISPLAY_MONO_PAL_GREY},
    {"DMG",    DISPLAY_MONO_PAL_DMG},
    {"POCKET", DISPLAY_MONO_PAL_MGB},
    {"LIGHT",  DISPLAY_MONO_PAL_GBL},
    {"CUSTOM", DISPLAY_MONO_PAL_CUSTOM},
};

static const choice_t kLightTempChoices[] = {
    {"-1.0", -10}, {"-0.8", -8}, {"-0.6", -6}, {"-0.4", -4}, {"-0.2", -2},
    {"OFF",    0},
    {"+0.2",   2}, {"+0.4",  4}, {"+0.6",  6}, {"+0.8",  8}, {"+1.0", 10},
};

#define N_CRIES   ((int)(sizeof kCriesChoices   / sizeof kCriesChoices[0]))
#define N_MONOPAL ((int)(sizeof kMonoPalChoices / sizeof kMonoPalChoices[0]))
#define N_LTEMP   ((int)(sizeof kLightTempChoices / sizeof kLightTempChoices[0]))
#define N_FILTER  ((int)(sizeof kFilterChoices  / sizeof kFilterChoices[0]))
#define N_RENDER  ((int)(sizeof kRendererChoices / sizeof kRendererChoices[0]))
#define N_BLEND   ((int)(sizeof kBlendChoices   / sizeof kBlendChoices[0]))
#define N_VSYNC   ((int)(sizeof kVSyncChoices   / sizeof kVSyncChoices[0]))
#define N_RENDERFPS ((int)(sizeof kRenderFpsChoices / sizeof kRenderFpsChoices[0]))

static const choice_t kAspectChoices[] = {
    { "NATIVE", 0 },
    { "16:9",   1 },
};
#define N_ASPECT  ((int)(sizeof kAspectChoices  / sizeof kAspectChoices[0]))
#define N_SGBBORDER ((int)(sizeof kSgbBorderChoices / sizeof kSgbBorderChoices[0]))
#define N_NTSC    ((int)(sizeof kNtscChoices / sizeof kNtscChoices[0]))
#define N_CRT     ((int)(sizeof kCrtChoices  / sizeof kCrtChoices[0]))
#define N_CRTCURVE ((int)(sizeof kCrtCurveChoices / sizeof kCrtCurveChoices[0]))

static int filter_is_tube_shader(int index) {
    return index >= 0 && index < N_FILTER &&
           (strcmp(kFilterFiles[index], "CRT") == 0 ||
            strcmp(kFilterFiles[index], "FlatCRT") == 0);
}

static int filter_blocked(int index) {
    return CrtRenderer_Profile() != CRT_PROFILE_OFF &&
           filter_is_tube_shader(index);
}
#define N_HEX     ((int)(sizeof kHexChoices     / sizeof kHexChoices[0]))
#define N_SCALE   ((int)(sizeof kScaleChoices   / sizeof kScaleChoices[0]))
#define N_EXPSHARE ((int)(sizeof kExpShareChoices / sizeof kExpShareChoices[0]))
#define N_FASTBOOT ((int)(sizeof kFastBootChoices / sizeof kFastBootChoices[0]))
#define N_WINSCALE ((int)(sizeof kWinScaleChoices / sizeof kWinScaleChoices[0]))
#define N_VOLUME  ((int)(sizeof kVolumeChoices  / sizeof kVolumeChoices[0]))
#define N_ANIMSPD ((int)(sizeof kAnimSpeedChoices / sizeof kAnimSpeedChoices[0]))
#define N_HPBAR   ((int)(sizeof kHpBarChoices     / sizeof kHpBarChoices[0]))
#define N_ROMSCROLL ((int)(sizeof kRomScrollChoices / sizeof kRomScrollChoices[0]))
#define N_OWSPD   ((int)(sizeof kOverworldSpeedChoices / sizeof kOverworldSpeedChoices[0]))
#define N_TEXTSPD ((int)(sizeof kTextSpeedChoices / sizeof kTextSpeedChoices[0]))
#define N_MISCSPD ((int)(sizeof kMiscAnimChoices  / sizeof kMiscAnimChoices[0]))
#define N_TRANS   ((int)(sizeof kTransitionChoices / sizeof kTransitionChoices[0]))
#define N_BATTLE  BATTLE_CUSTOM_INDEX

enum {
    ROW_COLOR = 0, ROW_CURVE, ROW_UI, ROW_SPRITES, ROW_PALETTE,
    ROW_FIELD, ROW_CRIES, ROW_ANIMSPD, ROW_HPBAR, ROW_OWSPD, ROW_TEXTSPD,
    ROW_EXPSHARE,
    ROW_FASTBOOT,
    ROW_MISCSPD, ROW_TRANSSPD, ROW_BATTLEALL, ROW_ROMSCROLL,
    ROW_VOLMASTER, ROW_VOLMUSIC, ROW_VOLSFX,
    ROW_RENDERER, ROW_FILTER, ROW_BLEND, ROW_MONOPAL, ROW_LTEMP, ROW_SCALE, ROW_WINSCALE, ROW_VSYNC,
    ROW_SGBBORDER, ROW_NTSC, ROW_CRT, ROW_CRTCURVE, ROW_ASPECT,

    ROW_PAL_FIRST,
    ROW_PAL_S0R = ROW_PAL_FIRST, ROW_PAL_S0G, ROW_PAL_S0B,
    ROW_PAL_S1R, ROW_PAL_S1G, ROW_PAL_S1B,
    ROW_PAL_S2R, ROW_PAL_S2G, ROW_PAL_S2B,
    ROW_PAL_S3R, ROW_PAL_S3G, ROW_PAL_S3B,
    ROW_PAL_LAST = ROW_PAL_S3B,

    ROW_RENDERFPS
};

typedef enum { RK_VALUE, RK_SUBMENU, RK_BACK } rowkind_t;

typedef struct {
    uint8_t     kind;

    uint8_t     setting;
    const char *label;
    const choice_t *tbl;
    int8_t      y;
    int8_t      indent;
    int8_t      two_line;
    int8_t      label_w;
} menu_row_t;

typedef struct { int8_t y; const char *text; } menu_header_t;

typedef struct {
    const menu_row_t    *rows;
    int                  n_rows;
    const menu_header_t *headers;
    int                  n_headers;
    int8_t               box_bottom;

    int8_t               box_l;
    int8_t               box_r;
} menu_page_t;

#define LABEL_W     7
#define LABEL_W_MAX 10

#define HEADER_BATTLE_Y    3
#define HEADER_OVERWORLD_Y 11

#define BOX_TOP    0

#define MAIN_BOX_L       0
#define MAIN_BOX_R      10
#define MAIN_ITEM_ROW_1  2
#define MAIN_ITEM_STEP   2
#define MAIN_BOX_BOTTOM  13
#define MAIN_CANCEL_ROW  (MAIN_ITEM_ROW_1 + 5 * MAIN_ITEM_STEP)

#define LOWEST_BACK_ROW  15

typedef char presentation_menu_cancel_fits[(LOWEST_BACK_ROW < SCREEN_HEIGHT) ? 1 : -1];
#define COL_L   0
#define COL_R  19

static int inner_l_of_page(void);
static int inner_r_of_page(void);
#define INNER_L (inner_l_of_page())
#define INNER_R (inner_r_of_page())

#define PAGE_MAIN     0
#define PAGE_GRAPHICS 1
#define PAGE_SPEED    2
#define PAGE_AUDIO    3
#define PAGE_DISPLAY  4
#define PAGE_PALETTE  5

#define PAGE_GAMEPLAY 6

static const menu_row_t kMainRows[] = {
    { RK_SUBMENU, PAGE_GRAPHICS, "GRAPHICS", NULL, MAIN_ITEM_ROW_1 + 0 * MAIN_ITEM_STEP, 0, 0, 0 },
    { RK_SUBMENU, PAGE_SPEED,    "SPEED",    NULL, MAIN_ITEM_ROW_1 + 1 * MAIN_ITEM_STEP, 0, 0, 0 },
    { RK_SUBMENU, PAGE_AUDIO,    "AUDIO",    NULL, MAIN_ITEM_ROW_1 + 2 * MAIN_ITEM_STEP, 0, 0, 0 },
    { RK_SUBMENU, PAGE_DISPLAY,  "DISPLAY",  NULL, MAIN_ITEM_ROW_1 + 3 * MAIN_ITEM_STEP, 0, 0, 0 },
    { RK_SUBMENU, PAGE_GAMEPLAY, "GAMEPLAY", NULL, MAIN_ITEM_ROW_1 + 4 * MAIN_ITEM_STEP, 0, 0, 0 },
    { RK_BACK,    0,             "CANCEL",   NULL, MAIN_CANCEL_ROW, 0, 0, 0 },
};

#define AUDIO_BOX_BOTTOM 6
static const menu_row_t kAudioRows[] = {
    { RK_VALUE, ROW_VOLMASTER, "MASTER", kVolumeChoices, 1, 0, 0, LABEL_W },
    { RK_VALUE, ROW_VOLMUSIC,  "MUSIC",  kVolumeChoices, 2, 1, 0, LABEL_W },
    { RK_VALUE, ROW_VOLSFX,    "SFX",    kVolumeChoices, 3, 1, 0, LABEL_W },
    { RK_VALUE, ROW_CRIES,     "CRIES",  kCriesChoices,  5, 0, 0, LABEL_W },

    { RK_BACK,  0,             "BACK",   NULL, AUDIO_BOX_BOTTOM + 2, 0, 0, 0 },
};

static const menu_row_t kGraphicsRows[] = {
    { RK_VALUE, ROW_COLOR,   "COLOR",             kColorChoices,    1, 0, 0, LABEL_W },
    { RK_VALUE, ROW_CURVE,   "CURVE",             kCurveChoices,    2, 0, 0, LABEL_W },
    { RK_VALUE, ROW_UI,      "UI",                kUiChoices,       4, 0, 0, LABEL_W },
    { RK_VALUE, ROW_SPRITES, "BATTLE SPRITES",    kSpriteChoices,   5, 0, 1, 0 },
    { RK_VALUE, ROW_PALETTE, "BATTLE PALETTES",   kPaletteChoices,  7, 1, 1, 0 },
    { RK_VALUE, ROW_FIELD,   "OVERWORLD PALETTE", kFieldChoices,   11, 0, 1, 0 },
    { RK_BACK,  0,           "BACK",              NULL,            15, 0, 0, 0 },
};
static const menu_header_t kGraphicsHeaders[] = {
    { 3,  "BATTLE" },
    { 10, "OVERWORLD" },
};

static const menu_row_t kSpeedRows[] = {
    { RK_VALUE, ROW_OWSPD,     "OVERWORLD",       kOverworldSpeedChoices, 1, 0, 0, 10 },
    { RK_VALUE, ROW_TEXTSPD,   "TEXT",            kTextSpeedChoices,      2, 0, 0, 10 },

    { RK_VALUE, ROW_ROMSCROLL, "GB SCROLL",       kRomScrollChoices,      3, 0, 0, 10 },
    { RK_VALUE, ROW_BATTLEALL, "ALL",             kBattleAllChoices,      5, 0, 0, 10 },

    { RK_VALUE, ROW_ANIMSPD,   "MOVE ANIMATIONS", kAnimSpeedChoices,      6, 1, 0, 10 },
    { RK_VALUE, ROW_HPBAR,     "HP BAR",          kHpBarChoices,          7, 1, 0, 10 },
    { RK_VALUE, ROW_MISCSPD,   "MISC ANIMATIONS", kMiscAnimChoices,       8, 1, 1, 0 },
    { RK_VALUE, ROW_TRANSSPD,  "TRANSITIONS",     kTransitionChoices,    10, 1, 1, 0 },
    { RK_BACK,  0,             "BACK",            NULL,                  14, 0, 0, 0 },
};
static const menu_header_t kSpeedHeaders[] = {
    { 4, "BATTLE" },
};

#define DISPLAY_LABEL_W    8
#define DISPLAY_BOX_BOTTOM 17
static const menu_row_t kDisplayRows[] = {
    { RK_VALUE, ROW_RENDERER, "RENDERER", kRendererChoices,  1, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_WINSCALE, "SIZE",     kWinScaleChoices,  2, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_RENDERFPS,"FPS",      kRenderFpsChoices, 3, 0, 0, DISPLAY_LABEL_W },

    { RK_VALUE, ROW_FILTER,   "FILTER",   kFilterChoices,    4, 0, 1, 0 },

    { RK_VALUE, ROW_ASPECT,   "ASPECT",   kAspectChoices,    6, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_BLEND,    "GHOSTING", kBlendChoices,     7, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_SCALE,    "SCALING",  kScaleChoices,     8, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_VSYNC,    "VSYNC",    kVSyncChoices,     9, 0, 0, DISPLAY_LABEL_W },

    { RK_VALUE, ROW_MONOPAL,  "MONO PAL", kMonoPalChoices,  10, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_LTEMP,    "LIGHT",    kLightTempChoices, 11, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_SGBBORDER, "SGB EDGE", kSgbBorderChoices, 12, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_NTSC,     "COMPOSITE", kNtscChoices,      13, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_CRT,      "CRT PROF",  kCrtChoices,       14, 0, 0, DISPLAY_LABEL_W },
    { RK_VALUE, ROW_CRTCURVE, "CRT CURVE", kCrtCurveChoices,  15, 0, 0, DISPLAY_LABEL_W },

    { RK_SUBMENU, PAGE_PALETTE, "EDIT CUSTOM PAL", NULL, 16, 0, 0, 0 },
    { RK_BACK,  0,            "BACK",     NULL, DISPLAY_BOX_BOTTOM + 2, 0, 0, 0 },
};

static const menu_header_t kPaletteHeaders[] = {
    { 0, "SHADE 0 IS LIGHTEST" },
};
#define PAL_LABEL_W 8
static const menu_row_t kPaletteRows[] = {
    { RK_VALUE, ROW_PAL_S0R, "S0 RED",   kHexChoices,  1, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S0G, "S0 GREEN", kHexChoices,  2, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S0B, "S0 BLUE",  kHexChoices,  3, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S1R, "S1 RED",   kHexChoices,  4, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S1G, "S1 GREEN", kHexChoices,  5, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S1B, "S1 BLUE",  kHexChoices,  6, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S2R, "S2 RED",   kHexChoices,  7, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S2G, "S2 GREEN", kHexChoices,  8, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S2B, "S2 BLUE",  kHexChoices,  9, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S3R, "S3 RED",   kHexChoices, 10, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S3G, "S3 GREEN", kHexChoices, 11, 0, 0, PAL_LABEL_W },
    { RK_VALUE, ROW_PAL_S3B, "S3 BLUE",  kHexChoices, 12, 0, 0, PAL_LABEL_W },
    { RK_BACK,  0,           "BACK",     NULL,        14, 0, 0, 0 },
};

static const menu_row_t kGameplayRows[] = {
    { RK_VALUE, ROW_EXPSHARE, "EXP SHARE", kExpShareChoices, 1, 0, 0, 10 },
    { RK_VALUE, ROW_FASTBOOT, "FAST BOOT", kFastBootChoices, 3, 0, 0, 10 },
    { RK_BACK,  0,            "BACK",      NULL,             5, 0, 0, 0 },
};
#define GAMEPLAY_BOX_BOTTOM 4

static const menu_page_t kPages[] = {
    { kMainRows,     (int)(sizeof kMainRows     / sizeof kMainRows[0]),
      NULL, 0, MAIN_BOX_BOTTOM, MAIN_BOX_L, MAIN_BOX_R },
    { kGraphicsRows, (int)(sizeof kGraphicsRows / sizeof kGraphicsRows[0]),
      kGraphicsHeaders, (int)(sizeof kGraphicsHeaders / sizeof kGraphicsHeaders[0]), 13, COL_L, COL_R },
    { kSpeedRows,    (int)(sizeof kSpeedRows    / sizeof kSpeedRows[0]),
      kSpeedHeaders,    (int)(sizeof kSpeedHeaders    / sizeof kSpeedHeaders[0]),    12, COL_L, COL_R },
    { kAudioRows,    (int)(sizeof kAudioRows    / sizeof kAudioRows[0]),
      NULL, 0, AUDIO_BOX_BOTTOM, COL_L, COL_R },
    { kDisplayRows,  (int)(sizeof kDisplayRows  / sizeof kDisplayRows[0]),
      NULL, 0, DISPLAY_BOX_BOTTOM, COL_L, COL_R },
    { kPaletteRows,  (int)(sizeof kPaletteRows  / sizeof kPaletteRows[0]),
      kPaletteHeaders, (int)(sizeof kPaletteHeaders / sizeof kPaletteHeaders[0]), 13, COL_L, COL_R },

    { kGameplayRows, (int)(sizeof kGameplayRows / sizeof kGameplayRows[0]),
      NULL, 0, GAMEPLAY_BOX_BOTTOM, COL_L, COL_R },
};

typedef char pages_cover_every_page_id[
    ((int)(sizeof kPages / sizeof kPages[0]) == PAGE_GAMEPLAY + 1) ? 1 : -1];

static int s_page;

static int s_return_row;
static const menu_page_t *page(void) { return &kPages[s_page]; }

static int inner_l_of_page(void) { return page()->box_l + 1; }
static int inner_r_of_page(void) { return page()->box_r - 1; }

static void apply(int row, int index);
static void PresentationMenu_SaveSettings(void);

static int s_fast_boot = 0;

int PresentationMenu_FastBoot(void) { return s_fast_boot; }

static int current_index(int row) {
    int v;
    const choice_t *tbl;
    int n;
    switch (row) {
    case ROW_COLOR:   v = GbcColor_IsEnabled();      tbl = kColorChoices;   n = N_COLOR;   break;
    case ROW_CURVE:   v = Display_GetColorCurve();   tbl = kCurveChoices;   n = N_CURVE;   break;
    case ROW_UI:      v = Gen1Color_UiStyle();       tbl = kUiChoices;      n = N_UI;      break;
    case ROW_SPRITES: v = Gen1Color_SpriteStyle();   tbl = kSpriteChoices;  n = N_SPRITE;  break;
    case ROW_PALETTE: v = GbcColor_BattleAutoColor() ? PAL_CHOICE_GBC_AUTO
                                                    : Gen1Color_MonPalStyle();
                      tbl = kPaletteChoices; n = N_PALETTE; break;
    case ROW_FIELD:   v = GbcColor_OverworldStyle(); tbl = kFieldChoices;   n = N_FIELD;   break;
    case ROW_CRIES:   v = Audio_GetCryStyle();       tbl = kCriesChoices;   n = N_CRIES;   break;
    case ROW_ANIMSPD: v = SpeedSettings_MoveAnim();    tbl = kAnimSpeedChoices; n = N_ANIMSPD; break;
    case ROW_HPBAR:   v = SpeedSettings_HpBar();       tbl = kHpBarChoices;     n = N_HPBAR;   break;
    case ROW_EXPSHARE: v = BattleExp_ModernShare();    tbl = kExpShareChoices;       n = N_EXPSHARE; break;
    case ROW_FASTBOOT: v = s_fast_boot;                tbl = kFastBootChoices;       n = N_FASTBOOT; break;
    case ROW_OWSPD:   v = SpeedSettings_Overworld();   tbl = kOverworldSpeedChoices; n = N_OWSPD; break;
    case ROW_TEXTSPD: v = SpeedSettings_Text();        tbl = kTextSpeedChoices;      n = N_TEXTSPD; break;
    case ROW_ROMSCROLL: v = SpeedSettings_RomTextScroll(); tbl = kRomScrollChoices;  n = N_ROMSCROLL; break;
    case ROW_MISCSPD: v = SpeedSettings_MiscAnim();    tbl = kMiscAnimChoices;       n = N_MISCSPD; break;
    case ROW_TRANSSPD: v = SpeedSettings_Transition(); tbl = kTransitionChoices;     n = N_TRANS;   break;
    case ROW_BATTLEALL: v = SpeedSettings_Battle();    tbl = kBattleAllChoices;      n = N_BATTLE;  break;
    case ROW_VOLMASTER: v = Audio_GetMasterVolume();   tbl = kVolumeChoices;         n = N_VOLUME;  break;
    case ROW_VOLMUSIC:  v = Audio_GetMusicVolume();    tbl = kVolumeChoices;         n = N_VOLUME;  break;
    case ROW_VOLSFX:    v = Audio_GetSfxVolume();      tbl = kVolumeChoices;         n = N_VOLUME;  break;
    case ROW_RENDERER:  v = DisplayGL_IsRequested();  tbl = kRendererChoices; n = N_RENDER; break;
    case ROW_BLEND:     v = (int)DisplayGL_Blending(); tbl = kBlendChoices;   n = N_BLEND;  break;
    case ROW_MONOPAL:   v = Display_MonoPalette();  tbl = kMonoPalChoices;  n = N_MONOPAL; break;
    case ROW_SCALE:     v = (int)DisplayGL_Scaling(); tbl = kScaleChoices;  n = N_SCALE;   break;
    case ROW_VSYNC:     v = (int)DisplayGL_VSync();   tbl = kVSyncChoices;  n = N_VSYNC;   break;
    case ROW_RENDERFPS: v = Display_RenderFPS();      tbl = kRenderFpsChoices; n = N_RENDERFPS; break;
    case ROW_ASPECT:    v = Display_Widescreen();     tbl = kAspectChoices; n = N_ASPECT;  break;
    case ROW_SGBBORDER: v = SgbBorder_IsEnabled();    tbl = kSgbBorderChoices; n = N_SGBBORDER; break;
    case ROW_NTSC:      v = NtscFilter_IsEnabled();   tbl = kNtscChoices;      n = N_NTSC;      break;
    case ROW_CRT:       v = (int)CrtRenderer_Profile(); tbl = kCrtChoices;       n = N_CRT;       break;
    case ROW_CRTCURVE:  v = (int)CrtRenderer_Curve();   tbl = kCrtCurveChoices;  n = N_CRTCURVE;  break;
    case ROW_PAL_S0R ... ROW_PAL_S3B: {
        int c[3];
        Display_GetCustomShade((row - ROW_PAL_FIRST) / 3, &c[0], &c[1], &c[2]);
        v = c[(row - ROW_PAL_FIRST) % 3];
        tbl = kHexChoices; n = N_HEX;
        break;
    }

    case ROW_WINSCALE:  v = Display_IsFullscreen() ? WINSCALE_FULLSCREEN
                                                   : Display_WindowScale();
                        tbl = kWinScaleChoices; n = N_WINSCALE; break;
    case ROW_LTEMP:     v = (int)(Display_LightTemperature() * 10.0f + (Display_LightTemperature() < 0 ? -0.5f : 0.5f));
                        tbl = kLightTempChoices; n = N_LTEMP; break;
    case ROW_FILTER: {
        const char *cur = DisplayGL_Filter();
        for (int i = 0; i < N_FILTER; i++)
            if (cur && strcmp(cur, kFilterFiles[i]) == 0) return i;
        return 0;
    }
    default: return 0;
    }
    for (int i = 0; i < n; i++) if (tbl[i].value == v) return i;
    return 0;
}

static int display_index(int row) {
    if (row == ROW_BATTLEALL && SpeedSettings_BattleIsCustom())
        return BATTLE_CUSTOM_INDEX;
    return current_index(row);
}

static int s_changed;

static int s_loading;

enum { NOTICE_NONE = 0, NOTICE_WIDESCREEN };
static int s_notice;
static int s_notice_dismiss;
static int s_wide_notice_seen;

static void refresh_overworld_gfx(void) {
    if (Game_GetScene() != 0) return;

    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);
    Display_LoadMapPalette();
    GbcColor_MarkDirty();

    if (Menu_IsOpen()) Menu_DrawBackdropForBag();
}

static void apply(int row, int index) {
    s_changed = 1;
    switch (row) {
    case ROW_COLOR:

        GbcColor_SetEnabled(kColorChoices[index].value ? 1 : 0);
        Gen1Color_SetEnabled(1);
        if (kColorChoices[index].value) GbcColor_MarkDirty();
        break;
    case ROW_CURVE:
        Display_SetColorCurve(kCurveChoices[index].value);
        break;
    case ROW_UI:
        Gen1Color_SetUiStyle(kUiChoices[index].value);
        break;
    case ROW_SPRITES:
        Gen1Color_SetSpriteStyle(kSpriteChoices[index].value);
        break;
    case ROW_PALETTE:
        if (kPaletteChoices[index].value == PAL_CHOICE_GBC_AUTO) {
            GbcColor_SetBattleAutoColor(1);
        } else {
            GbcColor_SetBattleAutoColor(0);
            Gen1Color_SetMonPalStyle(kPaletteChoices[index].value);
        }

        {
            int on_sgb = !GbcColor_BattleAutoColor() &&
                         Gen1Color_MonPalStyle() == G1C_MONPAL_SGB;
            Display_SetSgbFlashCompat(on_sgb);
            MoveAnim_SetOnSgb(on_sgb);
        }
        break;
    case ROW_FIELD:
        GbcColor_SetOverworldStyle(kFieldChoices[index].value);
        break;
    case ROW_CRIES:
        Audio_SetCryStyle(kCriesChoices[index].value);
        break;
    case ROW_ANIMSPD:
        SpeedSettings_SetMoveAnim(kAnimSpeedChoices[index].value);
        break;
    case ROW_HPBAR:
        SpeedSettings_SetHpBar(kHpBarChoices[index].value);
        break;
    case ROW_EXPSHARE:
        BattleExp_SetModernShare(kExpShareChoices[index].value);
        break;
    case ROW_FASTBOOT:
        s_fast_boot = kFastBootChoices[index].value;
        break;
    case ROW_OWSPD:
        SpeedSettings_SetOverworld(kOverworldSpeedChoices[index].value);
        break;
    case ROW_TEXTSPD:
        SpeedSettings_SetText(kTextSpeedChoices[index].value);
        break;
    case ROW_ROMSCROLL:
        SpeedSettings_SetRomTextScroll(kRomScrollChoices[index].value);
        break;
    case ROW_MISCSPD:
        SpeedSettings_SetMiscAnim(kMiscAnimChoices[index].value);
        break;
    case ROW_TRANSSPD:
        SpeedSettings_SetTransition(kTransitionChoices[index].value);
        break;
    case ROW_BATTLEALL:
        SpeedSettings_SetBattle(kBattleAllChoices[index].value);
        break;
    case ROW_VOLMASTER:
        Audio_SetMasterVolume(kVolumeChoices[index].value);
        break;
    case ROW_VOLMUSIC:
        Audio_SetMusicVolume(kVolumeChoices[index].value);
        break;
    case ROW_VOLSFX:
        Audio_SetSfxVolume(kVolumeChoices[index].value);
        break;
    case ROW_RENDERER:

        DisplayGL_SetRequested(kRendererChoices[index].value);
        Display_RequestBackendRestart();
        break;
    case ROW_FILTER:
        if (index >= 0 && index < N_FILTER)
            DisplayGL_SetFilter(kFilterFiles[index]);
        break;
    case ROW_BLEND:
        DisplayGL_SetBlending((display_gl_blend_t)kBlendChoices[index].value);
        break;
    case ROW_MONOPAL:
        Display_SetMonoPalette(kMonoPalChoices[index].value);
        break;
    case ROW_SCALE:
        DisplayGL_SetScaling((display_gl_scale_t)kScaleChoices[index].value);

        Display_ApplyScalingMode();
        break;
    case ROW_ASPECT:

        Display_SetWidescreen(kAspectChoices[index].value);

        if (!s_loading && kAspectChoices[index].value != 0 && !s_wide_notice_seen)
            s_notice = NOTICE_WIDESCREEN;
        break;
    case ROW_VSYNC:
        DisplayGL_SetVSync((display_gl_vsync_t)kVSyncChoices[index].value);
        break;
    case ROW_RENDERFPS:
        Display_SetRenderFPS(kRenderFpsChoices[index].value);
        break;
    case ROW_NTSC:
        NtscFilter_SetEnabled(kNtscChoices[index].value);
        break;
    case ROW_CRT:

        CrtRenderer_SetProfile((crt_profile_id_t)kCrtChoices[index].value);

        if (CrtRenderer_Profile() != CRT_PROFILE_OFF &&
            filter_is_tube_shader(current_index(ROW_FILTER)))
            DisplayGL_SetFilter(kFilterFiles[0]);
        break;
    case ROW_CRTCURVE:
        CrtRenderer_SetCurve((crt_curve_t)kCrtCurveChoices[index].value);
        break;
    case ROW_SGBBORDER:

        Display_SetSgbBorderLogicalSize(
            SgbBorder_SetEnabled(kSgbBorderChoices[index].value));

        Display_RefreshWindowScale();
        break;
    case ROW_PAL_S0R ... ROW_PAL_S3B: {

        const int shade = (row - ROW_PAL_FIRST) / 3;
        int c[3];
        Display_GetCustomShade(shade, &c[0], &c[1], &c[2]);
        c[(row - ROW_PAL_FIRST) % 3] = kHexChoices[index].value;
        Display_SetCustomShade(shade, c[0], c[1], c[2]);
        break;
    }
    case ROW_WINSCALE:
        if (kWinScaleChoices[index].value == WINSCALE_FULLSCREEN) {
            Display_SetFullscreen(1);
        } else {

            Display_SetFullscreen(0);
            Display_SetWindowScale(kWinScaleChoices[index].value);
        }
        break;
    case ROW_LTEMP:
        Display_SetLightTemperature(kLightTempChoices[index].value / 10.0f);
        break;
    default:
        break;
    }

    PresentationMenu_SaveSettings();
}

static const choice_t *row_table(int row) {
    switch (row) {
    case ROW_COLOR:     return kColorChoices;
    case ROW_CURVE:     return kCurveChoices;
    case ROW_UI:        return kUiChoices;
    case ROW_EXPSHARE:  return kExpShareChoices;
    case ROW_FASTBOOT:  return kFastBootChoices;
    case ROW_SPRITES:   return kSpriteChoices;
    case ROW_PALETTE:   return kPaletteChoices;
    case ROW_FIELD:     return kFieldChoices;
    case ROW_CRIES:     return kCriesChoices;
    case ROW_ANIMSPD:   return kAnimSpeedChoices;
    case ROW_HPBAR:     return kHpBarChoices;
    case ROW_OWSPD:     return kOverworldSpeedChoices;
    case ROW_TEXTSPD:   return kTextSpeedChoices;
    case ROW_ROMSCROLL: return kRomScrollChoices;
    case ROW_MISCSPD:   return kMiscAnimChoices;
    case ROW_TRANSSPD:  return kTransitionChoices;
    case ROW_BATTLEALL: return kBattleAllChoices;
    case ROW_VOLMASTER:
    case ROW_VOLMUSIC:
    case ROW_VOLSFX:    return kVolumeChoices;
    case ROW_RENDERER:  return kRendererChoices;
    case ROW_FILTER:    return kFilterChoices;
    case ROW_BLEND:     return kBlendChoices;
    case ROW_MONOPAL:   return kMonoPalChoices;
    case ROW_SCALE:     return kScaleChoices;
    case ROW_VSYNC:     return kVSyncChoices;
    case ROW_RENDERFPS: return kRenderFpsChoices;
    case ROW_ASPECT:    return kAspectChoices;
    case ROW_SGBBORDER: return kSgbBorderChoices;
    case ROW_NTSC:      return kNtscChoices;
    case ROW_CRT:       return kCrtChoices;
    case ROW_CRTCURVE:  return kCrtCurveChoices;
    case ROW_PAL_S0R ... ROW_PAL_S3B: return kHexChoices;
    case ROW_WINSCALE:  return kWinScaleChoices;
    case ROW_LTEMP:     return kLightTempChoices;
    default:            return kColorChoices;
    }
}

static int row_count(int row) {
    switch (row) {
    case ROW_COLOR:   return N_COLOR;
    case ROW_CURVE:   return N_CURVE;
    case ROW_UI:      return N_UI;
    case ROW_EXPSHARE: return N_EXPSHARE;
    case ROW_FASTBOOT: return N_FASTBOOT;
    case ROW_SPRITES: return N_SPRITE;
    case ROW_PALETTE: return N_PALETTE;
    case ROW_FIELD:   return N_FIELD;
    case ROW_CRIES:   return N_CRIES;
    case ROW_ANIMSPD: return N_ANIMSPD;
    case ROW_HPBAR:   return N_HPBAR;
    case ROW_OWSPD:   return N_OWSPD;
    case ROW_TEXTSPD: return N_TEXTSPD;
    case ROW_ROMSCROLL: return N_ROMSCROLL;
    case ROW_MISCSPD: return N_MISCSPD;
    case ROW_TRANSSPD: return N_TRANS;
    case ROW_BATTLEALL: return N_BATTLE;
    case ROW_VOLMASTER:
    case ROW_VOLMUSIC:
    case ROW_VOLSFX:  return N_VOLUME;
    case ROW_RENDERER: return N_RENDER;
    case ROW_FILTER:   return N_FILTER;
    case ROW_BLEND:    return N_BLEND;
    case ROW_MONOPAL:  return N_MONOPAL;
    case ROW_SCALE:    return N_SCALE;
    case ROW_VSYNC:    return N_VSYNC;
    case ROW_RENDERFPS: return N_RENDERFPS;
    case ROW_ASPECT:   return N_ASPECT;
    case ROW_SGBBORDER: return N_SGBBORDER;
    case ROW_NTSC:     return N_NTSC;
    case ROW_CRT:      return N_CRT;
    case ROW_CRTCURVE: return N_CRTCURVE;
    case ROW_PAL_S0R ... ROW_PAL_S3B: return N_HEX;
    case ROW_WINSCALE: return N_WINSCALE;
    case ROW_LTEMP:    return N_LTEMP;
    default:          return 1;
    }
}

static const char *presentation_cfg_path(void) {
    static char path[1200];
    return UserDataPath("presentation.cfg", path, sizeof path) ? path : "presentation.cfg";
}
#define PRESENTATION_CFG presentation_cfg_path()

static const struct { int row; const char *key; } kPersistRows[] = {
    { ROW_COLOR,    "color"       },
    { ROW_UI,       "ui"          },
    { ROW_SPRITES,  "sprites"     },
    { ROW_PALETTE,  "palette"     },
    { ROW_FIELD,    "field"       },

    { ROW_CURVE,    "curve"       },
    { ROW_CRIES,    "cries"       },
    { ROW_ANIMSPD,  "anim_speed"  },
    { ROW_HPBAR,    "hpbar"       },
    { ROW_OWSPD,    "ow_speed"    },
    { ROW_EXPSHARE, "exp_share"   },
    { ROW_FASTBOOT, "fast_boot"   },
    { ROW_TEXTSPD,  "text_speed"  },
    { ROW_ROMSCROLL, "rom_text_scroll" },
    { ROW_MISCSPD,  "misc_speed"  },
    { ROW_TRANSSPD, "trans_speed" },
    { ROW_VOLMASTER, "vol_master" },
    { ROW_VOLMUSIC,  "vol_music"  },
    { ROW_VOLSFX,    "vol_sfx"    },
    { ROW_RENDERER,  "renderer"   },
    { ROW_FILTER,    "filter"     },
    { ROW_BLEND,     "ghosting"   },
    { ROW_MONOPAL,   "mono_pal"   },
    { ROW_SCALE,     "gl_scaling" },
    { ROW_VSYNC,     "vsync"      },
    { ROW_RENDERFPS, "render_fps" },
    { ROW_ASPECT,    "aspect"     },
    { ROW_SGBBORDER, "sgb_border" },
    { ROW_NTSC,      "composite"  },
    { ROW_CRT,       "crt_profile" },
    { ROW_CRTCURVE,  "crt_curve"   },
    { ROW_PAL_S0R,   "pal_s0r"    }, { ROW_PAL_S0G, "pal_s0g" }, { ROW_PAL_S0B, "pal_s0b" },
    { ROW_PAL_S1R,   "pal_s1r"    }, { ROW_PAL_S1G, "pal_s1g" }, { ROW_PAL_S1B, "pal_s1b" },
    { ROW_PAL_S2R,   "pal_s2r"    }, { ROW_PAL_S2G, "pal_s2g" }, { ROW_PAL_S2B, "pal_s2b" },
    { ROW_PAL_S3R,   "pal_s3r"    }, { ROW_PAL_S3G, "pal_s3g" }, { ROW_PAL_S3B, "pal_s3b" },
    { ROW_WINSCALE,  "win_scale"  },
    { ROW_LTEMP,     "light_temp" },
};
#define N_PERSIST ((int)(sizeof kPersistRows / sizeof kPersistRows[0]))

static void PresentationMenu_SaveSettings(void) {
    FILE *f;
    if (s_loading) return;
    f = fopen(PRESENTATION_CFG, "w");
    if (!f) return;
    fprintf(f, "# oldamber presentation settings -- rewritten on every change\n");
    for (int i = 0; i < N_PERSIST; i++) {
        int row = kPersistRows[i].row;
        fprintf(f, "%s %d\n", kPersistRows[i].key,
                row_table(row)[current_index(row)].value);
    }

    fprintf(f, "wide_notice_seen %d\n", s_wide_notice_seen);
    fclose(f);
}

void PresentationMenu_PreloadRenderer(void) {
    FILE *f = fopen(PRESENTATION_CFG, "r");
    char line[128];
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char key[32];
        int val;
        if (sscanf(line, "%31s %d", key, &val) != 2) continue;
        if (strcmp(key, "renderer") == 0) {
            DisplayGL_SetRequested(val != 0);
            break;
        }
    }
    fclose(f);
}

void PresentationMenu_LoadSettings(void) {
    FILE *f = fopen(PRESENTATION_CFG, "r");
    char line[128];
    int was_changed = s_changed;
    if (!f) return;
    s_loading = 1;
    while (fgets(line, sizeof line, f)) {
        char key[32];
        int  val;
        if (sscanf(line, "%31s %d", key, &val) != 2) continue;
        if (key[0] == '#') continue;

        if (strcmp(key, "wide_notice_seen") == 0) { s_wide_notice_seen = (val != 0); continue; }
        for (int i = 0; i < N_PERSIST; i++) {
            int row, n;
            const choice_t *tbl;
            if (strcmp(key, kPersistRows[i].key) != 0) continue;
            row = kPersistRows[i].row;
            tbl = row_table(row);
            n   = row_count(row);
            for (int j = 0; j < n; j++) {
                if (tbl[j].value == val) { apply(row, j); break; }
            }
            break;
        }
    }
    fclose(f);
    s_loading = 0;

    s_changed = was_changed;
}

static int s_row;

static int row_needs_opengl(int setting) {

    return setting == ROW_CRT     || setting == ROW_CRTCURVE ||
           setting == ROW_FILTER  || setting == ROW_BLEND    ||
           setting == ROW_VSYNC;
}

static int row_is_available(int setting) {
    if (!row_needs_opengl(setting)) return 1;
    return DisplayGL_IsActive();
}

static const char *row_value_text(const menu_row_t *r) {
    if (!row_is_available(r->setting)) return "OPENGL ONLY";

    if (r->setting == ROW_WINSCALE && !Display_IsFullscreen() &&
        !Display_WindowScaleApplies())
        return "LOCKED";
    return r->tbl[display_index(r->setting)].label;
}

static void draw_row_1line(const menu_row_t *r) {
    const char *label = r->label;
    const int y = r->y;
    const int x = INNER_L + 1 + r->indent;
    const int w = r->label_w;
    char buf[LABEL_W_MAX + 1];
    int len = (int)strlen(label);
    int i;
    if (len > w) len = w;
    for (i = 0; i < len; i++) buf[i] = label[i];
    for (; i < w; i++) buf[i] = ' ';
    buf[w] = '\0';

    put_marker(INNER_L, y, (s_row == r - page()->rows) ? CHAR_FILLED : CHAR_SPACE);
    put(x, y, buf);
    clear_row(x + w, INNER_R, y);

    put_marker(x + w, y, CHAR_HOLLOW);
    put_value(x + w + 1, INNER_R, y, row_value_text(r));
}

static void draw_row_2line(const menu_row_t *r) {
    const int y = r->y;
    const int x = INNER_L + 1 + r->indent;

    put_marker(INNER_L, y, (s_row == r - page()->rows) ? CHAR_FILLED : CHAR_SPACE);
    clear_row(x, INNER_R, y);
    put(x, y, r->label);

    clear_row(INNER_L, INNER_R, y + 1);
    put_marker(x + 1, y + 1, CHAR_HOLLOW);
    put_value(x + 2, INNER_R, y + 1, row_value_text(r));
}

static void draw_row_submenu(const menu_row_t *r) {
    const int x = INNER_L + 1 + r->indent;
    put_marker(INNER_L, r->y, (s_row == r - page()->rows) ? CHAR_FILLED : CHAR_SPACE);
    clear_row(x, INNER_R, r->y);
    put(x, r->y, r->label);

}

static void draw_row_back(const menu_row_t *r) {
    put(2, r->y, r->label);
    put_marker(1, r->y, (s_row == r - page()->rows) ? CHAR_FILLED : CHAR_HOLLOW);
}

static void draw_row(const menu_row_t *r) {
    switch (r->kind) {
    case RK_SUBMENU: draw_row_submenu(r); break;
    case RK_BACK:    draw_row_back(r);    break;
    default:         if (r->two_line) draw_row_2line(r); else draw_row_1line(r); break;
    }
}

static int s_open;
static uint8_t     s_map_save[SCROLL_MAP_W * SCROLL_MAP_H];
static uint8_t     s_win_save[SCREEN_HEIGHT][SCREEN_WIDTH];
static oam_entry_t s_oam_save[MAX_SPRITES];

static void draw_all(void) {

    if (s_page == PAGE_MAIN) {
        memcpy(gScrollTileMap, s_map_save, sizeof s_map_save);
    } else {
        for (int r = 0; r < SCREEN_HEIGHT; r++)
            for (int c = 0; c < SCREEN_WIDTH; c++)
                smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
    }

    draw_box(page()->box_l, BOX_TOP, page()->box_r, page()->box_bottom);
    for (int i = 0; i < page()->n_headers; i++)
        put(INNER_L, page()->headers[i].y, page()->headers[i].text);
    for (int i = 0; i < page()->n_rows; i++)
        draw_row(&page()->rows[i]);
}

void PresentationMenu_Open(void) {
    if (s_open) return;
    memcpy(s_map_save, gScrollTileMap, sizeof s_map_save);
    memcpy(s_win_save, gWindowTileMap, sizeof s_win_save);
    memcpy(s_oam_save, wShadowOAM, sizeof s_oam_save);

    memset(gWindowTileMap, 0, sizeof s_win_save);
    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;

    s_row = 0;
    s_return_row = 0;
    s_page = PAGE_MAIN;
    s_changed = 0;
    s_open = 1;
    draw_all();
}

void PresentationMenu_Close(void) {
    if (!s_open) return;
    s_open = 0;
    memcpy(gScrollTileMap, s_map_save, sizeof s_map_save);
    memcpy(gWindowTileMap, s_win_save, sizeof s_win_save);
    memcpy(wShadowOAM, s_oam_save, sizeof s_oam_save);

    if (s_changed) refresh_overworld_gfx();
    s_changed = 0;
}

void PresentationMenu_Toggle(void) {
    if (s_open) PresentationMenu_Close();
    else        PresentationMenu_Open();
}

int PresentationMenu_IsOpen(void) { return s_open; }

static void draw_notice(void) {
    if (s_notice != NOTICE_WIDESCREEN) return;

    draw_box(0, 2, 19, 16);

    put(2,  4, "MODERN ASPECT");
    put(2,  5, "RATIOS ARE");
    put(2,  6, "EXPERIMENTAL,");
    put(2,  7, "SOME THINGS MAY");
    put(2,  8, "LOOK OFF");
    put(2,  9, "PRESENTATION");
    put(2, 10, "WISE, FOR THE");
    put(2, 11, "TIME BEING.");

    put(2, 13, "DO NOT SHOW AGAIN");

    put(6, 14, s_notice_dismiss ? "< YES >" : "< NO  >");

    put(2, 15, "A  CLOSE");
}

void PresentationMenu_Tick(void) {
    if (!s_open) return;

    const int n_rows = page()->n_rows;
    const menu_row_t *row = &page()->rows[s_row];

    if (s_notice != NOTICE_NONE) {
        if (hJoyPressed & (PAD_LEFT | PAD_RIGHT)) {
            s_notice_dismiss = !s_notice_dismiss;
            Audio_PlaySFX_PressAB();
        } else if (hJoyPressed & (PAD_A | PAD_B | PAD_START)) {
            Audio_PlaySFX_PressAB();
            if (s_notice == NOTICE_WIDESCREEN && s_notice_dismiss) {
                s_wide_notice_seen = 1;
                PresentationMenu_SaveSettings();
            }
            s_notice = NOTICE_NONE;
            s_notice_dismiss = 0;
        }
        for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
        memset(gWindowTileMap, 0, sizeof gWindowTileMap);
        s_marquee++;
        draw_all();
        draw_notice();
        return;
    }

    if (hJoyPressed & PAD_DOWN) {
        s_row = (s_row + 1) % n_rows;
    } else if (hJoyPressed & PAD_UP) {
        s_row = (s_row + n_rows - 1) % n_rows;
    } else if (hJoyPressed & (PAD_LEFT | PAD_RIGHT)) {
        if (row->kind == RK_VALUE && !row_is_available(row->setting)) {

            Audio_PlaySFX_PressAB();
        } else if (row->kind == RK_VALUE) {
            int n    = row_count(row->setting);
            int cur  = current_index(row->setting);
            int step = (hJoyPressed & PAD_RIGHT) ? 1 : -1;
            int i    = cur + step;

            if (row->setting == ROW_FILTER)
                while (i >= 0 && i < n && filter_blocked(i)) i += step;

            if (i < 0 || i > n - 1) i = cur;
            apply(row->setting, i);
        }
    } else if (hJoyPressed & PAD_A) {
        if (row->kind == RK_SUBMENU) {
            Audio_PlaySFX_PressAB();
            s_return_row = s_row;
            s_page = row->setting;
            s_row = 0;
        } else if (row->kind == RK_BACK) {
            Audio_PlaySFX_PressAB();
            if (s_page != PAGE_MAIN) { s_page = PAGE_MAIN; s_row = s_return_row; }
            else                     { PresentationMenu_Close(); return; }
        }
    } else if (hJoyPressed & (PAD_B | PAD_START)) {
        Audio_PlaySFX_PressAB();

        if (s_page != PAGE_MAIN) { s_page = PAGE_MAIN; s_row = s_return_row; }
        else                     { PresentationMenu_Close(); return; }
    }

    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    memset(gWindowTileMap, 0, sizeof gWindowTileMap);
    s_marquee++;
    draw_all();

    draw_notice();
}

static const char *kPageNames[] = {
    "OPTIONS", "GRAPHICS", "SPEED", "AUDIO", "DISPLAY", "PALETTE", "GAMEPLAY",
};
typedef char page_names_cover_every_page[
    ((int)(sizeof kPageNames / sizeof kPageNames[0]) == PAGE_GAMEPLAY + 1) ? 1 : -1];

int PresentationMenu_PageCount(void) {
    return (int)(sizeof kPages / sizeof kPages[0]);
}

const char *PresentationMenu_PageName(int pg) {
    if (pg < 0 || pg >= PresentationMenu_PageCount()) return "";
    return kPageNames[pg];
}

static const menu_row_t *value_row(int pg, int i) {
    if (pg < 0 || pg >= PresentationMenu_PageCount() || i < 0) return NULL;
    const menu_page_t *pp = &kPages[pg];
    for (int k = 0; k < pp->n_rows; k++) {
        if (pp->rows[k].kind != RK_VALUE) continue;
        if (i-- == 0) return &pp->rows[k];
    }
    return NULL;
}

void PresentationMenu_RefreshNow(void) {
    if (!s_changed) return;
    refresh_overworld_gfx();
    s_changed = 0;
}

int PresentationMenu_PageRowCount(int pg) {
    int n = 0;
    if (pg < 0 || pg >= PresentationMenu_PageCount()) return 0;
    for (int k = 0; k < kPages[pg].n_rows; k++)
        if (kPages[pg].rows[k].kind == RK_VALUE) n++;
    return n;
}

const char *PresentationMenu_RowHeader(int pg, int i) {
    const menu_page_t *p;
    const menu_row_t *cur, *prev;
    int h;
    if (pg < 0 || pg >= PresentationMenu_PageCount()) return NULL;
    p = &kPages[pg];
    if (!p->headers || p->n_headers <= 0) return NULL;
    cur = value_row(pg, i);
    if (!cur) return NULL;
    prev = (i > 0) ? value_row(pg, i - 1) : NULL;
    for (h = 0; h < p->n_headers; h++) {
        int hy = p->headers[h].y;
        if (hy <= cur->y && (!prev || hy > prev->y)) return p->headers[h].text;
    }
    return NULL;
}

const char *PresentationMenu_RowLabel(int pg, int i) {
    const menu_row_t *rr = value_row(pg, i);
    return rr ? rr->label : "";
}

int PresentationMenu_RowId(int pg, int i) {
    const menu_row_t *rr = value_row(pg, i);
    return rr ? (int)rr->setting : -1;
}

int PresentationMenu_RowAvailable(int row_id) {
    return row_is_available(row_id);
}

int PresentationMenu_ChoiceCount(int row_id) {
    if (row_id < 0) return 0;
    return row_count(row_id);
}

const char *PresentationMenu_ChoiceLabel(int row_id, int index) {
    const choice_t *tbl;
    if (row_id < 0 || index < 0 || index >= row_count(row_id)) return "";
    tbl = row_table(row_id);
    return tbl ? tbl[index].label : "";
}

int PresentationMenu_CurrentIndex(int row_id) {
    if (row_id < 0) return 0;
    return current_index(row_id);
}

void PresentationMenu_SetIndex(int row_id, int index) {
    if (row_id < 0 || index < 0 || index >= row_count(row_id)) return;
    apply(row_id, index);
    PresentationMenu_SaveSettings();
}

int PresentationMenu_TakeWidescreenNotice(void) {
    if (s_notice != NOTICE_WIDESCREEN) return 0;
    s_notice = NOTICE_NONE;
    return 1;
}

void PresentationMenu_DismissWidescreenNotice(int never_again) {
    if (!never_again) return;
    s_wide_notice_seen = 1;
    PresentationMenu_SaveSettings();
}
