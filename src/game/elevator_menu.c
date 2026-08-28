#include "elevator_menu.h"
#include "rom_text.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "warp.h"
#include "inventory.h"
#include "money_box.h"
#include "../data/font_data.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include <stdio.h>

#define BOX_COL_L  4
#define BOX_COL_R 19
#define BOX_ROW_T  2
#define BOX_ROW_B 12

#define ARROW_COL  5
#define ITEM_COL   6
#define ITEM_ROW0  4
#define ITEM_STEP  2

#define VEND_BOX_COL_L  0
#define VEND_BOX_COL_R 13
#define VEND_BOX_ROW_T  3
#define VEND_BOX_ROW_B 12
#define VEND_ARROW_COL  1
#define VEND_ITEM_COL   2
#define VEND_ITEM_ROW0  5
#define VEND_PRICE_COL  9
#define VEND_PRICE_ROW0 6

#define CHAR_TERM  0x50
#define CHAR_SPACE 0x7F
#define CHAR_ARROW 0xED

typedef struct {
    const char *name;
    int x;
    int y;
    const uint8_t *label;
} named_floor_t;

static const uint8_t kLblB1F[] = { 0x81, 0xF7, 0x85, CHAR_TERM };
static const uint8_t kLblB2F[] = { 0x81, 0xF8, 0x85, CHAR_TERM };
static const uint8_t kLblB4F[] = { 0x81, 0xFA, 0x85, CHAR_TERM };
static const uint8_t kLblBlank[] = { CHAR_TERM };
static const uint8_t kLbl1F[] = { 0xF7, 0x85, CHAR_TERM };
static const uint8_t kLbl2F[] = { 0xF8, 0x85, CHAR_TERM };
static const uint8_t kLbl3F[] = { 0xF9, 0x85, CHAR_TERM };
static const uint8_t kLbl4F[] = { 0xFA, 0x85, CHAR_TERM };
static const uint8_t kLbl5F[] = { 0xFB, 0x85, CHAR_TERM };
static const uint8_t kLbl6F[] = { 0xFC, 0x85, CHAR_TERM };
static const uint8_t kLbl7F[] = { 0xFD, 0x85, CHAR_TERM };
static const uint8_t kLbl8F[] = { 0xFE, 0x85, CHAR_TERM };
static const uint8_t kLbl9F[] = { 0xFF, 0x85, CHAR_TERM };
static const uint8_t kLbl10F[] = { 0xF7, 0xF6, 0x85, CHAR_TERM };
static const uint8_t kLbl11F[] = { 0xF7, 0xF7, 0x85, CHAR_TERM };
static const uint8_t kLblCancel[] = { 0x82, 0x80, 0x8D, 0x82, 0x84, 0x8B, CHAR_TERM };
static const uint8_t kLblFreshWater[] = { 0x85,0x91,0x84,0x92,0x87,0x7F,0x96,0x80,0x93,0x84,0x91, CHAR_TERM };
static const uint8_t kLblSodaPop[] = { 0x92,0x8E,0x83,0x80,0x7F,0x8F,0x8E,0x8F, CHAR_TERM };
static const uint8_t kLblLemonade[] = { 0x8B,0x84,0x8C,0x8E,0x8D,0x80,0x83,0x84, CHAR_TERM };

static const uint8_t kLblPrice200[] = { 0xF0, 0xF8, 0xF6, 0xF6, CHAR_TERM };
static const uint8_t kLblPrice300[] = { 0xF0, 0xF9, 0xF6, 0xF6, CHAR_TERM };
static const uint8_t kLblPrice350[] = { 0xF0, 0xF9, 0xFB, 0xF6, CHAR_TERM };

static const named_floor_t kRocketChoices[3] = {
    { "RocketHideoutB1F", 24, 19, kLblB1F },
    { "RocketHideoutB2F", 24, 19, kLblB2F },
    { "RocketHideoutB4F", 24, 15, kLblB4F },
};

static const named_floor_t kCeladonChoices[5] = {
    { "CeladonMart1F", 1, 1, kLbl1F },
    { "CeladonMart2F", 1, 1, kLbl2F },
    { "CeladonMart3F", 1, 1, kLbl3F },
    { "CeladonMart4F", 1, 1, kLbl4F },
    { "CeladonMart5F", 1, 1, kLbl5F },
};

static const named_floor_t kSilphChoices[11] = {
    { "SilphCo1F",  20, 0, kLbl1F },
    { "SilphCo2F",  20, 0, kLbl2F },
    { "SilphCo3F",  20, 0, kLbl3F },
    { "SilphCo4F",  20, 0, kLbl4F },
    { "SilphCo5F",  20, 0, kLbl5F },
    { "SilphCo6F",  18, 0, kLbl6F },
    { "SilphCo7F",  18, 0, kLbl7F },
    { "SilphCo8F",  18, 0, kLbl8F },
    { "SilphCo9F",  18, 0, kLbl9F },
    { "SilphCo10F", 12, 0, kLbl10F },
    { "SilphCo11F", 13, 0, kLbl11F },
};

static int g_open = 0;
static int g_mode = 0;
static int g_cursor = 0;
static int g_scroll_top = 0;
static int g_wait_a_release = 0;
static int g_wait_neutral = 0;
static uint8_t g_prev_held = 0;
static int g_open_queued = 0;
static int g_pending_tp = 0;
static uint8_t g_tp_map = 0;
static int g_tp_x = 0;
static int g_tp_y = 0;
static int g_shake_phase = 0;
static int g_shake_delay = 0;
static int g_shake_timer = 0;
static int g_shake_loops = 0;
static int g_shake_off = 1;
static uint8_t g_shake_base_scy = 0;
static int g_shake_base_scroll_y = 0;
static int g_pending_vmap = 0;
static char g_pending_vmap_name[32];
static int g_pending_vmap_x = 0;
static int g_pending_vmap_y = 0;

enum {
    ELEV_SHAKE_IDLE = 0,
    ELEV_SHAKE_PRE_DELAY,
    ELEV_SHAKE_RUN,
    ELEV_SHAKE_DING_WAIT,
    ELEV_VEND_SFX_LOOP,
    ELEV_VEND_TEXT_WAIT,
};
static uint8_t g_vend_item = 0;
static uint16_t g_vend_price = 0;
static int g_vend_sfx_loops = 0;
static int g_vend_sfx_timer = 0;
static char g_vend_msg[40];

static void smset(int col, int row, uint8_t tile) {
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void draw_box(void) {
    int l = (g_mode == 2) ? VEND_BOX_COL_L : BOX_COL_L;
    int r_ = (g_mode == 2) ? VEND_BOX_COL_R : BOX_COL_R;
    int t = (g_mode == 2) ? VEND_BOX_ROW_T : BOX_ROW_T;
    int b = (g_mode == 2) ? VEND_BOX_ROW_B : BOX_ROW_B;

    smset(l, t, (uint8_t)Font_CharToTile(0x79));
    for (int c = l + 1; c < r_; c++)
        smset(c, t, (uint8_t)Font_CharToTile(0x7A));
    smset(r_, t, (uint8_t)Font_CharToTile(0x7B));

    for (int row = t + 1; row < b; row++) {
        smset(l, row, (uint8_t)Font_CharToTile(0x7C));
        for (int c = l + 1; c < r_; c++)
            smset(c, row, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(r_, row, (uint8_t)Font_CharToTile(0x7C));
    }

    smset(l, b, (uint8_t)Font_CharToTile(0x7D));
    for (int c = l + 1; c < r_; c++)
        smset(c, b, (uint8_t)Font_CharToTile(0x7A));
    smset(r_, b, (uint8_t)Font_CharToTile(0x7E));
}

static void print_str(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++)
        smset(col, row, (uint8_t)Font_CharToTile(*s));
}

static int menu_choice_count(void) {
    if (g_mode == 1) return 3;
    if (g_mode == 3) return 11;
    if (g_mode == 4) return 5;
    return 3;
}

static const named_floor_t *menu_named_choices(void) {
    if (g_mode == 1) return kRocketChoices;
    if (g_mode == 3) return kSilphChoices;
    if (g_mode == 4) return kCeladonChoices;
    return NULL;
}

static const uint8_t *menu_label_at(int idx) {
    const named_floor_t *t = menu_named_choices();
    return t ? t[idx].label : kLblBlank;
}

static void draw_items(void) {
    if (g_mode == 1 || g_mode == 3 || g_mode == 4) {
        int total = menu_choice_count();
        for (int i = 0; i < 4; i++) {
            int idx = g_scroll_top + i;
            if (idx < total) {
                print_str(ITEM_COL, ITEM_ROW0 + i * ITEM_STEP, menu_label_at(idx));
            } else if (idx == total) {
                print_str(ITEM_COL, ITEM_ROW0 + i * ITEM_STEP, kLblCancel);
            } else {
                print_str(ITEM_COL, ITEM_ROW0 + i * ITEM_STEP, kLblBlank);
            }
        }
    } else {
        print_str(VEND_ITEM_COL, VEND_ITEM_ROW0 + 0 * ITEM_STEP, kLblFreshWater);
        print_str(VEND_ITEM_COL, VEND_ITEM_ROW0 + 1 * ITEM_STEP, kLblSodaPop);
        print_str(VEND_ITEM_COL, VEND_ITEM_ROW0 + 2 * ITEM_STEP, kLblLemonade);
        print_str(VEND_ITEM_COL, VEND_ITEM_ROW0 + 3 * ITEM_STEP, kLblCancel);
        print_str(VEND_PRICE_COL, VEND_PRICE_ROW0 + 0 * ITEM_STEP, kLblPrice200);
        print_str(VEND_PRICE_COL, VEND_PRICE_ROW0 + 1 * ITEM_STEP, kLblPrice300);
        print_str(VEND_PRICE_COL, VEND_PRICE_ROW0 + 2 * ITEM_STEP, kLblPrice350);
    }
}

static void set_cursor(int row_idx, uint8_t tile) {
    if (g_mode == 2)
        smset(VEND_ARROW_COL, VEND_ITEM_ROW0 + row_idx * ITEM_STEP, tile);
    else
        smset(ARROW_COL, ITEM_ROW0 + row_idx * ITEM_STEP, tile);
}

static void redraw_menu_body(void) {
    draw_box();
    draw_items();
    set_cursor(g_cursor - g_scroll_top, (uint8_t)Font_CharToTile(CHAR_ARROW));
}

void ElevatorMenu_OpenRocketHideout(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open = 1;
    g_mode = 1;
    g_cursor = 0;
    g_scroll_top = 0;
    g_wait_a_release = 1;
    g_wait_neutral = 1;
    g_prev_held = 0xFF;
    g_open_queued = 0;
    g_pending_tp = 0;

    for (int i = 4; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;

    for (int i = 0; i < 4; i++) wShadowOAM[i].y = 0;
    draw_box();
    draw_items();
    set_cursor(0, (uint8_t)Font_CharToTile(CHAR_ARROW));

    hJoyPressed = 0;
}

static void open_silph_elevator(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open = 1;
    g_mode = 3;
    g_cursor = 0;
    g_scroll_top = 0;
    g_wait_a_release = 1;
    g_wait_neutral = 1;
    g_prev_held = 0xFF;
    g_open_queued = 0;
    g_pending_tp = 0;
    for (int i = 4; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    for (int i = 0; i < 4; i++) wShadowOAM[i].y = 0;
    draw_box();
    draw_items();
    set_cursor(0, (uint8_t)Font_CharToTile(CHAR_ARROW));
    hJoyPressed = 0;
}

static void open_celadon_mart_elevator(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open = 1;
    g_mode = 4;
    g_cursor = 0;
    g_scroll_top = 0;
    g_wait_a_release = 1;
    g_wait_neutral = 1;
    g_prev_held = 0xFF;
    g_open_queued = 0;
    g_pending_tp = 0;
    for (int i = 4; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    for (int i = 0; i < 4; i++) wShadowOAM[i].y = 0;
    draw_box();
    draw_items();
    set_cursor(0, (uint8_t)Font_CharToTile(CHAR_ARROW));
    hJoyPressed = 0;
}

void ElevatorMenu_QueueOpenRocketHideout(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open_queued = 1;
}

void ElevatorMenu_QueueOpenSilphCo(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open_queued = 3;
}

void ElevatorMenu_QueueOpenVending(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open_queued = 2;
}

void ElevatorMenu_QueueOpenCeladonMart(void) {
    g_shake_phase = ELEV_SHAKE_IDLE;
    g_open_queued = 4;
}

void CeladonMartElevator_PanelInteract(void) {
    wDoNotWaitForButtonPress = 1;
    Text_ShowASCII(RomText("_WhichFloorText"));
    ElevatorMenu_QueueOpenCeladonMart();
}

void SilphCoElevator_PanelInteract(void) {
    wDoNotWaitForButtonPress = 1;
    Text_ShowASCII(RomText("_WhichFloorText"));
    ElevatorMenu_QueueOpenSilphCo();
}

void ElevatorMenu_TryOpenQueued(void) {
    if (!g_open_queued) return;
    if (g_open || Text_IsOpen()) return;
    Text_BlitBoxToBGAndHideWindow();
    if (g_open_queued == 1) {
        ElevatorMenu_OpenRocketHideout();
    } else if (g_open_queued == 3) {
        open_silph_elevator();
    } else if (g_open_queued == 4) {
        open_celadon_mart_elevator();
    } else {
        g_open = 1;
        g_mode = 2;
        g_cursor = 0;
        g_scroll_top = 0;
        g_wait_a_release = 1;
        g_wait_neutral = 1;
        g_prev_held = 0xFF;
        g_pending_tp = 0;
        for (int i = 4; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
        for (int i = 0; i < 4; i++) wShadowOAM[i].y = 0;
        draw_box();
        draw_items();
        set_cursor(0, (uint8_t)Font_CharToTile(CHAR_ARROW));

        MoneyBox_Draw();
        hJoyPressed = 0;
    }
    g_open_queued = 0;
}

void ElevatorMenu_Tick(void) {
    uint8_t held = (uint8_t)(hJoyHeld & (PAD_A | PAD_B | PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT));
    uint8_t pressed_local = (uint8_t)(held & (uint8_t)~g_prev_held);
    g_prev_held = held;

    if (g_shake_phase != ELEV_SHAKE_IDLE) {
        if (g_shake_phase == ELEV_VEND_SFX_LOOP) {

            if (--g_vend_sfx_timer <= 0) {
                g_vend_sfx_timer = 2;
                Audio_PlaySFX_PushBoulder();
                if (--g_vend_sfx_loops <= 0) {
                    char name[16];
                    Inventory_DecodeASCII(g_vend_item, name, sizeof(name));
                    snprintf(g_vend_msg, sizeof(g_vend_msg), "%s\npopped out!@", name);
                    Text_ShowASCII(g_vend_msg);
                    g_shake_phase = ELEV_VEND_TEXT_WAIT;
                }
            }
            return;
        }
        if (g_shake_phase == ELEV_VEND_TEXT_WAIT) {

            if (!Text_IsOpen()) {
                MoneyBox_Clear();
                g_shake_phase = ELEV_SHAKE_IDLE;
                g_mode = 0;
            }
            return;
        }
        if (g_shake_phase == ELEV_SHAKE_PRE_DELAY) {
            if (--g_shake_delay <= 0) {
                g_shake_phase = ELEV_SHAKE_RUN;
                g_shake_loops = 100;
                g_shake_timer = 2;
            }
            return;
        }
        if (g_shake_phase == ELEV_SHAKE_RUN) {
            if (--g_shake_timer <= 0) {
                g_shake_timer = 2;
                g_shake_off = -g_shake_off;
                hSCY = (uint8_t)(g_shake_base_scy + g_shake_off);
                gScrollPxY = g_shake_base_scroll_y + g_shake_off;
                NPC_BuildView(gScrollPxX, gScrollPxY);
                Player_SyncOAM();
                Audio_PlaySFX_CollisionRetrigger();
                if (--g_shake_loops <= 0) {
                    hSCY = g_shake_base_scy;
                    gScrollPxY = g_shake_base_scroll_y;
                    Player_SyncOAM();
                    Audio_PlaySFX_SafariZonePA();
                    g_shake_phase = ELEV_SHAKE_DING_WAIT;
                }
            }
            return;
        }
        if (g_shake_phase == ELEV_SHAKE_DING_WAIT) {
            if (!Audio_IsSFXPlaying()) {
                g_shake_phase = ELEV_SHAKE_IDLE;
                if (g_pending_vmap) {

                    g_pending_vmap = 0;
                    Warp_SetVmapElevatorDestination(g_pending_vmap_name, g_pending_vmap_x, g_pending_vmap_y);
                }
                Map_BuildScrollView();
                NPC_BuildView(0, 0);
            }
            return;
        }
    }

    if (!g_open) return;
    if (g_wait_a_release) {
        if (held & PAD_A)
            return;
        g_wait_a_release = 0;
        return;
    }

    if (g_wait_neutral) {
        if (held != 0)
            return;
        g_wait_neutral = 0;
        g_prev_held = 0;
        return;
    }

    int total_entries = (g_mode == 1 || g_mode == 3 || g_mode == 4) ? (menu_choice_count() + 1) : 4;
    if (pressed_local & PAD_UP) {
        g_cursor = (g_cursor == 0) ? (total_entries - 1) : (g_cursor - 1);
        if (g_cursor < g_scroll_top) g_scroll_top = g_cursor;
        if (g_cursor > g_scroll_top + 3) g_scroll_top = g_cursor - 3;
        redraw_menu_body();
        return;
    }
    if (pressed_local & PAD_DOWN) {
        g_cursor = (g_cursor == (total_entries - 1)) ? 0 : (g_cursor + 1);
        if (g_cursor > g_scroll_top + 3) g_scroll_top = g_cursor - 3;
        if (g_cursor < g_scroll_top) g_scroll_top = g_cursor;
        redraw_menu_body();
        return;
    }
    if (pressed_local & PAD_B) {
        Audio_PlaySFX_PressAB();
        Text_Close();
        g_open = 0;
        Player_SyncOAM();
        g_pending_tp = 0;
        if (g_mode == 2) MoneyBox_Clear();
        Map_BuildScrollView();
        NPC_BuildView(0, 0);
        return;
    }
    if (pressed_local & PAD_A) {
        if (g_cursor == total_entries - 1) {
            Audio_PlaySFX_PressAB();
            Text_Close();
            g_open = 0;
            Player_SyncOAM();
            g_pending_tp = 0;
            if (g_mode == 2) MoneyBox_Clear();
            Map_BuildScrollView();
            NPC_BuildView(0, 0);
            if (g_mode == 2) Text_ShowASCII(RomText("_VendingMachineText7"));
            g_mode = 0;
            return;
        }
        if (g_mode == 1 || g_mode == 3 || g_mode == 4) {
            const named_floor_t *f = &menu_named_choices()[g_cursor];
            Audio_PlaySFX_PressAB();
            Text_Close();
            g_pending_vmap = 1;
            snprintf(g_pending_vmap_name, sizeof(g_pending_vmap_name), "%s", f->name);
            g_pending_vmap_x = f->x;
            g_pending_vmap_y = f->y;
            g_shake_base_scy = hSCY;
            g_shake_base_scroll_y = gScrollPxY;
            g_shake_off = 1;
            g_shake_delay = 3;
            g_shake_phase = ELEV_SHAKE_PRE_DELAY;
            g_open = 0;
            Player_SyncOAM();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            return;
        }

        g_vend_item = (g_cursor == 0) ? 0x3C : (g_cursor == 1) ? 0x3D : 0x3E;
        g_vend_price = (g_cursor == 0) ? 200 : (g_cursor == 1) ? 300 : 350;
        uint32_t money = (uint32_t)(
            ((wPlayerMoney[0] >> 4) & 0xF) * 100000u +
            (wPlayerMoney[0] & 0xF) * 10000u +
            ((wPlayerMoney[1] >> 4) & 0xF) * 1000u +
            (wPlayerMoney[1] & 0xF) * 100u +
            ((wPlayerMoney[2] >> 4) & 0xF) * 10u +
            (wPlayerMoney[2] & 0xF));
        Audio_PlaySFX_PressAB();
        Text_Close();
        g_open = 0;
        Player_SyncOAM();
        g_pending_tp = 0;
        Map_BuildScrollView();
        NPC_BuildView(0, 0);

        if (money < 200) {
            g_mode = 0;
            MoneyBox_Clear();
            Text_ShowASCII(RomText("_VendingMachineText4"));
            return;
        }
        if (Inventory_Add(g_vend_item, 1) != 0) {
            g_mode = 0;
            MoneyBox_Clear();
            Text_ShowASCII(RomText("_VendingMachineText6"));
            return;
        }
        money -= g_vend_price;
        wPlayerMoney[0] = (uint8_t)(((money / 100000u) << 4) | ((money / 10000u) % 10u));
        wPlayerMoney[1] = (uint8_t)((((money / 1000u) % 10u) << 4) | ((money / 100u) % 10u));
        wPlayerMoney[2] = (uint8_t)((((money / 10u) % 10u) << 4) | (money % 10u));
        g_vend_sfx_loops = 60;
        g_vend_sfx_timer = 2;
        g_shake_phase = ELEV_VEND_SFX_LOOP;
        return;
    }
}

int ElevatorMenu_IsOpen(void) {
    return g_open;
}

int ElevatorMenu_IsBusy(void) {
    return g_shake_phase != ELEV_SHAKE_IDLE;
}

int ElevatorMenu_ConsumeTeleport(uint8_t *map_out, int *x_out, int *y_out) {
    if (!g_pending_tp) return 0;
    g_pending_tp = 0;
    *map_out = g_tp_map;
    *x_out = g_tp_x;
    *y_out = g_tp_y;
    return 1;
}
