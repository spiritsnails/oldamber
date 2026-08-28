
#include "bills_house_scripts.h"
#include "rom_text.h"
#include "bills_pokemon_list.h"
#include "text.h"
#include "npc.h"
#include "player.h"
#include "overworld.h"
#include "music.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/base_stats.h"
#include "../data/event_constants.h"
#include "../data/font_data.h"
#include "inventory.h"
#include <stdio.h>

#define MAP_BILLS_HOUSE  0x58

#define BILL_POKEMON_NPC  0
#define BILL1_NPC         1
#define BILL2_NPC         2

#define MACHINE_X  1
#define MACHINE_Y  2

#define ITEM_SS_TICKET  0x3F

#define DIR_DOWN   0
#define DIR_UP     1
#define DIR_LEFT   2
#define DIR_RIGHT  3

#define YESNO_COL  14
#define YESNO_ROW   8
#define YESNO_W     6
#define YESNO_H     4

#define BC_TL  0x79u
#define BC_H   0x7Au
#define BC_TR  0x7Bu
#define BC_V   0x7Cu
#define BC_BL  0x7Du
#define BC_BR  0x7Eu
#define BC_SP  0x7Fu
#define BC_CUR 0xEDu

typedef enum {
    BH_DEFAULT = 0,

    BH_POKEMON_TEXT1,
    BH_POKEMON_YESNO,
    BH_POKEMON_NO_TEXT,
    BH_POKEMON_TEXT2,

    BH_POKEMON_WALK,
    BH_POKEMON_ENTERED,

    BH_WAIT_PC,

    BH_PC_TEXT,
    BH_PC_DELAY_PRE,
    BH_PC_SFX1,
    BH_PC_DELAY2,
    BH_PC_SFX2,
    BH_PC_DELAY3,
    BH_PC_SFX3,
    BH_PC_DELAY4,
    BH_PC_SFX4,

    BH_PC_LIST,

    BH_BILL_APPEAR,
    BH_BILL_WALK,
    BH_BILL_CLEANUP,

    BH_BILL1_TEXT1,
    BH_BILL1_JINGLE,
    BH_BILL1_TEXT2,

    BH_BILL1_TEXT_WHY,

    BH_BILL2_TEXT,
} BHState;

static BHState g_state = BH_DEFAULT;
static int     g_frame = 0;

#define MAX_WALK_STEPS 6
static int g_walk_seq[MAX_WALK_STEPS + 1];
static int g_walk_pos = 0;

static const int kBillExitSeq[] = {
    DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_DOWN, -1
};
static int g_exit_pos = 0;
static int g_yesno_cursor = 0;

#define kText_ImNotAPokemon (RomText("BillsHouseBillPokemonText.ImNotAPokemonText"))

#define kText_UseSeparation (RomText("BillsHouseBillPokemonText.UseSeparationSystemText"))

#define kText_NoYouGottaHelp (RomText("BillsHouseBillPokemonText.NoYouGottaHelpText"))

#define kText_Monitor (RomText("BillsHouseMonitorText"))

#define kText_Initiated (RomText("_BillsHouseInitiatedText"))

#define kText_ThankYou (RomText("_BillsHouseBillThankYouText"))

#define kText_SSTicketReceived (RomText("_SSTicketReceivedText"))

#define kText_SSTicketNoRoom (RomText("BillsHouseBillSSTicketText.SSTicketNoRoomText"))

#define kText_WhyDontYouGo (RomText("_BillsHouseBillWhyDontYouGoInsteadOfMeText"))

#define kText_RarePokemon (RomText("BillsHouseBillCheckOutMyRarePokemonText.Text"))

static uint8_t yesno_char(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    return (uint8_t)Font_CharToTile(BC_SP);
}

static void yesno_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void yesno_draw(void) {
    yesno_set(YESNO_COL,             YESNO_ROW,     (uint8_t)Font_CharToTile(BC_TL));
    for (int c = 1; c < YESNO_W - 1; c++)
        yesno_set(YESNO_COL + c,     YESNO_ROW,     (uint8_t)Font_CharToTile(BC_H));
    yesno_set(YESNO_COL + YESNO_W-1, YESNO_ROW,     (uint8_t)Font_CharToTile(BC_TR));

    yesno_set(YESNO_COL,             YESNO_ROW + 1, (uint8_t)Font_CharToTile(BC_V));
    yesno_set(YESNO_COL + 1,         YESNO_ROW + 1,
              g_yesno_cursor == 0 ? (uint8_t)Font_CharToTile(BC_CUR)
                                  : (uint8_t)Font_CharToTile(BC_SP));
    yesno_set(YESNO_COL + 2,         YESNO_ROW + 1, yesno_char('Y'));
    yesno_set(YESNO_COL + 3,         YESNO_ROW + 1, yesno_char('E'));
    yesno_set(YESNO_COL + 4,         YESNO_ROW + 1, yesno_char('S'));
    yesno_set(YESNO_COL + YESNO_W-1, YESNO_ROW + 1, (uint8_t)Font_CharToTile(BC_V));

    yesno_set(YESNO_COL,             YESNO_ROW + 2, (uint8_t)Font_CharToTile(BC_V));
    yesno_set(YESNO_COL + 1,         YESNO_ROW + 2,
              g_yesno_cursor == 1 ? (uint8_t)Font_CharToTile(BC_CUR)
                                  : (uint8_t)Font_CharToTile(BC_SP));
    yesno_set(YESNO_COL + 2,         YESNO_ROW + 2, yesno_char('N'));
    yesno_set(YESNO_COL + 3,         YESNO_ROW + 2, yesno_char('O'));
    yesno_set(YESNO_COL + 4,         YESNO_ROW + 2, (uint8_t)Font_CharToTile(BC_SP));
    yesno_set(YESNO_COL + YESNO_W-1, YESNO_ROW + 2, (uint8_t)Font_CharToTile(BC_V));

    yesno_set(YESNO_COL,             YESNO_ROW + 3, (uint8_t)Font_CharToTile(BC_BL));
    for (int c = 1; c < YESNO_W - 1; c++)
        yesno_set(YESNO_COL + c,     YESNO_ROW + 3, (uint8_t)Font_CharToTile(BC_H));
    yesno_set(YESNO_COL + YESNO_W-1, YESNO_ROW + 3, (uint8_t)Font_CharToTile(BC_BR));
}

static void yesno_clear(void) {
    for (int r = 0; r < YESNO_H; r++)
        for (int c = 0; c < YESNO_W; c++)
            yesno_set(YESNO_COL + c, YESNO_ROW + r, (uint8_t)Font_CharToTile(BC_SP));
}

void BillsHouseScripts_OnMapLoad(void) {
    if (wCurMap != MAP_BILLS_HOUSE) return;

    int met_bill = CheckEvent(EVENT_MET_BILL);
    int said_use = CheckEvent(EVENT_BILL_SAID_USE_CELL_SEPARATOR);
    int used_sep = CheckEvent(EVENT_USED_CELL_SEPARATOR_ON_BILL);
    int left     = CheckEvent(EVENT_LEFT_BILLS_HOUSE_AFTER_HELPING);

    if (!met_bill) {
        if (said_use && !used_sep) {

            NPC_HideSprite(BILL_POKEMON_NPC);
            NPC_HideSprite(BILL1_NPC);
            NPC_HideSprite(BILL2_NPC);
            g_state = BH_WAIT_PC;
        } else {
            NPC_ShowSprite(BILL_POKEMON_NPC);
            NPC_HideSprite(BILL1_NPC);
            NPC_HideSprite(BILL2_NPC);
            g_state = BH_DEFAULT;
        }
    } else {

        NPC_HideSprite(BILL_POKEMON_NPC);
        if (left) {
            NPC_HideSprite(BILL1_NPC);
            NPC_ShowSprite(BILL2_NPC);
        } else {
            NPC_ShowSprite(BILL1_NPC);
            NPC_HideSprite(BILL2_NPC);
        }
        g_state = BH_DEFAULT;
    }

    printf("[bills_house] OnMapLoad: state=%d met=%d said=%d used=%d left=%d\n",
           g_state, met_bill, said_use, used_sep, left);
}

int BillsHouseScripts_IsActive(void) {
    return g_state != BH_DEFAULT && g_state != BH_WAIT_PC;
}

void BillsHouseScripts_PostRender(void) {
    if (g_state == BH_POKEMON_YESNO) yesno_draw();
}

void BillsHouseScripts_Tick(void) {
    if (wCurMap != MAP_BILLS_HOUSE) return;

    switch (g_state) {

    case BH_DEFAULT:
    case BH_WAIT_PC:
        return;

    case BH_POKEMON_TEXT1:
        if (Text_IsOpen()) { Text_Update(); return; }
        yesno_draw();
        g_state = BH_POKEMON_YESNO;
        return;

    case BH_POKEMON_YESNO:
        yesno_draw();
        if (hJoyPressed & PAD_UP) {
            if (g_yesno_cursor > 0) g_yesno_cursor--;
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_yesno_cursor < 1) g_yesno_cursor++;
        }
        if (hJoyPressed & (PAD_A | PAD_B)) {
            int chose_yes = (hJoyPressed & PAD_A) && g_yesno_cursor == 0;
            yesno_clear();
            Text_Close();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            if (chose_yes) {
                Text_ShowASCII(kText_UseSeparation);
                g_state = BH_POKEMON_TEXT2;
            } else {
                Text_ShowASCII(kText_NoYouGottaHelp);
                g_state = BH_POKEMON_NO_TEXT;
            }
        }
        return;

    case BH_POKEMON_NO_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        Text_ShowASCII(kText_UseSeparation);
        g_state = BH_POKEMON_TEXT2;
        return;

    case BH_POKEMON_TEXT2:
        if (Text_IsOpen()) { Text_Update(); return; }

        if (gPlayerFacing == DIR_DOWN) {

            g_walk_seq[0] = DIR_RIGHT;
            g_walk_seq[1] = DIR_UP;
            g_walk_seq[2] = DIR_UP;
            g_walk_seq[3] = DIR_LEFT;
            g_walk_seq[4] = DIR_UP;
            g_walk_seq[5] = -1;
        } else {

            g_walk_seq[0] = DIR_UP;
            g_walk_seq[1] = DIR_UP;
            g_walk_seq[2] = DIR_UP;
            g_walk_seq[3] = -1;
        }
        g_walk_pos = 0;

        NPC_DoScriptedStep(BILL_POKEMON_NPC, g_walk_seq[g_walk_pos++]);
        g_state = BH_POKEMON_WALK;
        return;

    case BH_POKEMON_WALK:
        if (NPC_IsWalking(BILL_POKEMON_NPC)) return;
        if (g_walk_seq[g_walk_pos] != -1) {
            NPC_DoScriptedStep(BILL_POKEMON_NPC, g_walk_seq[g_walk_pos++]);
            return;
        }
        g_state = BH_POKEMON_ENTERED;
        return;

    case BH_POKEMON_ENTERED:
        NPC_HideSprite(BILL_POKEMON_NPC);
        SetEvent(EVENT_BILL_SAID_USE_CELL_SEPARATOR);
        g_state = BH_WAIT_PC;
        printf("[bills_house] Monster entered machine; waiting for PC\n");
        return;

    case BH_PC_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }

        Music_Stop();
        g_frame = 0;
        g_state = BH_PC_DELAY_PRE;
        return;

    case BH_PC_DELAY_PRE:

        if (++g_frame >= 76) {
            Audio_PlaySFX_HealingMachine();            g_frame = 0;
            g_state = BH_PC_SFX1;
        }
        return;

    case BH_PC_SFX1:
        if (Audio_IsSFXPlaying()) return;
        g_frame = 0;
        g_state = BH_PC_DELAY2;
        return;

    case BH_PC_DELAY2:
        if (++g_frame >= 80) {
            Audio_PlaySFX_BallPoof();            g_frame = 0;
            g_state = BH_PC_SFX2;
        }
        return;

    case BH_PC_SFX2:
        if (Audio_IsSFXPlaying()) return;
        g_frame = 0;
        g_state = BH_PC_DELAY3;
        return;

    case BH_PC_DELAY3:
        if (++g_frame >= 48) {
            Audio_PlaySFX_HealingMachine();            g_frame = 0;
            g_state = BH_PC_SFX3;
        }
        return;

    case BH_PC_SFX3:
        if (Audio_IsSFXPlaying()) return;
        g_frame = 0;
        g_state = BH_PC_DELAY4;
        return;

    case BH_PC_DELAY4:
        if (++g_frame >= 32) {
            Audio_PlaySFX_GetKeyItem();            g_frame = 0;
            g_state = BH_PC_SFX4;
        }
        return;

    case BH_PC_SFX4:
        if (Audio_IsSFXPlaying_GetKeyItem()) return;

        Music_Play(Music_GetMapID(MAP_BILLS_HOUSE));
        SetEvent(EVENT_USED_CELL_SEPARATOR_ON_BILL);
        g_frame = 0;
        g_state = BH_BILL_APPEAR;
        printf("[bills_house] cell separator complete\n");
        return;

    case BH_PC_LIST:
        BillsPokemonList_Tick();
        if (!BillsPokemonList_IsOpen()) g_state = BH_DEFAULT;
        return;

    case BH_BILL_APPEAR:
        g_frame++;
        if (g_frame == 1) {

            NPC_ShowSprite(BILL1_NPC);
            NPC_SetTilePos(BILL1_NPC, MACHINE_X, MACHINE_Y);
        }
        if (g_frame >= 8) {

            g_exit_pos = 0;
            NPC_DoScriptedStep(BILL1_NPC, kBillExitSeq[g_exit_pos++]);
            g_state = BH_BILL_WALK;
        }
        return;

    case BH_BILL_WALK:
        if (NPC_IsWalking(BILL1_NPC)) return;
        if (kBillExitSeq[g_exit_pos] != -1) {
            NPC_DoScriptedStep(BILL1_NPC, kBillExitSeq[g_exit_pos++]);
            return;
        }
        g_state = BH_BILL_CLEANUP;
        return;

    case BH_BILL_CLEANUP:
        SetEvent(EVENT_MET_BILL_2);
        SetEvent(EVENT_MET_BILL);
        g_state = BH_DEFAULT;
        printf("[bills_house] transformation complete — Bill is human again\n");
        return;

    case BH_BILL1_TEXT1:
        if (Text_IsOpen()) { Text_Update(); return; }

        if (Inventory_Add(ITEM_SS_TICKET, 1) == 0) {
            SetEvent(EVENT_GOT_SS_TICKET);
            Text_SetItemName(ITEM_SS_TICKET);
            Text_ShowASCII(kText_SSTicketReceived);
            Audio_PlaySFX_GetKeyItem();
            g_state = BH_BILL1_JINGLE;
        } else {

            Text_ShowASCII(kText_SSTicketNoRoom);
            g_state = BH_BILL1_TEXT_WHY;
        }
        return;

    case BH_BILL1_JINGLE:
        if (Audio_IsSFXPlaying_GetKeyItem()) return;
        if (Text_IsOpen()) { Text_Update(); return; }
        Text_ShowASCII(kText_WhyDontYouGo);
        g_state = BH_BILL1_TEXT2;
        return;

    case BH_BILL1_TEXT2:
    case BH_BILL1_TEXT_WHY:
        if (Text_IsOpen()) { Text_Update(); return; }
        g_state = BH_DEFAULT;
        return;

    case BH_BILL2_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        g_state = BH_DEFAULT;
        return;

    default:
        g_state = BH_DEFAULT;
        return;
    }
}
