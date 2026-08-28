
#include "players_pc.h"
#include "rom_text.h"

#include "constants.h"
#include "inventory.h"
#include "pc_menu.h"
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "text.h"
#include "yesno.h"
#include "../data/font_data.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include "../platform/hardware.h"

#include <stdio.h>
#include <string.h>

extern uint8_t wNumBagItems;
extern uint8_t wBagItems[BAG_ITEM_CAPACITY * 2 + 1];
extern uint8_t wNumBoxItems;
extern uint8_t wBoxItems[PC_ITEM_CAPACITY * 2 + 1];
extern uint8_t wPlayerName[NAME_LENGTH];

typedef enum { OP_WITHDRAW = 0, OP_DEPOSIT = 1, OP_TOSS = 2 } ppc_op_t;

typedef enum {
    PPC_CLOSED = 0,
    PPC_BOOT_DELAY,
    PPC_WAIT_BOOT,
    PPC_MENU,
    PPC_WAIT_TO_MENU,
    PPC_LIST,
    PPC_WAIT_TO_LIST,
    PPC_QTY,
    PPC_WAIT_TOSS,
} ppc_mode_t;

static ppc_mode_t s_mode = PPC_CLOSED;

static int        s_from_pc_menu = 0;
static ppc_op_t   s_op;
static int        s_menu_cursor;
static int        s_list_cursor;
static int        s_list_scroll;
static int        s_sel_item;

static int        s_swap_marker = -1;
static int        s_qty;
static int        s_qty_max;
static int        s_boot_delay;

static char       s_msg[64];

#define CHAR_SPACE         0x7F
#define CHAR_ARROW         0xED
#define CHAR_ARROW_HOLLOW  0xEC
#define CHAR_DOWN          0xEE
#define CHAR_TIMES  0xF1

static uint8_t *ppc_src_num(void)   { return s_op == OP_DEPOSIT ? &wNumBagItems : &wNumBoxItems; }
static uint8_t *ppc_src_items(void) { return s_op == OP_DEPOSIT ?  wBagItems     :  wBoxItems;    }

static void ppc_put(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile;
}

static int ppc_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == ' ')  return BLANK_TILE_SLOT;
    if (c == '.')  return Font_CharToTile(0xE8);
    if (c == '!')  return Font_CharToTile(0xE7);
    if (c == '?')  return Font_CharToTile(0xE6);
    if (c == '\'') return Font_CharToTile(0xE0);
    if (c == '-')  return Font_CharToTile(0xE3);
    if (c == '/')  return Font_CharToTile(0xF3);
    return BLANK_TILE_SLOT;
}

static void ppc_str(int col, int row, const char *s) {
    for (; *s; s++, col++) ppc_put(col, row, (uint8_t)ppc_ascii_tile((unsigned char)*s));
}

static void ppc_clear_overlay(void) {
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            gWindowTileMap[r][c] = 0;
}

static void ppc_box(int l, int t, int r, int b) {
    ppc_put(l, t, (uint8_t)Font_CharToTile(0x79));
    for (int c = l + 1; c < r; c++) ppc_put(c, t, (uint8_t)Font_CharToTile(0x7A));
    ppc_put(r, t, (uint8_t)Font_CharToTile(0x7B));
    for (int y = t + 1; y < b; y++) {
        ppc_put(l, y, (uint8_t)Font_CharToTile(0x7C));
        for (int c = l + 1; c < r; c++) ppc_put(c, y, BLANK_TILE_SLOT);
        ppc_put(r, y, (uint8_t)Font_CharToTile(0x7C));
    }
    ppc_put(l, b, (uint8_t)Font_CharToTile(0x7D));
    for (int c = l + 1; c < r; c++) ppc_put(c, b, (uint8_t)Font_CharToTile(0x7A));
    ppc_put(r, b, (uint8_t)Font_CharToTile(0x7E));
}

static void ppc_item_name(int col, int row, uint8_t item_id, int width) {
    const uint8_t *n = Inventory_GetName(item_id);
    int i = 0;
    for (; i < width && n[i] != 0x50; i++)
        ppc_put(col + i, row, (uint8_t)Font_CharToTile(n[i]));
    for (; i < width; i++) ppc_put(col + i, row, BLANK_TILE_SLOT);
}

static void ppc_qty2(int col, int row, int qty) {
    ppc_put(col, row, (uint8_t)Font_CharToTile(CHAR_TIMES));
    ppc_put(col + 1, row, qty >= 10
            ? (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (qty / 10) % 10))
            : BLANK_TILE_SLOT);
    ppc_put(col + 2, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + qty % 10)));
}

static void ppc_player_name_ascii(char *buf, int cap) {
    int o = 0;
    for (int i = 0; i < NAME_LENGTH && o < cap - 1; i++) {
        uint8_t c = wPlayerName[i];
        if (c == 0x50 || c == 0x00) break;
        if      (c >= 0x80 && c <= 0x99) buf[o++] = (char)('A' + (c - 0x80));
        else if (c >= 0xA0 && c <= 0xB9) buf[o++] = (char)('a' + (c - 0xA0));
        else if (c >= 0xF6)              buf[o++] = (char)('0' + (c - 0xF6));
        else if (c == 0x7F)              buf[o++] = ' ';
    }
    buf[o] = '\0';
    if (o == 0) snprintf(buf, cap, "%s", "RED");
}

static void ppc_restore_overworld(void) {
    s_mode = PPC_CLOSED;

    if (s_from_pc_menu) { s_from_pc_menu = 0; return; }
    ppc_clear_overlay();
    hWY = SCREEN_HEIGHT_PX;
    Display_LoadMapPalette();
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void ppc_draw_menu_frame(uint8_t cursor_char) {
    static const char *entries[4] = { "WITHDRAW ITEM", "DEPOSIT ITEM", "TOSS ITEM", "LOG OFF" };
    ppc_box(0, 0, 15, 9);
    for (int i = 0; i < 4; i++) {
        int row = 2 + i * 2;
        ppc_put(1, row, (uint8_t)Font_CharToTile(i == s_menu_cursor ? cursor_char : CHAR_SPACE));
        ppc_str(2, row, entries[i]);
    }
}

static void ppc_draw_menu(void) {
    ppc_clear_overlay();
    hWY = 0;
    ppc_draw_menu_frame(CHAR_ARROW);

    ppc_box(0, 12, 19, 17);
    ppc_str(1, 14, "What do you want");
    ppc_str(1, 16, "to do?");
}

static int ppc_list_count(void)   { return (int)*ppc_src_num(); }
static int ppc_list_entries(void) { return ppc_list_count() + 1; }

static void ppc_clamp_list(void) {
    int total = ppc_list_entries();
    if (s_list_cursor < 0) s_list_cursor = 0;
    if (s_list_cursor >= total) s_list_cursor = total - 1;
    if (s_list_scroll > s_list_cursor) s_list_scroll = s_list_cursor;
    if (s_list_cursor >= s_list_scroll + 4) s_list_scroll = s_list_cursor - 3;
    if (s_list_scroll < 0) s_list_scroll = 0;
}

static void ppc_draw_list(void) {
    static const char *prompt[3][2] = {
        { "What do you want", "to withdraw?" },
        { "What do you want", "to deposit?" },
        { "What do you want", "to toss away?" },
    };
    const uint8_t *items = ppc_src_items();
    int count = ppc_list_count();

    ppc_clear_overlay();
    hWY = 0;

    ppc_draw_menu_frame(CHAR_ARROW_HOLLOW);

    ppc_box(0, 12, 19, 17);
    ppc_str(1, 14, prompt[s_op][0]);
    ppc_str(1, 16, prompt[s_op][1]);

    ppc_box(4, 2, 19, 12);
    for (int i = 0; i < 4; i++) {
        int idx = s_list_scroll + i;
        int name_row = 4 + i * 2;
        if (idx > count) continue;
        ppc_put(5, name_row, (uint8_t)Font_CharToTile(
            idx == s_list_cursor ? CHAR_ARROW :
            idx == s_swap_marker ? CHAR_ARROW_HOLLOW : CHAR_SPACE));
        if (idx == count) { ppc_str(6, name_row, "CANCEL"); continue; }
        uint8_t id  = items[idx * 2];
        uint8_t qty = items[idx * 2 + 1];
        ppc_item_name(6, name_row, id, 12);

        if (!Inventory_IsKeyItem(id))
            ppc_qty2(14, name_row + 1, qty);
    }

    if (s_list_scroll + 3 < count)
        ppc_put(18, 11, (uint8_t)Font_CharToTile(CHAR_DOWN));
}

static void ppc_handle_select(void) {
    if (s_list_cursor >= ppc_list_count()) return;
    if (s_swap_marker < 0) {
        s_swap_marker = s_list_cursor;
        ppc_draw_list();
        return;
    }
    if (s_swap_marker == s_list_cursor) return;

    uint8_t *items = ppc_src_items();
    int ia = s_swap_marker, ib = s_list_cursor;
    uint8_t id_a = items[ia * 2], qty_a = items[ia * 2 + 1];
    uint8_t id_b = items[ib * 2], qty_b = items[ib * 2 + 1];

    if (id_a != id_b) {
        items[ia * 2]     = id_b;  items[ia * 2 + 1] = qty_b;
        items[ib * 2]     = id_a;  items[ib * 2 + 1] = qty_a;
    } else {
        int sum = (int)qty_a + (int)qty_b;
        if (sum <= 99) {
            items[ib * 2 + 1] = (uint8_t)sum;
            int n = ppc_list_count();
            for (int i = ia; i < n - 1; i++) {
                items[i * 2]     = items[(i + 1) * 2];
                items[i * 2 + 1] = items[(i + 1) * 2 + 1];
            }
            items[(n - 1) * 2] = 0xFF;
            *ppc_src_num() = (uint8_t)(n - 1);
            s_list_scroll = 0;
            s_list_cursor = 0;
        } else {
            items[ia * 2 + 1] = (uint8_t)(sum - 99);
            items[ib * 2 + 1] = 99;
        }
    }
    s_swap_marker = -1;
    ppc_clamp_list();
    ppc_draw_list();
}

static void ppc_draw_qty(void) {
    ppc_box(15, 9, 19, 11);
    ppc_qty2(16, 10, s_qty);
}

static void ppc_msg_to_menu(const char *s) { Text_InstantNext(); Text_ShowASCII(s); s_mode = PPC_WAIT_TO_MENU; }
static void ppc_msg_to_list(const char *s) { Text_InstantNext(); Text_ShowASCII(s); s_mode = PPC_WAIT_TO_LIST; }

static void ppc_do_move(void) {
    char name[16];
    Inventory_DecodeASCII((uint8_t)s_sel_item, name, sizeof(name));

    if (s_op == OP_WITHDRAW) {
        if (Inventory_AddTo(&wNumBagItems, wBagItems, BAG_ITEM_CAPACITY,
                            (uint8_t)s_sel_item, (uint8_t)s_qty) != 0) {
            ppc_msg_to_list(RomText("_CantCarryMoreText"));
            return;
        }
        Inventory_RemoveFrom(&wNumBoxItems, wBoxItems, (uint8_t)s_sel_item, (uint8_t)s_qty);
        Audio_PlaySFX_WithdrawDeposit();
        snprintf(s_msg, sizeof(s_msg), "Withdrew\n%s.", name);
        ppc_msg_to_list(s_msg);
    } else if (s_op == OP_DEPOSIT) {
        if (Inventory_AddTo(&wNumBoxItems, wBoxItems, PC_ITEM_CAPACITY,
                            (uint8_t)s_sel_item, (uint8_t)s_qty) != 0) {
            ppc_msg_to_list(RomText("_NoRoomToStoreText"));
            return;
        }
        Inventory_RemoveFrom(&wNumBagItems, wBagItems, (uint8_t)s_sel_item, (uint8_t)s_qty);
        Audio_PlaySFX_WithdrawDeposit();
        snprintf(s_msg, sizeof(s_msg), "%s was\nstored via PC.", name);
        ppc_msg_to_list(s_msg);
    } else {
        snprintf(s_msg, sizeof(s_msg), "Is it OK to toss\n%s?", name);
        Text_InstantNext();
        YesNo_Show(s_msg);
        s_mode = PPC_WAIT_TOSS;
    }
}

static void ppc_choose_item(uint8_t id) {
    s_sel_item = id;
    int is_key = Inventory_IsKeyItem(id);
    int is_hm  = (id >= HM01 && id < TM01);

    if (s_op == OP_TOSS && (is_key || is_hm)) {
        ppc_msg_to_list(RomText("_TooImportantToTossText"));
        return;
    }
    if (is_key) { s_qty = 1; ppc_do_move(); return; }

    s_qty = 1;
    s_qty_max = (int)ppc_src_items()[s_list_cursor * 2 + 1];
    ppc_draw_list();
    ppc_draw_qty();
    s_mode = PPC_QTY;
}

void PlayersPC_Activate(void) {
    if (s_mode != PPC_CLOSED || PCMenu_IsOpen()) return;
    char name[NAME_LENGTH + 2];
    ppc_clear_overlay();
    s_menu_cursor = 0;
    Audio_PlaySFX_TurnOnPC();
    ppc_player_name_ascii(name, sizeof(name));
    snprintf(s_msg, sizeof(s_msg), "%s turned on\nthe PC.", name);

    s_boot_delay = 3;
    s_mode = PPC_BOOT_DELAY;
}

void PlayersPC_ActivateFromPCMenu(void) {
    if (s_mode != PPC_CLOSED) return;
    s_from_pc_menu = 1;
    ppc_clear_overlay();
    s_menu_cursor = 0;
    ppc_draw_menu();
    s_mode = PPC_MENU;
}

int PlayersPC_IsOpen(void) { return s_mode != PPC_CLOSED; }

void PlayersPC_Tick(void) {
    if (s_mode == PPC_CLOSED) return;

    switch (s_mode) {
    case PPC_BOOT_DELAY:

        hWY = 12 * TILE_PX;
        ppc_box(0, 12, 19, 17);
        if (s_boot_delay-- > 0) return;
        Text_InstantNext();
        Text_ShowASCII(s_msg);
        s_mode = PPC_WAIT_BOOT;
        return;

    case PPC_WAIT_BOOT:
        ppc_draw_menu();
        s_mode = PPC_MENU;
        return;

    case PPC_WAIT_TO_MENU:
        ppc_draw_menu();
        s_mode = PPC_MENU;
        return;

    case PPC_WAIT_TO_LIST:
        s_swap_marker = -1;
        ppc_clamp_list();
        ppc_draw_list();
        s_mode = PPC_LIST;
        return;

    case PPC_WAIT_TOSS: {
        char name[16];
        if (YesNo_GetResult()) {
            Inventory_DecodeASCII((uint8_t)s_sel_item, name, sizeof(name));
            Inventory_RemoveFrom(&wNumBoxItems, wBoxItems, (uint8_t)s_sel_item, (uint8_t)s_qty);
            snprintf(s_msg, sizeof(s_msg), "Threw away\n%s.", name);
            ppc_msg_to_list(s_msg);
        } else {
            s_swap_marker = -1;
            ppc_clamp_list();
            ppc_draw_list();
            s_mode = PPC_LIST;
        }
        return;
    }

    case PPC_MENU:
        if (hJoyPressed & PAD_UP)   { s_menu_cursor = (s_menu_cursor + 3) % 4; ppc_draw_menu(); return; }
        if (hJoyPressed & PAD_DOWN) { s_menu_cursor = (s_menu_cursor + 1) % 4; ppc_draw_menu(); return; }
        if (hJoyPressed & PAD_B) {
            if (!s_from_pc_menu) Audio_PlaySFX_TurnOffPC();
            ppc_restore_overworld();
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (s_menu_cursor == 3) {
                if (!s_from_pc_menu) Audio_PlaySFX_TurnOffPC();
                ppc_restore_overworld();
                return;
            }
            s_op = (ppc_op_t)s_menu_cursor;
            if (ppc_list_count() == 0) {
                ppc_msg_to_menu(s_op == OP_DEPOSIT ? RomText("_NothingToDepositText")
                                                   : RomText("_NothingStoredText"));
                return;
            }
            s_list_cursor = 0;
            s_list_scroll = 0;
            s_swap_marker = -1;
            ppc_draw_list();
            s_mode = PPC_LIST;
            return;
        }
        return;

    case PPC_LIST: {
        int total = ppc_list_entries();
        if (hJoyPressed & PAD_UP)   { s_list_cursor = (s_list_cursor + total - 1) % total; ppc_clamp_list(); ppc_draw_list(); return; }
        if (hJoyPressed & PAD_DOWN) { s_list_cursor = (s_list_cursor + 1) % total;         ppc_clamp_list(); ppc_draw_list(); return; }
        if (hJoyPressed & PAD_SELECT) { ppc_handle_select(); return; }
        if (hJoyPressed & PAD_B) { ppc_draw_menu(); s_mode = PPC_MENU; return; }
        if (hJoyPressed & PAD_A) {
            if (s_list_cursor >= ppc_list_count()) { ppc_draw_menu(); s_mode = PPC_MENU; return; }
            ppc_choose_item(ppc_src_items()[s_list_cursor * 2]);
            return;
        }
        return;
    }

    case PPC_QTY:
        if (hJoyPressed & PAD_UP)   { if (++s_qty > s_qty_max) s_qty = 1;         ppc_draw_qty(); return; }
        if (hJoyPressed & PAD_DOWN) { if (--s_qty < 1) s_qty = s_qty_max;         ppc_draw_qty(); return; }
        if (hJoyPressed & PAD_B) { s_swap_marker = -1; ppc_draw_list(); s_mode = PPC_LIST; return; }
        if (hJoyPressed & PAD_A) { ppc_do_move(); return; }
        return;

    default:
        return;
    }
}
