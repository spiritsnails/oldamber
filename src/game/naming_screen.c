#include "naming_screen.h"
#include "assetpack_bind.h"
#include "constants.h"
#include "pokemon.h"
#include "overworld.h"
#include "player.h"
#include "../data/font_data.h"
#include "../data/base_stats.h"
#include "npc.h"
#include "gbc_color.h"
#include "../data/gbc_palettes.h"
#include "../platform/display.h"
#include "../platform/hardware.h"

#include <string.h>

#define CHAR_TERM   0x50
#define CHAR_SPACE  0x7F
#define CHAR_CURSOR 0xED

#define BOX_L 0
#define BOX_T 4
#define BOX_INNER_W 18
#define BOX_INNER_H 9
#define BOX_R (BOX_L + 1 + BOX_INNER_W)
#define BOX_B (BOX_T + 1 + BOX_INNER_H)

#define ALPHA_X 2
#define ALPHA_Y 5
#define ALPHA_ROW_STEP 2

static int s_open = 0;

#define NAMING_WHITEOUT_HOLD 30
static int s_transition = 0;
static int s_transition_timer = 0;
static uint8_t s_type = NAME_MON_SCREEN;
static uint8_t s_species = 0;
static uint8_t *s_dst = NULL;
static uint8_t s_work[NAME_LENGTH];
static uint8_t s_case = 0;
static int s_row = 0;
static int s_col = 0;
static oam_entry_t s_oam_backup[MAX_SPRITES];
static uint8_t s_saved_ed_tile[TILE_SIZE];
static int s_ed_tile_overridden = 0;
static uint8_t s_saved_underscore_tile[TILE_SIZE];
static uint8_t s_saved_raised_underscore_tile[TILE_SIZE];
static int s_name_underscore_tiles_overridden = 0;

static const uint8_t kEdTile[TILE_SIZE] = {
    0xF0, 0xF0, 0xC0, 0xC0, 0xF0, 0xF0, 0xCE, 0xCE,
    0xFD, 0xFD, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E,
};

static const uint8_t kUnderscoreTile[TILE_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE,
    0xFE, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t kAlphaUpper[5][9] = {
    {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88},
    {0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91},
    {0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,CHAR_SPACE},
    {0xF1,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xE1,0xE2},
    {0xE3,0xE6,0xE7,0xEF,0xF5,0xF3,0xF2,0xF4,0xF0},
};

static const uint8_t kAlphaLower[5][9] = {
    {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8},
    {0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1},
    {0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,CHAR_SPACE},
    {0xF1,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xE1,0xE2},
    {0xE3,0xE6,0xE7,0xEF,0xF5,0xF3,0xF2,0xF4,0xF0},
};

static int max_len(void) {
    return (s_type >= NAME_MON_SCREEN) ? (NAME_LENGTH - 1) : (PLAYER_NAME_LENGTH - 1);
}

static int ascii_to_gb(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return 0x80 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0xA0 + (c - 'a');
    if (c >= '0' && c <= '9') return 0xF6 + (c - '0');
    if (c == ' ') return CHAR_SPACE;
    if (c == '.') return 0xE8;
    if (c == '!') return 0xE7;
    if (c == '?') return 0xE6;
    if (c == '\'') return 0xE0;
    if (c == '-') return 0xE3;
    if (c == ':') return 0x9C;
    return CHAR_SPACE;
}

static int gb_strlen(const uint8_t *s) {
    int n = 0;
    while (n < NAME_LENGTH && s[n] != CHAR_TERM) n++;
    return n;
}

static void enforce_cursor_for_length(void) {
    int len = gb_strlen(s_work);
    int max = max_len();

    if (len >= max) {
        s_row = 4;
        s_col = 8;
    }
}

static void put_tile(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void put_gb(int col, int row, uint8_t ch) {
    put_tile(col, row, (uint8_t)Font_CharToTile(ch));
}

static void clear_screen(void) {

    for (int r = 0; r < SCROLL_MAP_H; r++) {
        for (int c = 0; c < SCROLL_MAP_W; c++) {
            gScrollTileMap[r * SCROLL_MAP_W + c] = (uint8_t)BLANK_TILE_SLOT;
        }
    }
}

static void draw_box(void) {
    put_gb(BOX_L, BOX_T, 0x79);
    for (int c = 0; c < BOX_INNER_W; c++) put_gb(BOX_L + 1 + c, BOX_T, 0x7A);
    put_gb(BOX_R, BOX_T, 0x7B);
    for (int r = 0; r < BOX_INNER_H; r++) {
        int y = BOX_T + 1 + r;
        put_gb(BOX_L, y, 0x7C);
        for (int c = 0; c < BOX_INNER_W; c++) put_gb(BOX_L + 1 + c, y, CHAR_SPACE);
        put_gb(BOX_R, y, 0x7C);
    }
    put_gb(BOX_L, BOX_B, 0x7D);
    for (int c = 0; c < BOX_INNER_W; c++) put_gb(BOX_L + 1 + c, BOX_B, 0x7A);
    put_gb(BOX_R, BOX_B, 0x7E);
}

static void put_ascii(int col, int row, const char *s) {
    while (*s) {
        put_gb(col++, row, (uint8_t)ascii_to_gb((unsigned char)*s));
        s++;
    }
}

static void put_species_name(int col, int row) {
    uint8_t dex = gSpeciesToDex[s_species];
    const char *name = Pokemon_GetName(dex);
    if (!name) return;
    while (*name) {
        put_gb(col++, row, (uint8_t)ascii_to_gb((unsigned char)*name));
        name++;
    }
}

static void draw_prompt(void) {
    if (s_type == NAME_PLAYER_SCREEN) {
        put_ascii(0, 1, "YOUR NAME?");
        return;
    }
    if (s_type == NAME_RIVAL_SCREEN) {
        put_ascii(0, 1, "RIVAL'S NAME?");
        return;
    }
    put_species_name(4, 1);
    put_ascii(1, 3, "NICKNAME?");
}

static void draw_alphabet(void) {
    const uint8_t (*alpha)[9] = s_case ? kAlphaLower : kAlphaUpper;
    for (int r = 0; r < 5; r++) {
        int y = ALPHA_Y + r * ALPHA_ROW_STEP;
        for (int c = 0; c < 9; c++) {
            int x = ALPHA_X + c * 2;
            put_gb(x, y, alpha[r][c]);

            if (c < 8) put_gb(x + 1, y, CHAR_SPACE);
        }
    }

    int case_y = ALPHA_Y + 5 * ALPHA_ROW_STEP;
    for (int c = 0; c < 17; c++) put_gb(ALPHA_X + c, case_y, CHAR_SPACE);
    if (s_case) {
        put_ascii(ALPHA_X, case_y, "UPPER CASE");
    } else {
        put_ascii(ALPHA_X, case_y, "lower case");
    }
}

static void override_ed_tile(void) {
    uint8_t slot = (uint8_t)Font_CharToTile(0xF0);
    if (!s_ed_tile_overridden) {
        Display_GetTile(slot, s_saved_ed_tile);
        s_ed_tile_overridden = 1;
    }
    Display_LoadTile(slot, kEdTile);
}

static void restore_ed_tile(void) {
    if (!s_ed_tile_overridden) return;
    Display_LoadTile((uint8_t)Font_CharToTile(0xF0), s_saved_ed_tile);
    s_ed_tile_overridden = 0;
}

static void override_name_underscore_tiles(void) {
    uint8_t us_slot = (uint8_t)Font_CharToTile(0x76);
    uint8_t raised_slot = (uint8_t)Font_CharToTile(0x77);
    if (!s_name_underscore_tiles_overridden) {
        Display_GetTile(us_slot, s_saved_underscore_tile);
        Display_GetTile(raised_slot, s_saved_raised_underscore_tile);
        s_name_underscore_tiles_overridden = 1;
    }
    Display_LoadTile(us_slot, kUnderscoreTile);
    Display_LoadTile(raised_slot, kRaisedUnderscoreTile);
}

static void restore_name_underscore_tiles(void) {
    uint8_t us_slot = (uint8_t)Font_CharToTile(0x76);
    uint8_t raised_slot = (uint8_t)Font_CharToTile(0x77);
    if (!s_name_underscore_tiles_overridden) return;
    Display_LoadTile(us_slot, s_saved_underscore_tile);
    Display_LoadTile(raised_slot, s_saved_raised_underscore_tile);
    s_name_underscore_tiles_overridden = 0;
}

static void draw_name_line(void) {
    int len = gb_strlen(s_work);
    int max = max_len();
    for (int c = 10; c < 20; c++) put_gb(c, 2, CHAR_SPACE);
    for (int i = 0; i < max && i < 10; i++) put_gb(10 + i, 3, 0x76);
    for (int i = 0; i < len && i < 10; i++) put_gb(10 + i, 2, s_work[i]);
    if (len >= max) {
        put_gb(10 + (max - 1), 3, 0x77);
    } else if (len < 10) {
        put_gb(10 + len, 3, 0x77);
    }
}

static void draw_cursor(void) {

    for (int r = 0; r < 5; r++) {
        int y = ALPHA_Y + r * ALPHA_ROW_STEP;
        for (int c = 0; c < 9; c++) {
            int x = ALPHA_X + c * 2;
            put_gb(x - 1, y, CHAR_SPACE);
        }
    }
    put_gb(ALPHA_X - 1, ALPHA_Y + 5 * ALPHA_ROW_STEP, CHAR_SPACE);

    for (int r = 0; r < 5; r++) {
        int y = ALPHA_Y + r * ALPHA_ROW_STEP;
        for (int c = 0; c < 9; c++) {
            int x = ALPHA_X + c * 2;
            if ((r == s_row) && (c == s_col))
                put_gb(x - 1, y, CHAR_CURSOR);
        }
    }
    if (s_row == 5) put_gb(ALPHA_X - 1, ALPHA_Y + 5 * ALPHA_ROW_STEP, CHAR_CURSOR);
}

static void redraw(void) {
    enforce_cursor_for_length();
    clear_screen();
    draw_box();
    draw_prompt();
    draw_alphabet();
    draw_name_line();
    draw_cursor();
}

static void restore_caller_screen(void);

static void finalize_close(void) {

    if (!s_dst) {
        memcpy(wShadowOAM, s_oam_backup, sizeof(s_oam_backup));
        restore_name_underscore_tiles();
        restore_ed_tile();
        s_open = 0;
        return;
    }

    if (s_work[0] == CHAR_TERM) {
        clear_screen();
        memcpy(wShadowOAM, s_oam_backup, sizeof(s_oam_backup));
        restore_name_underscore_tiles();
        restore_ed_tile();
        s_open = 0;
        restore_caller_screen();
        return;
    }
    memcpy(s_dst, s_work, NAME_LENGTH);
    clear_screen();
    memcpy(wShadowOAM, s_oam_backup, sizeof(s_oam_backup));
    restore_name_underscore_tiles();
    restore_ed_tile();
    s_open = 0;
    restore_caller_screen();
}

static void restore_caller_screen(void) {
    if (s_type != NAME_MON_SCREEN) {
        Display_SetPalette(0xE4, 0xD0, 0xE0);
        return;
    }
    hWY = SCREEN_HEIGHT_PX;
    Display_LoadMapPalette();
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();

    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void submit_and_close(void) {

    Display_SetPalette(0, 0, 0);
    s_transition = 2;
    s_transition_timer = NAMING_WHITEOUT_HOLD;
}

static void handle_add_char(uint8_t ch) {
    int len = gb_strlen(s_work);
    if (len >= max_len() || len >= NAME_LENGTH - 1) return;
    s_work[len] = ch;
    s_work[len + 1] = CHAR_TERM;
}

static void naming_apply_palettes(void) {
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

void NamingScreen_Open(uint8_t type, uint8_t species, uint8_t *name_buf) {
    if (!name_buf) return;
    memcpy(s_oam_backup, wShadowOAM, sizeof(s_oam_backup));
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
    gScrollPxX = 0;
    gScrollPxY = 0;
    hWY = SCREEN_HEIGHT_PX;
    s_open = 1;
    s_type = type;
    s_species = species;
    s_dst = name_buf;

    s_work[0] = CHAR_TERM;
    s_case = 0;
    s_row = 0;
    s_col = 0;
    override_name_underscore_tiles();
    override_ed_tile();

    naming_apply_palettes();
    Display_SetPalette(0, 0, 0);
    s_transition = 1;
    s_transition_timer = NAMING_WHITEOUT_HOLD;
}

int NamingScreen_IsOpen(void) {
    return s_open;
}

void NamingScreen_Tick(void) {
    if (!s_open) return;
    if (s_transition) {

        if (--s_transition_timer > 0) return;
        int was_opening = (s_transition == 1);
        s_transition = 0;
        if (was_opening) {
            redraw();

            Display_SetPalette(0xE4, 0xD0, 0xE0);
        } else {
            finalize_close();
        }
        return;
    }
    gScrollPxX = 0;
    gScrollPxY = 0;
    hWY = SCREEN_HEIGHT_PX;

    if (hJoyPressed & PAD_START) {
        submit_and_close();
        return;
    }
    if (hJoyPressed & PAD_SELECT) {
        s_case ^= 1;
        draw_alphabet();
        draw_cursor();
        return;
    }
    if (hJoyPressed & PAD_B) {
        int len = gb_strlen(s_work);
        if (len > 0) {
            s_work[len - 1] = CHAR_TERM;
            if (s_row == 4 && s_col == 8 && gb_strlen(s_work) < max_len()) {
                s_col = 0;
            }
            draw_name_line();
            enforce_cursor_for_length();
            draw_cursor();
        }
        return;
    }
    if (hJoyPressed & PAD_UP) {
        s_row--;
        if (s_row < 0) {
            s_row = 5;
            s_col = 0;
        }
        if (s_row == 5) s_col = 0;
        draw_cursor();
        return;
    }
    if (hJoyPressed & PAD_DOWN) {
        s_row++;
        if (s_row > 5) {
            s_row = 0;
            s_col = 0;
        }
        if (s_row == 5) s_col = 0;
        draw_cursor();
        return;
    }
    if (hJoyPressed & PAD_LEFT) {
        if (s_row < 5) {
            s_col--;
            if (s_col < 0) s_col = 8;
            draw_cursor();
        }
        return;
    }
    if (hJoyPressed & PAD_RIGHT) {
        if (s_row < 5) {
            s_col++;
            if (s_col > 8) s_col = 0;
            draw_cursor();
        }
        return;
    }
    if (hJoyPressed & PAD_A) {
        if (s_row == 4 && s_col == 8) {
            submit_and_close();
            return;
        }
        if (s_row == 5 && s_col == 0) {
            s_case ^= 1;
            draw_alphabet();
            draw_cursor();
            return;
        }
        if (s_row < 5) {
            const uint8_t (*alpha)[9] = s_case ? kAlphaLower : kAlphaUpper;
            handle_add_char(alpha[s_row][s_col]);
            draw_name_line();
            enforce_cursor_for_length();
            draw_cursor();
        }
    }
}
