
#include "bag_menu.h"
#include "menu.h"
#include "inventory.h"
#include "battle/battle_items.h"
#include "battle/battle_exp.h"
#include "party_menu.h"
#include "overworld.h"
#include "npc.h"
#include "tmhm.h"
#include "pokeflute.h"
#include "bicycle.h"
#include "town_map.h"
#include "pokemon.h"
#include "player.h"
#include "warp.h"
#include "escape_anim.h"
#include "gbc_color.h"
#include "rom_text.h"
#include "fishing.h"
#include "yesno.h"
#include "constants.h"
#include "text.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include "../data/map_data.h"
#include "amberscript_mapbank.h"
#include "itemfinder.h"
#include "crystal_pack.h"
#include "crystal_fade.h"
#include "inventory.h"
#include <stdio.h>
#include <string.h>
#include "../platform/debug_log.h"

#define CHAR_TERM   0x50
#define CHAR_SPACE  0x7F
#define CHAR_ARROW_HOLLOW 0xEC
#define CHAR_ARROW  0xED
#define CHAR_TIMES  0xF1

#define BOX_L           4
#define BOX_R          19
#define BOX_T           2
#define BOX_B          12
#define ITEM_COL        6
#define CURSOR_COL      5
#define QTY_COL        14
#define ROW_FIRST       4
#define ROW_STEP        2
#define VISIBLE         4

#define BTL_BOX_L        4
#define BTL_BOX_R       19
#define BTL_BOX_T        2
#define BTL_BOX_B       12
#define BTL_ITEM_COL     6
#define BTL_CURSOR_COL   5
#define BTL_CURSOR_CHAR  0xED
#define BTL_QTY_COL     14
#define BTL_ROW_FIRST    4
#define BTL_ROW_STEP     2
#define BTL_DOWN_COL    18
#define BTL_DOWN_ROW    11

#define ASUB_L         13
#define ASUB_R         19
#define ASUB_T         10
#define ASUB_B         14
#define ASUB_CURSOR    14
#define ASUB_ITEM      15
#define ASUB_ROW_FIRST 11
#define ASUB_ROW_STEP   2
#define NUM_ACTIONS     2
#define ASUB_W  (ASUB_R - ASUB_L + 1)
#define ASUB_H  (ASUB_B - ASUB_T + 1)

#define DOWN_COL       18
#define DOWN_ROW       11

typedef enum { BAG_LIST = 0, BAG_ACTION, BAG_TARGET, BAG_RESULT, BAG_MESSAGE,
               BAG_FADE_OUT, BAG_TOSS_QTY, BAG_TOSS_CONFIRM } BagState;

static int      gBagOpen         = 0;
static int      gBagCursor       = 0;
static int      gBagScrollTop    = 0;
static BagState gBagState        = BAG_LIST;
static int      gActionCursor    = 0;
static int      gBagBattleMode   = 0;

static int      gBagSwapMarker   = -1;

static int      gBagOldManMode      = 0;
static int      gBagOldManAutoTimer = 0;
static int      gBagFromStartMenu = 0;
static uint8_t  gBagSelectedItem = 0;
static uint8_t  gFieldUseItem    = 0;
static uint8_t  gBagMsgItem      = 0;

static char     gBagMsgBuf[48]   = {0};
static int      gRareCandyEvoPending = 0;

static uint8_t  gBagTossItem     = 0;
static int      gBagTossQty      = 1;
static int      gBagTossMaxQty   = 1;

extern int Game_BeginFieldEvolution(void);
static int      gBattleDownBlink = 0;
static int      gBattleDownOn    = 1;

static void smset(int col, int row, uint8_t tile) {

    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfsRight()] = tile;
}

static uint8_t smget(int col, int row) {
    return gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfsRight()];
}

static uint8_t s_asub_save[ASUB_H][ASUB_W];

static void pstr(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++)
        smset(col, row, (uint8_t)Font_CharToTile(*s));
}

static void pqty(int col, int row, uint8_t qty) {
    smset(col,   row, (uint8_t)Font_CharToTile(CHAR_TIMES));
    smset(col+1, row, (uint8_t)Font_CharToTile(qty >= 10 ? 0xF6 + qty/10 : CHAR_SPACE));
    smset(col+2, row, (uint8_t)Font_CharToTile(0xF6 + qty % 10));
}

static void draw_toss_qty_box(void) {
    smset(15, 9,  (uint8_t)Font_CharToTile(0x79));
    smset(16, 9,  (uint8_t)Font_CharToTile(0x7A));
    smset(17, 9,  (uint8_t)Font_CharToTile(0x7A));
    smset(18, 9,  (uint8_t)Font_CharToTile(0x7A));
    smset(19, 9,  (uint8_t)Font_CharToTile(0x7B));
    smset(15, 10, (uint8_t)Font_CharToTile(0x7C));
    smset(19, 10, (uint8_t)Font_CharToTile(0x7C));
    smset(15, 11, (uint8_t)Font_CharToTile(0x7D));
    smset(16, 11, (uint8_t)Font_CharToTile(0x7A));
    smset(17, 11, (uint8_t)Font_CharToTile(0x7A));
    smset(18, 11, (uint8_t)Font_CharToTile(0x7A));
    smset(19, 11, (uint8_t)Font_CharToTile(0x7E));
    smset(16, 10, (uint8_t)Font_CharToTile(CHAR_TIMES));
    smset(17, 10, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (gBagTossQty / 10) % 10)));
    smset(18, 10, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + gBagTossQty % 10)));
}

static const uint8_t kStrCancel[] = {0x82,0x80,0x8D,0x82,0x84,0x8B,CHAR_TERM};

static const uint8_t kStrUse[]    = {0x94,0x92,0x84,CHAR_TERM};

static const uint8_t kStrToss[]   = {0x93,0x8E,0x92,0x92,CHAR_TERM};

static int bag_total_entries(void) {
    if (gBagOldManMode) return 2;
    return gBagBattleMode ? ((int)wNumBagItems + 1) : ((int)wNumBagItems + 1);
}

static void draw_battle_down_arrow(void) {
    if (!gBagBattleMode) return;
    smset(BTL_DOWN_COL, BTL_DOWN_ROW,
          (uint8_t)Font_CharToTile(gBattleDownOn ? 0xEE  : CHAR_SPACE));
}

static void draw_ow_down_arrow(void) {
    int show = (gBagScrollTop + VISIBLE - 1 < (int)wNumBagItems) && gBattleDownOn;
    smset(DOWN_COL, DOWN_ROW, (uint8_t)Font_CharToTile(show ? 0xEE  : CHAR_SPACE));
}

#define G2BAG_VISIBLE 5

static int bag_gen2(void) {

    return !gBagBattleMode && Font_GetStyle() == FONT_STYLE_GEN2;
}

static int g2bag_x1(void) { return gCrystalPockets[0].x1; }
static int g2bag_y1(void) { return gCrystalPockets[0].y1; }
static int g2bag_x2(void) { return gCrystalPockets[0].x2; }
static int g2bag_y2(void) { return gCrystalPockets[0].y2; }
static int g2bag_rows(void) { return gCrystalPockets[0].rows; }

static void g2bag_textbox(int L, int T, int R, int B) {
    smset(L, T, (uint8_t)Font_CharToTile(0x79));
    for (int c = L + 1; c < R; c++) smset(c, T, (uint8_t)Font_CharToTile(0x7A));
    smset(R, T, (uint8_t)Font_CharToTile(0x7B));
    for (int r = T + 1; r < B; r++) {
        smset(L, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = L + 1; c < R; c++)
            smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(R, r, (uint8_t)Font_CharToTile(0x7C));
    }
    smset(L, B, (uint8_t)Font_CharToTile(0x7D));
    for (int c = L + 1; c < R; c++) smset(c, B, (uint8_t)Font_CharToTile(0x7A));
    smset(R, B, (uint8_t)Font_CharToTile(0x7E));
}

static void g2bag_ascii(int col, int row, const char *s) {
    static const uint8_t kPoke[] = {0x8F, 0x8E, 0x8A, 0xBA};
    for (; *s && *s != '\n'; s++) {
        if (*s == '#') {
            for (int i = 0; i < 4; i++)
                smset(col++, row, (uint8_t)Font_CharToTile(kPoke[i]));
        } else if (*s >= 'A' && *s <= 'Z') {
            smset(col++, row, (uint8_t)Font_CharToTile(0x80 + (*s - 'A')));
        } else if (*s >= 'a' && *s <= 'z') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xA0 + (*s - 'a')));
        } else if (*s >= '0' && *s <= '9') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xF6 + (*s - '0')));
        } else if (*s == '\'') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xE0));
        } else if (*s == '.') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xE8));
        } else if (*s == ',') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xF4));
        } else if (*s == '!') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xE7));
        } else if (*s == '?') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xE6));
        } else if (*s == '(') {
            smset(col++, row, (uint8_t)Font_CharToTile(0x9A));
        } else if (*s == ')') {
            smset(col++, row, (uint8_t)Font_CharToTile(0x9B));
        } else {
            smset(col++, row, (uint8_t)Font_CharToTile(CHAR_SPACE));
        }
    }
}

static void g2bag_normalize(const char *s, char *out, int outsz) {
    int n = 0;
    for (; *s && n < outsz - 1; s++) {
        char c = *s;
        if (c == '#') {
            const char *p = "POKE";
            while (*p && n < outsz - 1) out[n++] = *p++;
        } else if (c >= 'a' && c <= 'z') {
            out[n++] = (char)(c - 'a' + 'A');
        } else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out[n++] = c;
        }
    }
    out[n] = 0;
}

static const char *g2bag_desc_for(uint8_t port_item_id) {
    char want[24], have[24];
    char ascii[24];
    Inventory_DecodeASCII(port_item_id, ascii, (int)sizeof ascii);
    g2bag_normalize(ascii, want, (int)sizeof want);
    if (!want[0]) return 0;
    for (int i = 0; i < CRYSTAL_ITEM_DESC_COUNT; i++) {
        g2bag_normalize(gCrystalItemName[i], have, (int)sizeof have);
        if (have[0] && strcmp(have, want) == 0) return gCrystalItemDesc[i];
    }
    return 0;
}

static void g2bag_draw_desc(void) {
    int idx = gBagCursor;
    g2bag_textbox(0, 12, 19, 17);
    if (idx < 0 || idx >= (int)wNumBagItems) return;
    {
        const char *d = g2bag_desc_for(wBagItems[idx * 2]);
        if (!d) return;
        {
            const char *nl = strchr(d, '\n');
            g2bag_ascii(1, 14, d);
            if (nl) g2bag_ascii(1, 15, nl + 1);
        }
    }
}

static void g2bag_draw_items(void) {
    int total = bag_total_entries();
    int first = g2bag_y1() + 1;
    for (int s = 0; s < g2bag_rows(); s++) {
        int idx = gBagScrollTop + s;
        int row = first + s * 2;

        for (int c = CRYSTAL_PACK_LIST_COL; c < SCREEN_WIDTH; c++)
            smset(c, row, (uint8_t)Font_CharToTile(CHAR_SPACE));
        if (idx >= total) continue;
        if (idx == (int)wNumBagItems) {
            pstr(g2bag_x1() + 2, row, kStrCancel);
            continue;
        }
        {
            uint8_t id  = wBagItems[idx * 2];
            uint8_t qty = wBagItems[idx * 2 + 1];
            pstr(g2bag_x1() + 2, row, Inventory_GetName(id));

            if (!Inventory_IsKeyItem(id)) pqty(g2bag_x2() - 4, row, qty);
        }
    }
}

static void g2bag_draw_cursor(void) {
    int first = g2bag_y1() + 1;
    for (int s = 0; s < g2bag_rows(); s++)
        smset(g2bag_x1() + 1, first + s * 2,
              (uint8_t)Font_CharToTile(CHAR_SPACE));
    smset(g2bag_x1() + 1, first + (gBagCursor - gBagScrollTop) * 2,
          (uint8_t)Font_CharToTile(CHAR_ARROW));
    g2bag_draw_desc();
}

#define G2BAG_TILE_BASE 0x50

static void g2bag_load_menu_gfx(void) {
    for (int i = 0; i < CRYSTAL_PACK_MENU_TILES; i++)
        Display_LoadTile((uint8_t)i, gCrystalPackMenuGFX[i]);
}

static void g2bag_apply_palettes(void) {
    if (!GbcColor_IsEnabled()) return;
    for (int i = 0; i < 8; i++)
        Display_SetBGColorPalette(i, gCrystalPackPals[i]);
    Display_SetPositionAttrMode(1);
    Display_ClearAttrBoxes(0);
    Display_FillAttrBox(0,  0, 10, 1, 1);
    Display_FillAttrBox(10, 0, 10, 1, 2);
    Display_FillAttrBox(7,  2,  1, 9, 3);
    Display_FillAttrBox(0,  7,  5, 3, 4);
    Display_FillAttrBox(0,  3,  5, 3, 5);
    Display_SetColorMode(1);
}

static void g2bag_release_palettes(void) {
    Display_SetPositionAttrMode(0);
    Display_ClearAttrBoxes(0);
    GbcColor_MarkDirty();
}

static int s_g2_gfx_dirty = 0;

static void g2bag_load_gfx(void) {
    int img = gCrystalPackGFXOrder[0];
    if (img < 0 || img > 3) img = 0;
    for (int i = 0; i < CRYSTAL_PACK_IMG_TILES; i++)
        Display_LoadTile((uint8_t)(G2BAG_TILE_BASE + i), gCrystalPackGFX[img][i]);
    s_g2_gfx_dirty = 1;
}

static void g2bag_place_gfx(void) {
    for (int r = 0; r < CRYSTAL_PACK_IMG_H; r++)
        for (int c = 0; c < CRYSTAL_PACK_IMG_W; c++)
            smset(c, 3 + r,
                  (uint8_t)(G2BAG_TILE_BASE + r * CRYSTAL_PACK_IMG_W + c));
}

static void g2bag_place_pocket_name(int pocket) {
    if (pocket < 0 || pocket > 3) pocket = 0;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 5; c++)
            smset(c, CRYSTAL_PACK_NAME_ROW + r,
                  gCrystalPocketNameTilemap[pocket][r * 5 + c]);
}

static void draw_box(void) {
    if (bag_gen2()) {

        g2bag_load_menu_gfx();
        g2bag_load_gfx();

        for (int r = 1; r <= 11; r++)
            for (int c = 0; c < SCREEN_WIDTH; c++)
                smset(c, r, CRYSTAL_PACK_BG_TILE);

        for (int r = 1; r <= 11; r++)
            for (int c = CRYSTAL_PACK_LIST_COL; c < SCREEN_WIDTH; c++)
                smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));

        for (int c = 0; c < SCREEN_WIDTH; c++)
            smset(c, 0, (uint8_t)(CRYSTAL_PACK_HDR_TILE + c));
        g2bag_place_pocket_name(0);
        g2bag_place_gfx();

        g2bag_textbox(0, 12, 19, 17);
        g2bag_apply_palettes();
        return;
    }
    smset(BOX_L, BOX_T, (uint8_t)Font_CharToTile(0x79));
    for (int c = BOX_L+1; c < BOX_R; c++) smset(c, BOX_T, (uint8_t)Font_CharToTile(0x7A));
    smset(BOX_R, BOX_T, (uint8_t)Font_CharToTile(0x7B));
    for (int r = BOX_T+1; r < BOX_B; r++) {
        smset(BOX_L, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = BOX_L+1; c < BOX_R; c++) smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(BOX_R, r, (uint8_t)Font_CharToTile(0x7C));
    }
    smset(BOX_L, BOX_B, (uint8_t)Font_CharToTile(0x7D));
    for (int c = BOX_L+1; c < BOX_R; c++) smset(c, BOX_B, (uint8_t)Font_CharToTile(0x7A));
    smset(BOX_R, BOX_B, (uint8_t)Font_CharToTile(0x7E));
}

static void draw_battle_box(void) {
    smset(BTL_BOX_L, BTL_BOX_T, (uint8_t)Font_CharToTile(0x79));
    for (int c = BTL_BOX_L + 1; c < BTL_BOX_R; c++) smset(c, BTL_BOX_T, (uint8_t)Font_CharToTile(0x7A));
    smset(BTL_BOX_R, BTL_BOX_T, (uint8_t)Font_CharToTile(0x7B));
    for (int r = BTL_BOX_T + 1; r < BTL_BOX_B; r++) {
        smset(BTL_BOX_L, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = BTL_BOX_L + 1; c < BTL_BOX_R; c++) smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(BTL_BOX_R, r, (uint8_t)Font_CharToTile(0x7C));
    }
    smset(BTL_BOX_L, BTL_BOX_B, (uint8_t)Font_CharToTile(0x7D));
    for (int c = BTL_BOX_L + 1; c < BTL_BOX_R; c++) smset(c, BTL_BOX_B, (uint8_t)Font_CharToTile(0x7A));
    smset(BTL_BOX_R, BTL_BOX_B, (uint8_t)Font_CharToTile(0x7E));
}

static void draw_items(void) {
    if (bag_gen2()) { g2bag_draw_items(); return; }
    if (gBagBattleMode) {
        int total = bag_total_entries();
        for (int r = BTL_ROW_FIRST; r <= BTL_DOWN_ROW; r++) {
            for (int c = BTL_BOX_L + 1; c < BTL_BOX_R; c++)
                smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
        }
        if (gBagOldManMode) {

            pstr(BTL_ITEM_COL, BTL_ROW_FIRST, Inventory_GetName(ITEM_POKE_BALL));
            pqty(BTL_QTY_COL, BTL_ROW_FIRST + 1, 50);
            pstr(BTL_ITEM_COL, BTL_ROW_FIRST + BTL_ROW_STEP, kStrCancel);
            return;
        }
        for (int s = 0; s < VISIBLE; s++) {
            int idx = gBagScrollTop + s;
            int row = BTL_ROW_FIRST + s * BTL_ROW_STEP;

            if (idx >= total) continue;
            if (idx == (int)wNumBagItems) {
                pstr(BTL_ITEM_COL, row, kStrCancel);
                continue;
            }

            {
                uint8_t id  = wBagItems[idx * 2];
                uint8_t qty = wBagItems[idx * 2 + 1];
                pstr(BTL_ITEM_COL, row, Inventory_GetName(id));
                if (!Inventory_IsKeyItem(id))
                    pqty(BTL_QTY_COL, row + 1, qty);
            }
        }
        draw_battle_down_arrow();
        return;
    }

    int total = bag_total_entries();
    for (int r = ROW_FIRST; r < BOX_B; r++)
        for (int c = BOX_L + 1; c < BOX_R; c++)
            smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
    for (int s = 0; s < VISIBLE; s++) {
        int idx = gBagScrollTop + s;
        int row = ROW_FIRST + s * ROW_STEP;
        if (idx >= total) break;
        if (idx == (int)wNumBagItems) {
            pstr(ITEM_COL, row, kStrCancel);
            continue;
        }
        uint8_t id  = wBagItems[idx * 2];
        uint8_t qty = wBagItems[idx * 2 + 1];
        pstr(ITEM_COL, row, Inventory_GetName(id));
        if (!Inventory_IsKeyItem(id))
            pqty(QTY_COL, row + 1, qty);
    }

    draw_ow_down_arrow();
}

static int cursor_screen_row(void) {
    if (gBagBattleMode)
        return BTL_ROW_FIRST + (gBagCursor - gBagScrollTop) * BTL_ROW_STEP;
    return ROW_FIRST + (gBagCursor - gBagScrollTop) * ROW_STEP;
}

static void draw_cursor(void) {
    if (bag_gen2()) { g2bag_draw_cursor(); return; }
    if (gBagBattleMode) {
        for (int s = 0; s < VISIBLE; s++)
            smset(BTL_CURSOR_COL, BTL_ROW_FIRST + s * BTL_ROW_STEP, (uint8_t)Font_CharToTile(CHAR_SPACE));
        if (gBagSwapMarker >= gBagScrollTop && gBagSwapMarker < gBagScrollTop + VISIBLE)
            smset(BTL_CURSOR_COL, BTL_ROW_FIRST + (gBagSwapMarker - gBagScrollTop) * BTL_ROW_STEP,
                  (uint8_t)Font_CharToTile(CHAR_ARROW_HOLLOW));
        smset(BTL_CURSOR_COL, cursor_screen_row(), (uint8_t)Font_CharToTile(BTL_CURSOR_CHAR));
        draw_battle_down_arrow();
        return;
    }

    for (int s = 0; s < VISIBLE; s++)
        smset(CURSOR_COL, ROW_FIRST + s * ROW_STEP, (uint8_t)Font_CharToTile(CHAR_SPACE));
    if (gBagSwapMarker >= gBagScrollTop && gBagSwapMarker < gBagScrollTop + VISIBLE)
        smset(CURSOR_COL, ROW_FIRST + (gBagSwapMarker - gBagScrollTop) * ROW_STEP,
              (uint8_t)Font_CharToTile(CHAR_ARROW_HOLLOW));
    smset(CURSOR_COL, cursor_screen_row(), (uint8_t)Font_CharToTile(CHAR_ARROW));
}

static void draw_action_submenu(void) {

    for (int r = 0; r < ASUB_H; r++)
        for (int c = 0; c < ASUB_W; c++)
            s_asub_save[r][c] = smget(ASUB_L + c, ASUB_T + r);

    smset(ASUB_L, ASUB_T, (uint8_t)Font_CharToTile(0x79));
    for (int c = ASUB_L+1; c < ASUB_R; c++) smset(c, ASUB_T, (uint8_t)Font_CharToTile(0x7A));
    smset(ASUB_R, ASUB_T, (uint8_t)Font_CharToTile(0x7B));
    for (int r = ASUB_T+1; r < ASUB_B; r++) {
        smset(ASUB_L, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = ASUB_L+1; c < ASUB_R; c++) smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(ASUB_R, r, (uint8_t)Font_CharToTile(0x7C));
    }
    smset(ASUB_L, ASUB_B, (uint8_t)Font_CharToTile(0x7D));
    for (int c = ASUB_L+1; c < ASUB_R; c++) smset(c, ASUB_B, (uint8_t)Font_CharToTile(0x7A));
    smset(ASUB_R, ASUB_B, (uint8_t)Font_CharToTile(0x7E));

    pstr(ASUB_ITEM, ASUB_ROW_FIRST + 0 * ASUB_ROW_STEP, kStrUse);
    pstr(ASUB_ITEM, ASUB_ROW_FIRST + 1 * ASUB_ROW_STEP, kStrToss);
    smset(ASUB_CURSOR, ASUB_ROW_FIRST + gActionCursor * ASUB_ROW_STEP,
          (uint8_t)Font_CharToTile(CHAR_ARROW));
}

static int bag_item_is_vitamin(uint8_t id) {
    return id == ITEM_HP_UP || id == ITEM_PROTEIN || id == ITEM_IRON ||
           id == ITEM_CARBOS || id == ITEM_CALCIUM;
}

static int bag_item_is_evo_stone(uint8_t id) {
    return id == ITEM_MOON_STONE  || id == ITEM_FIRE_STONE ||
           id == ITEM_THUNDER_STONE || id == ITEM_WATER_STONE ||
           id == ITEM_LEAF_STONE;
}

static int bag_item_is_field_medicine(uint8_t id) {
    switch (id) {
    case ITEM_POTION:
    case ITEM_SUPER_POTION:
    case ITEM_HYPER_POTION:
    case ITEM_MAX_POTION:
    case ITEM_FRESH_WATER:
    case ITEM_SODA_POP:
    case ITEM_LEMONADE:
    case ITEM_ANTIDOTE:
    case ITEM_BURN_HEAL:
    case ITEM_ICE_HEAL:
    case ITEM_AWAKENING:
    case ITEM_PARLYZ_HEAL:
    case ITEM_FULL_HEAL:
    case ITEM_REVIVE:
    case ITEM_MAX_REVIVE:
    case ITEM_FULL_RESTORE:
        return 1;
    default:
        return 0;
    }
}

static void bag_move_select_rom(const char *symbol, pm_moveuse_fn apply) {
    static char l1[32], l2[32];
    const char *t = RomText(symbol);
    const char *nl = strchr(t, '\n');
    if (!nl) {
        snprintf(l1, sizeof l1, "%s", t);
        l2[0] = '\0';
    } else {
        size_t n = (size_t)(nl - t);
        if (n > sizeof l1 - 1) n = sizeof l1 - 1;
        memcpy(l1, t, n);
        l1[n] = '\0';
        snprintf(l2, sizeof l2, "%s", nl + 1);
    }
    PartyMenu_RequestMoveSelect(l1, l2, apply);
}

static pm_moveuse_disp_t bag_ether_apply(int slot, int mv, char *l1, char *l2) {
    int restored = Pokemon_ApplyPPRestore((uint8_t)slot, mv, gFieldUseItem);
    if (restored) {
        Audio_PlaySFX_HealAilment();
        Inventory_Remove(gFieldUseItem, 1);
        strcpy(l1, "PP was");
        strcpy(l2, "restored.");
    } else {
        strcpy(l1, "It won't have");
        strcpy(l2, "any effect.");
    }
    return PM_MOVEUSE_CLOSE;
}

static pm_moveuse_disp_t bag_pp_up_apply(int slot, int mv, char *l1, char *l2) {
    int raised = Pokemon_ApplyPPUp((uint8_t)slot, mv);
    snprintf(l1, 20, "%s's PP", PartyMenu_MonName(slot));
    if (raised) {
        Inventory_Remove(ITEM_PP_UP, 1);
        strcpy(l2, "increased.");
        return PM_MOVEUSE_CLOSE;
    }
    strcpy(l2, "is maxed out.");
    return PM_MOVEUSE_RELOOP;
}

static void erase_action_submenu(void) {

    for (int r = 0; r < ASUB_H; r++)
        for (int c = 0; c < ASUB_W; c++)
            smset(ASUB_L + c, ASUB_T + r, s_asub_save[r][c]);

    draw_box();
    draw_items();
    draw_cursor();
}

static void scroll_clamp(void) {

    int max_top = bag_total_entries() - (bag_gen2() ? G2BAG_VISIBLE : VISIBLE);
    if (max_top < 0) max_top = 0;
    if (gBagScrollTop > max_top) gBagScrollTop = max_top;
    if (gBagScrollTop < 0)       gBagScrollTop = 0;
}

static void bag_handle_select(void) {
    if (gBagCursor >= (int)wNumBagItems) return;
    if (gBagSwapMarker < 0) {
        gBagSwapMarker = gBagCursor;
        draw_cursor();
        return;
    }
    if (gBagSwapMarker == gBagCursor) return;

    int ia = gBagSwapMarker, ib = gBagCursor;
    uint8_t id_a = wBagItems[ia * 2], qty_a = wBagItems[ia * 2 + 1];
    uint8_t id_b = wBagItems[ib * 2], qty_b = wBagItems[ib * 2 + 1];

    if (id_a != id_b) {
        wBagItems[ia * 2]     = id_b;  wBagItems[ia * 2 + 1] = qty_b;
        wBagItems[ib * 2]     = id_a;  wBagItems[ib * 2 + 1] = qty_a;
    } else {
        int sum = (int)qty_a + (int)qty_b;
        if (sum <= 99) {
            wBagItems[ib * 2 + 1] = (uint8_t)sum;
            int n = (int)wNumBagItems;
            for (int i = ia; i < n - 1; i++) {
                wBagItems[i * 2]     = wBagItems[(i + 1) * 2];
                wBagItems[i * 2 + 1] = wBagItems[(i + 1) * 2 + 1];
            }
            wBagItems[(n - 1) * 2] = 0xFF;
            wNumBagItems  = (uint8_t)(n - 1);
            gBagScrollTop = 0;
            gBagCursor    = 0;
        } else {
            wBagItems[ia * 2 + 1] = (uint8_t)(sum - 99);
            wBagItems[ib * 2 + 1] = 99;
        }
    }
    gBagSwapMarker = -1;
    scroll_clamp();
    draw_items();
    draw_cursor();
}

static int s_party_gfx_dirty = 0;

static void bag_rebuild_overworld_tiles(void) {
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void bag_restore_overworld(void) {

    if (gBagBattleMode) return;
    bag_rebuild_overworld_tiles();
    Display_LoadMapPalette();
}

#define BAG_ITEM_CLOSE_FADE 14

static void bag_close(void) {
    gBagOpen = 0;
    gBagSwapMarker = -1;
    if (s_g2_gfx_dirty) {

        Map_ReloadGfx();
        Font_Load();
        NPC_ReloadTiles();
        g2bag_release_palettes();
        s_g2_gfx_dirty = 0;

        CrystalFade_Start(CRYSTAL_FADE_IN_FROM_WHITE);
    }
    if (s_party_gfx_dirty) {

        Map_ReloadGfx();
        Font_Load();
        NPC_ReloadTiles();
        Player_SyncOAM();
        s_party_gfx_dirty = 0;
    }
    if (gBagFromStartMenu) {

        gBagFromStartMenu = 0;
        Menu_ResumeFromBag();
    } else {
        bag_restore_overworld();
    }
    gBagBattleMode = 0;
}

static int s_reopen_white = 0;

static void bag_reveal_tick(void) {
    if (s_reopen_white > 0 && --s_reopen_white == 0)
        Display_LoadMapPalette();
}

static void bag_hide_sprites_behind_box(void) {
    const int ofs = Map_UiColOfsRight();
    if (ofs == 0) {
        for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
        return;
    }

    const int left_px   = ((bag_gen2() ? 0 : BOX_L) + ofs) * 8;
    const int bottom_px = ((bag_gen2() ? SCREEN_HEIGHT - 1 : BOX_B) + 1) * 8;
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (wShadowOAM[i].y == 0) continue;
        int sx = (int)wShadowOAM[i].x - 8;
        int sy = (int)wShadowOAM[i].y - 16;
        if (sx + 8 > left_px && sy < bottom_px) wShadowOAM[i].y = 0;
    }
}

static void bag_reopen_after_use(void) {
    gBagSwapMarker = -1;
    if (gBagCursor > (int)wNumBagItems) gBagCursor = (int)wNumBagItems;
    scroll_clamp();
    Display_SetPalette(0x00, 0x00, 0x00);
    Menu_DrawBackdropForBag();
    bag_hide_sprites_behind_box();
    gBagState = BAG_LIST;
    draw_box();
    draw_items();
    draw_cursor();
    Overworld_ArmCloseWhiteout(BAG_ITEM_CLOSE_FADE);
    s_reopen_white = BAG_ITEM_CLOSE_FADE;
}

void BagMenu_ReopenAfterUse(void) {
    bag_reopen_after_use();
}

static void bag_return_to_list(void) {
    Menu_DrawBackdropForBag();
    bag_hide_sprites_behind_box();
    gBagState = BAG_LIST;
    draw_box();
    draw_items();
    draw_cursor();
}

void BagMenu_PalTrace(const char *where) {
#if AMBER_DEBUG_PRINTS
    static int last = -1;
    int bgp = Display_GetBGP();
    FILE *f;
    if (bgp == last) return;
    last = bgp;
    f = fopen("bugs/tmdbg.log", "a");
    if (!f) return;
    DBG_FPRINTF(f, "[PALDBG] %-22s bgp=%02X bag=%d white_left=%d\n",
            where, bgp, gBagOpen, s_reopen_white);
    fclose(f);
#else

    (void)where;
#endif
}

void BagMenu_Open(void) {
    gBagOpen      = 1;
    gBagCursor    = 0;
    gBagScrollTop = 0;
    gBagSwapMarker = -1;
    gBagState     = BAG_LIST;
    gActionCursor = 0;
    gFieldUseItem = 0;
    gBagFromStartMenu = 1;
    gBattleDownBlink = 0;
    gBattleDownOn    = 1;

    bag_hide_sprites_behind_box();
    if (bag_gen2()) {

        gBagState = BAG_FADE_OUT;
        CrystalFade_Start(CRYSTAL_FADE_OUT_TO_WHITE);
        return;
    }
    draw_box();
    draw_items();
    draw_cursor();
}

int BagMenu_IsOpen(void) { return gBagOpen; }

void BagMenu_OpenBattle(void) {
    gBagOldManMode   = 0;
    gBagBattleMode   = 1;
    gBagFromStartMenu = 0;
    gBagSelectedItem = 0;
    gBagOpen         = 1;
    gBagCursor       = 0;
    gBagScrollTop    = 0;
    gBagSwapMarker   = -1;
    gBagState        = BAG_LIST;
    gActionCursor    = 0;
    gFieldUseItem    = 0;
    gBattleDownBlink = 0;
    gBattleDownOn    = 1;

    draw_battle_box();
    draw_items();
    draw_cursor();
}

void BagMenu_OpenBattleOldMan(void) {
    BagMenu_OpenBattle();
    gBagOldManMode      = 1;
    gBagOldManAutoTimer = 40;
    draw_items();
    draw_cursor();
}

uint8_t BagMenu_GetSelected(void) { return gBagSelectedItem; }

void BagMenu_Tick(void) {
    bag_reveal_tick();
    BagMenu_PalTrace("bag_tick");
    if (gBagState == BAG_FADE_OUT) {

        if (CrystalFade_Active()) return;
        CrystalFade_Reset();
        gBagState = BAG_LIST;
        draw_box();
        draw_items();
        draw_cursor();
        return;
    }
    if (gBagState == BAG_LIST) {

        if (++gBattleDownBlink >= 12) {
            gBattleDownBlink = 0;
            gBattleDownOn ^= 1;
            if (gBagBattleMode) draw_battle_down_arrow();
            else                draw_ow_down_arrow();
        }
        if (gBagOldManMode) {

            if (--gBagOldManAutoTimer <= 0) {
                gBagSelectedItem = ITEM_POKE_BALL;
                gBagOldManMode   = 0;
                bag_close();
            }
            return;
        }
    }

    if (gBagState == BAG_MESSAGE) {

        Text_Update();
        if (!Text_IsOpen()) {
            if (gBagMsgItem) { Inventory_Remove(gBagMsgItem, 1); gBagMsgItem = 0; }
            if (gBagCursor > (int)wNumBagItems) gBagCursor = (int)wNumBagItems;
            scroll_clamp();
            draw_box();
            draw_items();
            draw_cursor();
            gBagState = BAG_LIST;
        }
        return;
    }

    if (gBagState == BAG_RESULT) {
        if (PartyMenu_IsOpen()) {
            PartyMenu_Tick();
            return;
        }

        if (gRareCandyEvoPending) {

            if (Audio_IsSFXPlaying())
                return;
            gRareCandyEvoPending = 0;
            gBagOpen       = 0;
            gBagBattleMode = 0;
            gBagFromStartMenu = 0;
            gBagState      = BAG_LIST;
            int began = Game_BeginFieldEvolution();

            wForceEvolution = 0;
            if (began)
                return;

            bag_restore_overworld();
            return;
        }

        bag_reopen_after_use();
        return;
    }

    if (gBagState == BAG_ACTION) {

        if (hJoyPressed & PAD_UP) {
            smset(ASUB_CURSOR, ASUB_ROW_FIRST + gActionCursor * ASUB_ROW_STEP,
                  (uint8_t)Font_CharToTile(CHAR_SPACE));
            gActionCursor = (gActionCursor == 0) ? NUM_ACTIONS - 1 : gActionCursor - 1;
            smset(ASUB_CURSOR, ASUB_ROW_FIRST + gActionCursor * ASUB_ROW_STEP,
                  (uint8_t)Font_CharToTile(CHAR_ARROW));
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            smset(ASUB_CURSOR, ASUB_ROW_FIRST + gActionCursor * ASUB_ROW_STEP,
                  (uint8_t)Font_CharToTile(CHAR_SPACE));
            gActionCursor = (gActionCursor == NUM_ACTIONS - 1) ? 0 : gActionCursor + 1;
            smset(ASUB_CURSOR, ASUB_ROW_FIRST + gActionCursor * ASUB_ROW_STEP,
                  (uint8_t)Font_CharToTile(CHAR_ARROW));
            return;
        }
        if (hJoyPressed & (PAD_B | PAD_START)) {
            gBagState = BAG_LIST;
            erase_action_submenu();
            return;
        }
        if (hJoyPressed & PAD_A) {
            uint8_t id = wBagItems[gBagCursor * 2];
            switch (gActionCursor) {
                case 0:
                    if (gBagBattleMode) {

                        gBagSelectedItem = id;
                        bag_close();
                    } else if (bag_item_is_field_medicine(id) || id == ITEM_RARE_CANDY ||
                               bag_item_is_evo_stone(id) || bag_item_is_vitamin(id) ||
                               id == ITEM_ELIXER || id == ITEM_MAX_ELIXER ||
                               id == ITEM_ETHER || id == ITEM_MAX_ETHER || id == ITEM_PP_UP) {

                        gFieldUseItem = id;

                        PartyMenu_SetEvoStone(bag_item_is_evo_stone(id) ? id : 0);
                        s_party_gfx_dirty = 1;
        PartyMenu_Open(PARTY_MENU_ITEM_USE);
                        if (id == ITEM_ETHER || id == ITEM_MAX_ETHER)
                            bag_move_select_rom("RestorePPWhichTechniqueText",
                                                bag_ether_apply);
                        else if (id == ITEM_PP_UP)
                            bag_move_select_rom("RaisePPWhichTechniqueText",
                                                bag_pp_up_apply);
                        gBagState = BAG_TARGET;
                    } else if (id == ITEM_OLD_ROD || id == ITEM_GOOD_ROD ||
                               id == ITEM_SUPER_ROD) {

                        if (!Fishing_CanUse()) {
                            Text_ShowASCII(RomText("_ItemUseNotTimeText"));
                        } else {

                            gBagFromStartMenu = 0;
                            bag_close();
                            Fishing_Use(id);
                        }
                    } else if (id == ITEM_TOWN_MAP) {
                        TownMap_Open();
                        bag_close();
                    } else if (id >= HM01) {

                        s_party_gfx_dirty = 1;
            TMHM_Use(id);
                    } else if (id == ITEM_POKE_FLUTE) {

                        gBagFromStartMenu = 0;
                        bag_close();
                        PokeFlute_Use();
                    } else if (id == ITEM_ITEMFINDER) {

                        gBagFromStartMenu = 0;
                        bag_close();
                        Itemfinder_Use();
                    } else if (id == ITEM_REPEL || id == ITEM_SUPER_REPEL ||
                               id == ITEM_MAX_REPEL) {

                        wRepelRemainingSteps =
                            (id == ITEM_MAX_REPEL)   ? 250 :
                            (id == ITEM_SUPER_REPEL) ? 200 : 100;
                        Audio_PlaySFX_HealAilment();
                        {
                            const char *rn = (id == ITEM_MAX_REPEL)   ? "MAX REPEL"   :
                                             (id == ITEM_SUPER_REPEL) ? "SUPER REPEL" : "REPEL";
                            snprintf(gBagMsgBuf, sizeof(gBagMsgBuf), "{PLAYER} used\n%s!", rn);
                            Text_ShowASCII(gBagMsgBuf);
                        }

                        gBagMsgItem = id;
                        gBagState   = BAG_MESSAGE;
                    } else if (id == ITEM_ESCAPE_ROPE) {

                        if (!EscapeAnim_CanEscapeHere()) {

                            Text_ShowASCII("OAK: {PLAYER}!\nThis isn't the\ntime to use that!");
                            gBagState = BAG_LIST;
                            erase_action_submenu();
                        } else {
                            Inventory_Remove(id, 1);

                            gBagFromStartMenu = 0;
                            bag_close();
                            EscapeAnim_StartToLastHealTown();
                        }
                    } else if (id == ITEM_BICYCLE) {
                        if (Bicycle_UseFromBag()) {

                            gBagFromStartMenu = 0;
                            bag_close();
                        } else {
                            gBagState = BAG_LIST;
                            erase_action_submenu();
                        }
                    } else if (id == ITEM_EXP_ALL) {

                        gBagMsgItem = 0;
                        Text_ShowASCII(RomText("_ItemUseNotTimeText"));
                        gBagState = BAG_MESSAGE;
                        erase_action_submenu();
                    } else {

                        gBagState = BAG_LIST;
                        erase_action_submenu();
                    }
                    break;
                case 1:

                    if (Inventory_IsHM(id) || Inventory_IsKeyItem(id)) {

                        Text_ShowASCII(RomText("_TooImportantToTossText"));
                        gBagMsgItem = 0;
                        gBagState   = BAG_MESSAGE;
                        erase_action_submenu();
                        break;
                    }
                    gBagTossItem = id;

                    gBagTossMaxQty = (int)Inventory_GetQty(id);
                    if (id >= HM01 || gBagTossMaxQty <= 1) {
                        gBagTossQty = 1;
                        Text_SetItemName(id);
                        YesNo_Show(RomText("_IsItOKToTossItemText"));
                        gBagState = BAG_TOSS_CONFIRM;
                        erase_action_submenu();
                        break;
                    }
                    gBagTossQty = 1;
                    draw_toss_qty_box();
                    gBagState = BAG_TOSS_QTY;
                    break;
            }
            return;
        }
        return;
    }

    if (gBagState == BAG_TOSS_QTY) {

        if (hJoyPressed & PAD_UP) {
            gBagTossQty++;
            if (gBagTossQty > gBagTossMaxQty) gBagTossQty = 1;
            draw_toss_qty_box();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            gBagTossQty--;
            if (gBagTossQty < 1) gBagTossQty = gBagTossMaxQty;
            draw_toss_qty_box();
            return;
        }
        if (hJoyPressed & PAD_B) {
            bag_return_to_list();
            return;
        }
        if (hJoyPressed & PAD_A) {
            Text_SetItemName(gBagTossItem);
            YesNo_Show(RomText("_IsItOKToTossItemText"));
            gBagState = BAG_TOSS_CONFIRM;
            return;
        }
        return;
    }

    if (gBagState == BAG_TOSS_CONFIRM) {
        YesNo_Tick();
        if (YesNo_IsOpen()) return;
        if (YesNo_GetResult()) {

            Inventory_Remove(gBagTossItem, (uint8_t)gBagTossQty);
            {
                char iname[20];
                char msg[48];
                Inventory_DecodeASCII(gBagTossItem, iname, sizeof(iname));
                RomTextSplice(msg, sizeof(msg), "_ThrewAwayItemText", "{badge}", iname);
                snprintf(gBagMsgBuf, sizeof(gBagMsgBuf), "%s", msg);
            }
            Text_ShowASCII(gBagMsgBuf);
            gBagMsgItem = 0;
            gBagState = BAG_MESSAGE;
        } else {

            bag_return_to_list();
        }
        return;
    }

    if (gBagState == BAG_TARGET) {
        if (PartyMenu_IsOpen()) {
            PartyMenu_Tick();
            return;
        }

        int slot = PartyMenu_GetSelected();
        gBagState = BAG_LIST;
        if (slot < 0) {

            bag_reopen_after_use();
            return;
        }

        if (gFieldUseItem == ITEM_RARE_CANDY) {

            uint8_t new_level = 0;
            int r = Pokemon_ApplyRareCandy((uint8_t)slot, &new_level);
            if (r != 0)
                Inventory_Remove(gFieldUseItem, 1);
            gRareCandyEvoPending = (r == 2);
            PartyMenu_ShowRareCandyResult(slot, r != 0, (int)new_level, r == 2);
            gBagState = BAG_RESULT;
            return;
        }

        if (bag_item_is_evo_stone(gFieldUseItem)) {

            int armed = Pokemon_ApplyEvoStone((uint8_t)slot, gFieldUseItem);
            if (armed) {
                Audio_PlaySFX_HealAilment();
                Inventory_Remove(gFieldUseItem, 1);
                PartyMenu_KeepIconsForEvolution();
                gRareCandyEvoPending = 1;
            } else {

                PartyMenu_ShowItemUseResult(slot, 0, 0);
            }
            gBagState = BAG_RESULT;
            return;
        }

        if (bag_item_is_vitamin(gFieldUseItem)) {

            int rose = Pokemon_ApplyVitamin((uint8_t)slot, gFieldUseItem);
            if (rose) {
                Audio_PlaySFX_HealAilment();
                Inventory_Remove(gFieldUseItem, 1);
            }
            PartyMenu_ShowStatRoseResult(slot, Pokemon_VitaminStatName(gFieldUseItem), rose);
            gBagState = BAG_RESULT;
            return;
        }

        if (gFieldUseItem == ITEM_ETHER || gFieldUseItem == ITEM_MAX_ETHER ||
            gFieldUseItem == ITEM_PP_UP) {

            gBagState = BAG_RESULT;
            return;
        }

        if (gFieldUseItem == ITEM_ELIXER || gFieldUseItem == ITEM_MAX_ELIXER) {

            int restored = Pokemon_ApplyPPRestore((uint8_t)slot, -1, gFieldUseItem);
            if (restored) {
                Audio_PlaySFX_HealAilment();
                Inventory_Remove(gFieldUseItem, 1);
                PartyMenu_ShowTextResult("PP was", "restored.");
            } else {
                PartyMenu_ShowTextResult("It won't have", "any effect.");
            }
            gBagState = BAG_RESULT;
            return;
        }

        uint16_t old_hp = wPartyMons[slot].base.hp;
        item_use_result_t res = Battle_UseItem(gFieldUseItem, (uint8_t)slot);
        uint16_t new_hp = wPartyMons[slot].base.hp;
        uint16_t healed = (new_hp > old_hp) ? (uint16_t)(new_hp - old_hp) : 0;

        if (res == ITEM_USE_OK && !Inventory_IsKeyItem(gFieldUseItem))
            Inventory_Remove(gFieldUseItem, 1);

        if (res == ITEM_USE_OK) {
            int hp_item = (gFieldUseItem >= ITEM_FULL_RESTORE &&
                           gFieldUseItem != ITEM_FULL_HEAL);
            if (hp_item) Audio_PlaySFX_HealHP();
            else         Audio_PlaySFX_HealAilment();
            if (healed > 0)
                PartyMenu_AnimateItemHeal(slot, old_hp, new_hp, healed);
            else
                PartyMenu_ShowItemUseResult(slot, healed, 1);
        } else {
            PartyMenu_ShowItemUseResult(slot, healed, 0);
        }
        gBagState = BAG_RESULT;
        return;
    }

    if (gBagBattleMode) {
        int total = bag_total_entries();
        if (hJoyPressed & PAD_UP) {
            if (gBagCursor > 0) {
                gBagCursor--;
                if (gBagCursor < gBagScrollTop) gBagScrollTop--;
                scroll_clamp();
                draw_items();
                draw_cursor();
            }
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            if (gBagCursor < total - 1) {
                gBagCursor++;
                if (gBagCursor >= gBagScrollTop + (bag_gen2() ? G2BAG_VISIBLE : VISIBLE))
            gBagScrollTop++;
                scroll_clamp();
                draw_items();
                draw_cursor();
            }
            return;
        }
        if (hJoyPressed & PAD_SELECT) {
            bag_handle_select();
            return;
        }
        if (hJoyPressed & (PAD_B | PAD_START)) {
            gBagSelectedItem = 0;
            bag_close();
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (gBagCursor == (int)wNumBagItems) {
                gBagSelectedItem = 0;
            } else {
                gBagSelectedItem = wBagItems[gBagCursor * 2];
            }
            bag_close();
            return;
        }
        return;
    }

    int total = bag_total_entries();
    if (hJoyPressed & PAD_UP) {
        if (gBagCursor > 0) {
            gBagCursor--;
            if (gBagCursor < gBagScrollTop) gBagScrollTop--;
            scroll_clamp();
            draw_items();
            draw_cursor();
        }
        return;
    }
    if (hJoyPressed & PAD_DOWN) {
        if (gBagCursor < total - 1) {
            gBagCursor++;
            if (gBagCursor >= gBagScrollTop + (bag_gen2() ? G2BAG_VISIBLE : VISIBLE))
            gBagScrollTop++;
            scroll_clamp();
            draw_items();
            draw_cursor();
        }
        return;
    }

    if (hJoyPressed & PAD_SELECT) {
        bag_handle_select();
        return;
    }

    if (hJoyPressed & (PAD_B | PAD_START)) {
        bag_close();
        return;
    }

    if (hJoyPressed & PAD_A) {
        if (gBagCursor == (int)wNumBagItems) {

            bag_close();
        } else {
            gBagSwapMarker = -1;

            gBagState     = BAG_ACTION;
            gActionCursor = 0;
            draw_action_submenu();
        }
        return;
    }
}
