#include "name_rater_scripts.h"

#include "rom_text.h"
#include "yesno.h"
#include "party_menu.h"
#include "naming_screen.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "pokemon.h"
#include "text.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/base_stats.h"
#include "../data/font_data.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    NRS_IDLE = 0,
    NRS_WAIT_RATE_YESNO,
    NRS_WAIT_WHICH_TEXT,
    NRS_PARTY_PICK,
    NRS_WAIT_RENAME_YESNO,
    NRS_WAIT_WHAT_NAME_TEXT,
    NRS_WAIT_NAMING,
    NRS_WAIT_DONE_TEXT,
} name_rater_state_t;

static name_rater_state_t s_state = NRS_IDLE;
static int s_party_slot = -1;
static char s_name_buf[NAME_LENGTH + 1];
static char s_text_buf[256];

#define kPromptRate (RomText("NameRatersHouseNameRaterText.WantMeToRateText"))

#define kPromptWhich (RomText("NameRatersHouseNameRaterText.WhichPokemonText"))

#define kComeAnyTime (RomText("NameRatersHouseNameRaterText.ComeAnyTimeYouLikeText"))

#define kWhatShouldWeNameIt (RomText("NameRatersHouseNameRaterText.WhatShouldWeNameItText"))

static void restore_overworld_after_party_menu(void) {
    hWY = SCREEN_HEIGHT_PX;
    Display_LoadMapPalette();
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();

    Player_SyncOAM();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

static void decode_gb_name(const uint8_t *src, char *dst, int dst_size) {
    int out = 0;
    if (dst_size <= 0) return;
    for (int i = 0; i < NAME_LENGTH - 1 && out < dst_size - 1; i++) {
        uint8_t c = src[i];
        if (c == 0x00 || c == 0x50) break;
        if      (c >= 0x80 && c <= 0x99) dst[out++] = (char)('A' + (c - 0x80));
        else if (c >= 0xA0 && c <= 0xB9) dst[out++] = (char)('a' + (c - 0xA0));
        else if (c >= 0xF6)              dst[out++] = (char)('0' + (c - 0xF6));
        else if (c == 0x7F)              dst[out++] = ' ';
        else if (c == 0xE8)              dst[out++] = '.';
        else if (c == 0xE7)              dst[out++] = '!';
        else if (c == 0xE6)              dst[out++] = '?';
        else if (c == 0xE3)              dst[out++] = '-';
        else if (c == 0xE0)              dst[out++] = '\'';
    }
    dst[out] = '\0';
}

static void selected_mon_name(char *dst, int dst_size) {
    const uint8_t *nick = wPartyMonNicks[s_party_slot];
    decode_gb_name(nick, dst, dst_size);
    if (dst[0] == '\0') {
        uint8_t dex = gSpeciesToDex[wPartyMons[s_party_slot].base.species];
        snprintf(dst, dst_size, "%s", Pokemon_GetName(dex));
    }
}

static int is_players_mon(int slot) {
    if (slot < 0 || slot >= (int)wPartyCount) return 0;
    if (memcmp(wPartyMonOT[slot], wPlayerName, NAME_LENGTH) != 0) return 0;
    if (wPartyMons[slot].base.ot_id != wPlayerID) return 0;
    return 1;
}

void NameRater_Start(void) {
    if (s_state != NRS_IDLE) return;
    s_party_slot = -1;
    YesNo_Show(kPromptRate);
    s_state = NRS_WAIT_RATE_YESNO;
}

int NameRater_IsActive(void) {
    return s_state != NRS_IDLE;
}

void NameRater_PostRender(void) {
    if (s_state == NRS_WAIT_RATE_YESNO || s_state == NRS_WAIT_RENAME_YESNO)
        YesNo_PostRender();
}

void NameRater_Tick(void) {
    switch (s_state) {
    case NRS_IDLE:
        break;

    case NRS_WAIT_RATE_YESNO:
        YesNo_Tick();
        if (!YesNo_IsOpen()) {
            if (!YesNo_GetResult()) {
                Text_ShowASCII(kComeAnyTime);
                s_state = NRS_WAIT_DONE_TEXT;
            } else {
                Text_ShowASCII(kPromptWhich);
                s_state = NRS_WAIT_WHICH_TEXT;
            }
        }
        break;

    case NRS_WAIT_WHICH_TEXT:

        PartyMenu_Open(PARTY_MENU_TRADE);
        s_state = NRS_PARTY_PICK;
        break;

    case NRS_PARTY_PICK:
        PartyMenu_Tick();
        if (!PartyMenu_IsOpen()) {
            restore_overworld_after_party_menu();
            s_party_slot = PartyMenu_GetSelected();
            if (s_party_slot < 0) {
                Text_ShowASCII(kComeAnyTime);
                s_state = NRS_WAIT_DONE_TEXT;
                break;
            }
            selected_mon_name(s_name_buf, sizeof(s_name_buf));
            if (!is_players_mon(s_party_slot)) {
                RomTextSplice(s_text_buf, sizeof(s_text_buf),
                             "_NameRatersHouseNameRaterATrulyImpeccableNameText",
                             "{badge}", s_name_buf);
                Text_ShowASCII(s_text_buf);
                s_state = NRS_WAIT_DONE_TEXT;
                break;
            }
            RomTextSplice(s_text_buf, sizeof(s_text_buf),
                         "_NameRatersHouseNameRaterGiveItANiceNameText",
                         "{badge}", s_name_buf);
            YesNo_Show(s_text_buf);
            s_state = NRS_WAIT_RENAME_YESNO;
        }
        break;

    case NRS_WAIT_RENAME_YESNO:
        YesNo_Tick();
        if (!YesNo_IsOpen()) {
            if (!YesNo_GetResult()) {
                Text_ShowASCII(kComeAnyTime);
                s_state = NRS_WAIT_DONE_TEXT;
            } else {
                Text_ShowASCII(kWhatShouldWeNameIt);
                s_state = NRS_WAIT_WHAT_NAME_TEXT;
            }
        }
        break;

    case NRS_WAIT_WHAT_NAME_TEXT:
        if (s_party_slot >= 0 && s_party_slot < (int)wPartyCount) {
            NamingScreen_Open(NAME_MON_SCREEN,
                              wPartyMons[s_party_slot].base.species,
                              wPartyMonNicks[s_party_slot]);
            s_state = NRS_WAIT_NAMING;
        } else {
            Text_ShowASCII(kComeAnyTime);
            s_state = NRS_WAIT_DONE_TEXT;
        }
        break;

    case NRS_WAIT_NAMING:
        if (NamingScreen_IsOpen()) break;
        selected_mon_name(s_name_buf, sizeof(s_name_buf));
        RomTextSplice(s_text_buf, sizeof(s_text_buf),

                     "NameRatersHouseNameRaterText.PokemonHasBeenRenamedText",
                     "{ram:CEE9}", s_name_buf);
        Text_ShowASCII(s_text_buf);
        s_state = NRS_WAIT_DONE_TEXT;
        break;

    case NRS_WAIT_DONE_TEXT:
        s_state = NRS_IDLE;
        break;
    }
}
