
#include "badge_house_menu.h"
#include "text.h"
#include "rom_text.h"
#include "npc.h"
#include "player.h"
#include "overworld.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../data/font_data.h"

#define BC_TL  0x79u
#define BC_H   0x7Au
#define BC_TR  0x7Bu
#define BC_V   0x7Cu
#define BC_BL  0x7Du
#define BC_BR  0x7Eu
#define BC_SP  0x7Fu

#define BOX_L  4
#define BOX_R 19
#define BOX_T  2
#define BOX_B 12
#define ARROW_COL  5
#define ITEM_COL   6
#define ITEM_ROW0  4
#define ITEM_STEP  2
#define VISIBLE_ROWS 4

#define NUM_BADGES 8

static const char *kBadgeNames[NUM_BADGES] = {
    "BOULDERBADGE", "CASCADEBADGE", "THUNDERBADGE", "RAINBOWBADGE",
    "SOULBADGE", "MARSHBADGE", "VOLCANOBADGE", "EARTHBADGE",
};
static const char kLabelCancel[] = "CANCEL";

static const char kTextIntro[] =
    "#MON BADGEs\nare owned only by\nskilled trainers.\f"
    "I see you have\nat least one.\f"
    "Those BADGEs have\namazing secrets!";

static const char kTextPrompt[] =
    "Now then...\f"
    "Which of the 8\nBADGEs should I\f"
    "BADGEs should I\ndescribe?";

static const char kTextOutro[] =
    "CeruleanBadgeHouseMiddleAgedManText.VisitAnyTimeText";

static const char *kBadgeText[NUM_BADGES] = {
    "CeruleanBadgeHouseBoulderBadgeText",
    "CeruleanBadgeHouseCascadeBadgeText",
    "CeruleanBadgeHouseThunderBadgeText",
    "CeruleanBadgeHouseRainbowBadgeText",
    "CeruleanBadgeHouseSoulBadgeText",
    "CeruleanBadgeHouseMarshBadgeText",
    "CeruleanBadgeHouseVolcanoBadgeText",
    "CeruleanBadgeHouseEarthBadgeText",
};

typedef enum {
    BHM_CLOSED = 0,
    BHM_INTRO,
    BHM_PROMPT,
    BHM_MENU,
    BHM_DESC,
    BHM_OUTRO,
} BHMState;

static BHMState s_state      = BHM_CLOSED;
static int      s_cursor     = 0;
static int      s_scroll_top = 0;

static void bhm_set(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static uint8_t bhm_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == ' ') return (uint8_t)Font_CharToTile(BC_SP);
    if (c == '-') return (uint8_t)Font_CharToTile(0xE3);
    return (uint8_t)Font_CharToTile(BC_SP);
}

static void bhm_str(int col, int row, const char *s) {
    for (; *s; s++, col++) bhm_set(col, row, bhm_tile((unsigned char)*s));
}

static void bhm_box(int l, int t, int r, int b) {
    bhm_set(l, t, (uint8_t)Font_CharToTile(BC_TL));
    for (int c = l + 1; c < r; c++) bhm_set(c, t, (uint8_t)Font_CharToTile(BC_H));
    bhm_set(r, t, (uint8_t)Font_CharToTile(BC_TR));
    for (int y = t + 1; y < b; y++) {
        bhm_set(l, y, (uint8_t)Font_CharToTile(BC_V));
        for (int c = l + 1; c < r; c++) bhm_set(c, y, (uint8_t)Font_CharToTile(BC_SP));
        bhm_set(r, y, (uint8_t)Font_CharToTile(BC_V));
    }
    bhm_set(l, b, (uint8_t)Font_CharToTile(BC_BL));
    for (int c = l + 1; c < r; c++) bhm_set(c, b, (uint8_t)Font_CharToTile(BC_H));
    bhm_set(r, b, (uint8_t)Font_CharToTile(BC_BR));
}

static void bhm_restore_overworld(void) {
    Map_BuildScrollView();
    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void bhm_draw_list(void) {
    bhm_box(BOX_L, BOX_T, BOX_R, BOX_B);
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = s_scroll_top + i;
        int row = ITEM_ROW0 + i * ITEM_STEP;
        const char *label =
            (idx < NUM_BADGES) ? kBadgeNames[idx] :
            (idx == NUM_BADGES) ? kLabelCancel : NULL;
        bhm_set(ARROW_COL, row, (uint8_t)Font_CharToTile(
            (idx == s_cursor) ? 0xED : BC_SP));
        if (label) bhm_str(ITEM_COL, row, label);
    }
}

void BadgeHouseMenu_Open(void) {
    s_cursor = 0;
    s_scroll_top = 0;

    Text_ShowASCII(kTextIntro);
    s_state = BHM_INTRO;
}

int BadgeHouseMenu_IsOpen(void) {
    return s_state != BHM_CLOSED;
}

void BadgeHouseMenu_Tick(void) {
    switch (s_state) {

    case BHM_CLOSED:
        return;

    case BHM_INTRO:
        if (Text_IsOpen()) { Text_Update(); return; }
        Text_KeepTilesOnClose();
        Text_ShowASCII(kTextPrompt);
        s_state = BHM_PROMPT;
        return;

    case BHM_PROMPT:
        if (Text_IsOpen()) { Text_Update(); return; }

        for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
        Map_BuildScrollView();
        bhm_draw_list();
        s_state = BHM_MENU;
        return;

    case BHM_MENU: {
        int total = NUM_BADGES + 1;
        if (hJoyPressed & PAD_UP) {
            s_cursor = (s_cursor == 0) ? (total - 1) : (s_cursor - 1);
            if (s_cursor < s_scroll_top) s_scroll_top = s_cursor;
            if (s_cursor > s_scroll_top + VISIBLE_ROWS - 1) s_scroll_top = s_cursor - (VISIBLE_ROWS - 1);
            bhm_draw_list();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            s_cursor = (s_cursor == total - 1) ? 0 : (s_cursor + 1);
            if (s_cursor > s_scroll_top + VISIBLE_ROWS - 1) s_scroll_top = s_cursor - (VISIBLE_ROWS - 1);
            if (s_cursor < s_scroll_top) s_scroll_top = s_cursor;
            bhm_draw_list();
            return;
        }
        if (hJoyPressed & PAD_B) {
            Text_ShowASCII(RomText(kTextOutro));
            s_state = BHM_OUTRO;
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (s_cursor == NUM_BADGES) {

                Text_ShowASCII(RomText(kTextOutro));
                s_state = BHM_OUTRO;
                return;
            }
            Text_ShowASCII(RomText(kBadgeText[s_cursor]));
            s_state = BHM_DESC;
            return;
        }
        return;
    }

    case BHM_DESC:
        if (Text_IsOpen()) { Text_Update(); return; }

        Text_KeepTilesOnClose();
        Text_ShowASCII(kTextPrompt);
        s_state = BHM_PROMPT;
        return;

    case BHM_OUTRO:
        if (Text_IsOpen()) { Text_Update(); return; }
        bhm_restore_overworld();
        s_state = BHM_CLOSED;
        return;
    }
}
