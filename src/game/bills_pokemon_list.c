
#include "bills_pokemon_list.h"
#include "text.h"
#include "rom_text.h"
#include "npc.h"
#include "player.h"
#include "overworld.h"
#include "pokedex.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/font_data.h"

#define BC_TL  0x79u
#define BC_H   0x7Au
#define BC_TR  0x7Bu
#define BC_V   0x7Cu
#define BC_BL  0x7Du
#define BC_BR  0x7Eu
#define BC_SP  0x7Fu

#define BHPC_MENU_L  0
#define BHPC_MENU_T  0
#define BHPC_MENU_R 10
#define BHPC_MENU_B 11

static const int kDexList[4] = {133, 136, 135, 134};

#define kText_PokemonList (RomText("BillsHousePokemonListText1"))

static const char *kText_WhichPokemonL1 = "Which MONSTER do";
static const char *kText_WhichPokemonL2 = "you want to see?";

typedef enum {
    BPL_CLOSED = 0,
    BPL_TEXT1,
    BPL_MENU,
    BPL_DATA,
} BPLState;

static BPLState s_state       = BPL_CLOSED;
static int      s_cursor      = 0;
static int      s_shown_dex   = 0;

static void bpl_set(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static uint8_t bpl_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == ' ') return (uint8_t)Font_CharToTile(BC_SP);
    if (c == '\'') return (uint8_t)Font_CharToTile(0xE0);
    if (c == '!') return (uint8_t)Font_CharToTile(0xE7);
    if (c == '-') return (uint8_t)Font_CharToTile(0xE3);
    if (c == '?') return (uint8_t)Font_CharToTile(0xE6);
    if (c == '>') return (uint8_t)Font_CharToTile(0xED);
    return (uint8_t)Font_CharToTile(BC_SP);
}

static void bpl_str(int col, int row, const char *s) {
    for (; *s; s++, col++) {
        if (*s == '#') {

            bpl_set(col++, row, (uint8_t)Font_CharToTile(0x8F));
            bpl_set(col++, row, (uint8_t)Font_CharToTile(0x8E));
            bpl_set(col++, row, (uint8_t)Font_CharToTile(0x8A));
            bpl_set(col,   row, (uint8_t)Font_CharToTile(0xBA));
            continue;
        }
        bpl_set(col, row, bpl_tile((unsigned char)*s));
    }
}

static void bpl_box(int l, int t, int r, int b) {
    bpl_set(l, t, (uint8_t)Font_CharToTile(BC_TL));
    for (int c = l + 1; c < r; c++) bpl_set(c, t, (uint8_t)Font_CharToTile(BC_H));
    bpl_set(r, t, (uint8_t)Font_CharToTile(BC_TR));
    for (int y = t + 1; y < b; y++) {
        bpl_set(l, y, (uint8_t)Font_CharToTile(BC_V));
        for (int c = l + 1; c < r; c++) bpl_set(c, y, (uint8_t)Font_CharToTile(BC_SP));
        bpl_set(r, y, (uint8_t)Font_CharToTile(BC_V));
    }
    bpl_set(l, b, (uint8_t)Font_CharToTile(BC_BL));
    for (int c = l + 1; c < r; c++) bpl_set(c, b, (uint8_t)Font_CharToTile(BC_H));
    bpl_set(r, b, (uint8_t)Font_CharToTile(BC_BR));
}

static void bpl_restore_overworld(void) {
    Map_BuildScrollView();
    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void bpl_draw_prompt_box(void) {
    bpl_box(0, 12, SCREEN_WIDTH - 1, 17);
    for (int r = 13; r <= 16; r++)
        for (int c = 1; c < SCREEN_WIDTH - 1; c++)
            bpl_set(c, r, (uint8_t)BLANK_TILE_SLOT);
    bpl_str(1, 14, kText_WhichPokemonL1);
    bpl_str(1, 16, kText_WhichPokemonL2);
}

static void bpl_draw_list(void) {
    static const char *kItems[5] = {
        "EEVEE",
        "FLAREON",
        "JOLTEON",
        "VAPOREON",
        "CANCEL"
    };
    bpl_box(BHPC_MENU_L, BHPC_MENU_T, BHPC_MENU_R, BHPC_MENU_B);
    for (int i = 0; i < 5; i++) {
        int row = 2 + i * 2;
        bpl_set(1, row, (uint8_t)Font_CharToTile(i == s_cursor ? 0xED : BC_SP));
        bpl_str(2, row, kItems[i]);
    }
}

void BillsPokemonList_Open(void) {
    s_cursor = 0;

    Text_ShowASCII(kText_PokemonList);
    s_state = BPL_TEXT1;
}

int BillsPokemonList_IsOpen(void) {
    return s_state != BPL_CLOSED;
}

void BillsPokemonList_Tick(void) {
    switch (s_state) {

    case BPL_CLOSED:
        return;

    case BPL_TEXT1:
        if (Text_IsOpen()) { Text_Update(); return; }

        for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
        Map_BuildScrollView();
        bpl_draw_list();
        bpl_draw_prompt_box();
        s_state = BPL_MENU;
        return;

    case BPL_MENU:
        if (hJoyPressed & PAD_UP) {
            if (s_cursor > 0) s_cursor--;
            bpl_draw_list();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            if (s_cursor < 4) s_cursor++;
            bpl_draw_list();
            return;
        }
        if (hJoyPressed & PAD_B) {
            bpl_restore_overworld();
            s_state = BPL_CLOSED;
            return;
        }
        if (hJoyPressed & PAD_A) {

            if (s_cursor == 4) {
                bpl_restore_overworld();
                s_state = BPL_CLOSED;
                return;
            }

            s_shown_dex = kDexList[s_cursor];
            Pokedex_ShowData(s_shown_dex);
            s_state = BPL_DATA;
            return;
        }
        return;

    case BPL_DATA:
        if (Pokedex_IsShowingData()) {
            Pokedex_ShowDataTick();
            return;
        }

        Pokedex_SetSeenByDexNum(s_shown_dex);

        Display_LoadMapPalette();
        Map_ReloadGfx();
        Font_Load();
        NPC_ReloadTiles();
        Map_BuildScrollView();
        for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
        bpl_draw_list();
        bpl_draw_prompt_box();
        s_state = BPL_MENU;
        return;
    }
}
