
#include "daycare.h"

#include "yesno.h"
#include "money_box.h"
#include "party_menu.h"
#include "pokemon.h"
#include "rom_text.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/font_data.h"

#include <stdio.h>
#include <string.h>

#define MOVE_FLASH_ID 0x94
static const uint8_t kHMMoves[5] = { MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH_ID };

typedef enum {
    DC_IDLE = 0,
    DC_INTRO_YESNO,
    DC_DECLINE_TEXT,
    DC_ONLY_ONE_MON_TEXT,
    DC_WHICH_MON_TEXT,
    DC_PARTY_PICK,
    DC_HM_REJECT_TEXT,
    DC_ALLRIGHT_TEXT,
    DC_WILL_LOOK_TEXT,
    DC_COME_SEE_TEXT,

    DC_GROWTH_TEXT,
    DC_NO_ROOM_TEXT,
    DC_OWE_MONEY_YESNO,
    DC_NOT_ENOUGH_MONEY_TEXT,
    DC_HERES_MON_TEXT,
    DC_GOT_MON_BACK_TEXT,
} daycare_state_t;

static daycare_state_t s_state = DC_IDLE;
static int      s_party_slot   = -1;
static uint32_t s_total_cost   = 0;

static uint8_t  s_start_level  = 0;
static char     s_name_buf[NAME_LENGTH + 1];
static char     s_text_buf[256];

static void decode_name(const uint8_t *src, char *dst, int dst_size) {
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

static uint32_t money_get(void) {
    return (uint32_t)(
        ((wPlayerMoney[0] >> 4) & 0xF) * 100000u +
        (wPlayerMoney[0] & 0xF)        * 10000u  +
        ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   +
        (wPlayerMoney[1] & 0xF)        * 100u     +
        ((wPlayerMoney[2] >> 4) & 0xF) * 10u      +
        (wPlayerMoney[2] & 0xF)
    );
}
static void money_set(uint32_t v) {
    if (v > 999999u) v = 999999u;
    wPlayerMoney[0] = (uint8_t)(((v / 100000u) << 4) | ((v / 10000u) % 10u));
    wPlayerMoney[1] = (uint8_t)((((v / 1000u) % 10u) << 4) | ((v / 100u) % 10u));
    wPlayerMoney[2] = (uint8_t)((((v / 10u) % 10u) << 4) | (v % 10u));
}

static int knows_hm_move(const party_mon_t *mon) {
    for (int m = 0; m < 4; m++) {
        uint8_t mv = mon->base.moves[m];
        if (mv == 0) continue;
        for (int h = 0; h < 5; h++)
            if (mv == kHMMoves[h]) return 1;
    }
    return 0;
}

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

#define kIntroText       (RomText("DaycareGentlemanText.IntroText"))
#define kWhichMonText    (RomText("DaycareGentlemanText.WhichMonText"))
#define kOnlyOneMonText  (RomText("DaycareGentlemanText.OnlyHaveOneMonText"))
#define kHMRejectText    (RomText("DaycareGentlemanText.CantAcceptMonWithHMText"))
#define kNoRoomText      (RomText("DaycareGentlemanText.NoRoomForMonText"))

#define kDeclineText        (RomText("_DaycareGentlemanComeAgainText"))
#define kAllRightThenText   (RomText("DaycareGentlemanText.AllRightThenText"))
#define kComeSeeMeText      (RomText("_DaycareGentlemanComeSeeMeInAWhileText"))
static const char kNotEnoughMoneyText[] =
    "Hey, you don't\nhave enough \xa5!";

void Daycare_StepCheck(void) {

    uint8_t *exp = wDayCareMon.exp;
    if (!wDayCareInUse) return;
    if (++exp[2] != 0) return;
    if (++exp[1] != 0) return;
    if (++exp[0] >= 0x50) exp[0] = 0x50;
}

void Daycare_Interact(void) {
    if (s_state != DC_IDLE) return;

    if (!wDayCareInUse) {
        YesNo_Show(kIntroText);
        s_state = DC_INTRO_YESNO;
        return;
    }

    {
        uint8_t new_level = Pokemon_DaycareCheckedLevel(wDayCareMon.species, wDayCareMon.exp);
        uint8_t levels_grown = 0;
        s_start_level = wDayCareMon.box_level;
        wDayCareMon.box_level = new_level;

        decode_name(wDayCareMonName, s_name_buf, sizeof(s_name_buf));
        if (s_name_buf[0] == '\0')
            snprintf(s_name_buf, sizeof(s_name_buf), "%s",
                     Pokemon_GetNameBySpecies(wDayCareMon.species));

        if (new_level == s_start_level) {
            snprintf(s_text_buf, sizeof(s_text_buf),
                     "Back already?\nYour %s\nneeds some more\ntime with me.",
                     s_name_buf);
        } else {
            levels_grown = (uint8_t)(new_level - s_start_level);
            snprintf(s_text_buf, sizeof(s_text_buf),
                     "Your %s\nhas grown a lot!\fBy level, it's\ngrown by %u!\fAren't I great?",
                     s_name_buf, (unsigned)levels_grown);
        }

        s_total_cost = 100u * (uint32_t)(levels_grown + 1);
        Text_ShowASCII(s_text_buf);
        s_state = DC_GROWTH_TEXT;
    }
}

int Daycare_IsActive(void) { return s_state != DC_IDLE; }

void Daycare_PostRender(void) {
    if (s_state == DC_INTRO_YESNO || s_state == DC_OWE_MONEY_YESNO)
        YesNo_PostRender();
    MoneyBox_Refresh();
}

void Daycare_Tick(void) {
    switch (s_state) {
    case DC_IDLE:
        break;

    case DC_INTRO_YESNO:
        YesNo_Tick();
        if (!YesNo_IsOpen()) {
            if (!YesNo_GetResult()) {
                Text_ShowASCII(kDeclineText);
                s_state = DC_DECLINE_TEXT;
                break;
            }
            if (wPartyCount <= 1) {
                Text_ShowASCII(kOnlyOneMonText);
                s_state = DC_ONLY_ONE_MON_TEXT;
                break;
            }
            Text_ShowASCII(kWhichMonText);
            s_state = DC_WHICH_MON_TEXT;
        }
        break;

    case DC_WHICH_MON_TEXT:
        if (Text_IsOpen()) break;
        PartyMenu_Open(PARTY_MENU_TRADE);
        s_state = DC_PARTY_PICK;
        break;

    case DC_PARTY_PICK:
        PartyMenu_Tick();
        if (!PartyMenu_IsOpen()) {
            restore_overworld_after_party_menu();
            s_party_slot = PartyMenu_GetSelected();
            if (s_party_slot < 0) {
                Text_ShowASCII(kAllRightThenText);
                s_state = DC_ALLRIGHT_TEXT;
                break;
            }
            if (knows_hm_move(&wPartyMons[s_party_slot])) {
                Text_ShowASCII(kHMRejectText);
                s_state = DC_HM_REJECT_TEXT;
                break;
            }
            decode_name(wPartyMonNicks[s_party_slot], s_name_buf, sizeof(s_name_buf));
            if (s_name_buf[0] == '\0')
                snprintf(s_name_buf, sizeof(s_name_buf), "%s",
                         Pokemon_GetNameBySpecies(wPartyMons[s_party_slot].base.species));
            snprintf(s_text_buf, sizeof(s_text_buf),
                     "Fine, I'll look\nafter %s\nfor a while.", s_name_buf);
            Text_ShowASCII(s_text_buf);
            s_state = DC_WILL_LOOK_TEXT;
        }
        break;

    case DC_WILL_LOOK_TEXT:
        if (Text_IsOpen()) break;
        {
            uint8_t species = wPartyMons[s_party_slot].base.species;
            wDayCareInUse = 1;
            Pokemon_DepositPartyMonToDaycare(s_party_slot);
            Audio_PlayCry(species);
        }
        Text_ShowASCII(kComeSeeMeText);
        s_state = DC_COME_SEE_TEXT;
        break;

    case DC_DECLINE_TEXT:
    case DC_ONLY_ONE_MON_TEXT:
    case DC_HM_REJECT_TEXT:
    case DC_COME_SEE_TEXT:
        if (Text_IsOpen()) break;
        s_state = DC_IDLE;
        break;

    case DC_ALLRIGHT_TEXT:

        if (Text_IsOpen()) break;
        MoneyBox_Clear();
        s_state = DC_IDLE;
        break;

    case DC_GROWTH_TEXT:
        if (Text_IsOpen()) break;
        if (wPartyCount >= PARTY_LENGTH) {

            wDayCareMon.box_level = s_start_level;
            Text_ShowASCII(kNoRoomText);
            s_state = DC_NO_ROOM_TEXT;
            break;
        }
        snprintf(s_text_buf, sizeof(s_text_buf),
                 "You owe me \xa5%u\nfor the return\nof this MONSTER.",
                 (unsigned)s_total_cost);
        YesNo_ArmMoneyBox();
        YesNo_Show(s_text_buf);
        s_state = DC_OWE_MONEY_YESNO;
        break;

    case DC_OWE_MONEY_YESNO:
        YesNo_Tick();
        if (!YesNo_IsOpen()) {
            if (!YesNo_GetResult()) {
                wDayCareMon.box_level = s_start_level;

                Text_ShowASCII(kAllRightThenText);
                s_state = DC_ALLRIGHT_TEXT;
                break;
            }
            if (s_total_cost > money_get()) {
                wDayCareMon.box_level = s_start_level;
                Text_ShowASCII(kNotEnoughMoneyText);
                s_state = DC_NOT_ENOUGH_MONEY_TEXT;
                break;
            }
            money_set(money_get() - s_total_cost);
            Audio_PlaySFX_Purchase();

            Text_ShowASCII(RomText("DaycareGentlemanText.HeresYourMonText"));
            s_state = DC_HERES_MON_TEXT;
        }
        break;

    case DC_NOT_ENOUGH_MONEY_TEXT:
        if (Text_IsOpen()) break;
        MoneyBox_Clear();
        s_state = DC_IDLE;
        break;

    case DC_NO_ROOM_TEXT:
        if (Text_IsOpen()) break;
        s_state = DC_IDLE;
        break;

    case DC_HERES_MON_TEXT:
        if (Text_IsOpen()) break;
        MoneyBox_Clear();
        {
            uint8_t species = wDayCareMon.species;
            char nick_buf[NAME_LENGTH + 1];
            decode_name(wDayCareMonName, nick_buf, sizeof(nick_buf));
            if (nick_buf[0] == '\0')
                snprintf(nick_buf, sizeof(nick_buf), "%s", Pokemon_GetNameBySpecies(species));
            Pokemon_WithdrawDaycareMonToParty();
            Audio_PlayCry(species);
            snprintf(s_text_buf, sizeof(s_text_buf), "{PLAYER} got\n%s back!", nick_buf);
        }
        Text_ShowASCII(s_text_buf);
        s_state = DC_GOT_MON_BACK_TEXT;
        break;

    case DC_GOT_MON_BACK_TEXT:
        if (Text_IsOpen()) break;
        s_state = DC_IDLE;
        break;
    }
}
