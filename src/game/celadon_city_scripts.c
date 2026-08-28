#include "celadon_city_scripts.h"
#include "rom_text.h"
#include "constants.h"
#include "inventory.h"
#include "npc.h"
#include "pokemon.h"
#include "pokedex.h"
#include "text.h"
#include "yesno.h"
#include "naming_screen.h"
#include "elevator_menu.h"
#include "overworld.h"
#include "../data/font_data.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdio.h>

#define ITEM_TM01  (ITEM_HM01 + 5)
#define ITEM_TM41  (ITEM_TM01 + 40)
#define ITEM_TM18  (ITEM_TM01 + 17)
#define ITEM_COIN_CASE 0x45
#define ITEM_HM02  (ITEM_HM01 + 1)
#define ITEM_PP_UP 0x4F
#define ITEM_FRESH_WATER 0x3C
#define ITEM_SODA_POP 0x3D
#define ITEM_LEMONADE 0x3E
#define ITEM_TM13 (ITEM_TM01 + 12)
#define ITEM_TM48 (ITEM_TM01 + 47)
#define ITEM_TM49 (ITEM_TM01 + 48)

#define SPECIES_POLIWRATH 0x14
#define MAP_CELADON_MANSION_ROOF_HOUSE 0x84
#define MAP_CELADON_MART_ROOF 0x7E
#define MAP_ROUTE16_FLY_HOUSE 0xBC
#define NPC_CELADON_MANSION_EEVEE_BALL 1
#define POKE_SPACE  0x7F
#define POKE_TL     0x79
#define POKE_H      0x7A
#define POKE_TR     0x7B
#define POKE_V      0x7C
#define POKE_BL     0x7D
#define POKE_BR     0x7E
#define POKE_CURSOR 0xED

#define TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())

typedef enum {
    CG_IDLE = 0,
    CG_COINCASE_WAIT_PRETEXT_CLOSE,
    CG_COINCASE_WAIT_JINGLE,
    CG_EEVEE_WAIT_RECEIVED_CLOSE,
    CG_EEVEE_WAIT_NICK_YESNO,
    CG_EEVEE_WAIT_NAMING,
    CG_TM18_WAIT_PRETEXT_CLOSE,
    CG_ROOF_WAIT_DRINK_YN,
    CG_ROOF_WAIT_SELECT_DRINK,
    CG_ROOF_WAIT_GIRL_REWARD,
} CeladonGiftState;

static CeladonGiftState sGiftState = CG_IDLE;
static int sEeveePartySlot = -1;
static int sCursor = 0;
static int sDrinkCount = 0;
static uint8_t sDrinkList[3];
static uint8_t sPendingDrinkItem = 0;
static uint8_t sMenuSaved[6 * 14];
static int sMenuSavedValid = 0;

#define kTM41Intro (RomText("CeladonCityGramps3Text.Text"))

#define kTM41Explain (RomText("CeladonCityGramps3Text.TM41ExplanationText"))

#define kTM41NoRoom (RomText("CeladonCityGramps3Text.TM41NoRoomText"))
#define kTM18PreReceive (RomText("CeladonMart3FClerkText.TM18PreReceiveText"))

#define kTM18Received (RomText("CeladonMart3FClerkText.ReceivedTM18Text"))
#define kTM18Explain (RomText("CeladonMart3FClerkText.TM18ExplanationText"))
#define kTM18NoRoom (RomText("CeladonMart3FClerkText.TM18NoRoomText"))

#define kCoinCaseGifted (RomText("CeladonDinerGymGuideText.ReceivedCoinCaseText"))

#define kCoinCasePreGift (RomText("CeladonDinerGymGuideText.ImFlatOutBustedText"))

#define kCoinCaseNoRoom (RomText("CeladonDinerGymGuideText.CoinCaseNoRoomText"))

#define kCoinCaseAfter (RomText("CeladonDinerGymGuideText.WinItBackText"))

static char kEeveeReceivedBuf[48];

static char kEeveeNickPromptBuf[64];

#define kEeveePartyFull (PortText("You can't carry\nany more #MON!"))

#define kHM02Intro (RomText("Route16FlyHouseBrunetteGirlText.Text"))

#define kHM02Explain (RomText("Route16FlyHouseBrunetteGirlText.HM02ExplanationText"))

#define kHM02NoRoom (RomText("Route16FlyHouseBrunetteGirlText.HM02NoRoomText"))

static char kFoundPPUpBuf[48];
#define kNoRoomForPPUp (RomText("_HiddenItemBagFullText"))
#define kGirlGiveDrinkPrompt (RomText("CeladonMartRoofLittleGirlText.GiveHerADrinkText"))
#define kGirlGiveWhichDrink (RomText("CeladonMartRoofLittleGirlGiveHerWhichDrinkText"))
#define kGirlImThirsty (RomText("CeladonMartRoofLittleGirlText.ImThirstyText"))
#define kGirlNotThirsty (RomText("CeladonMartRoofLittleGirlImNotThirstyText"))
#define kGirlNoRoom (RomText("CeladonMartRoofLittleGirlNoRoomText"))
#define kGirlYayFresh (RomText("CeladonMartRoofLittleGirlYayFreshWaterText"))
#define kGirlYaySoda (RomText("CeladonMartRoofLittleGirlYaySodaPopText"))
#define kGirlYayLemon (RomText("CeladonMartRoofLittleGirlYayLemonadeText"))

static char kGirlGotTM13Buf[128];
static char kGirlGotTM48Buf[128];
static char kGirlGotTM49Buf[128];
#define kVendingIntro (RomText("VendingMachineText1"))

static uint8_t poke_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == '-') return (uint8_t)Font_CharToTile(0xE3);
    if (c == ' ') return (uint8_t)Font_CharToTile(POKE_SPACE);
    return (uint8_t)Font_CharToTile(POKE_SPACE);
}

static void menu_set(int row, int col, uint8_t tile) {
    gScrollTileMap[TMIDX(row, col)] = tile;
}

static void menu_draw_box(int row, int col, int w, int h) {
    menu_set(row, col, (uint8_t)Font_CharToTile(POKE_TL));
    for (int c = 1; c < w - 1; c++) menu_set(row, col + c, (uint8_t)Font_CharToTile(POKE_H));
    menu_set(row, col + w - 1, (uint8_t)Font_CharToTile(POKE_TR));
    for (int r = 1; r < h - 1; r++) {
        menu_set(row + r, col, (uint8_t)Font_CharToTile(POKE_V));
        for (int c = 1; c < w - 1; c++) menu_set(row + r, col + c, (uint8_t)Font_CharToTile(POKE_SPACE));
        menu_set(row + r, col + w - 1, (uint8_t)Font_CharToTile(POKE_V));
    }
    menu_set(row + h - 1, col, (uint8_t)Font_CharToTile(POKE_BL));
    for (int c = 1; c < w - 1; c++) menu_set(row + h - 1, col + c, (uint8_t)Font_CharToTile(POKE_H));
    menu_set(row + h - 1, col + w - 1, (uint8_t)Font_CharToTile(POKE_BR));
}

static void menu_put(int row, int col, const char *s) {
    for (int i = 0; s[i]; i++) menu_set(row, col + i, poke_char((unsigned char)s[i]));
}

static void build_roof_drink_list(void) {
    sDrinkCount = 0;
    if (Inventory_GetQty(ITEM_FRESH_WATER) > 0) sDrinkList[sDrinkCount++] = ITEM_FRESH_WATER;
    if (Inventory_GetQty(ITEM_SODA_POP) > 0) sDrinkList[sDrinkCount++] = ITEM_SODA_POP;
    if (Inventory_GetQty(ITEM_LEMONADE) > 0) sDrinkList[sDrinkCount++] = ITEM_LEMONADE;
}

static void menu_save(void) {
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 14; c++)
            sMenuSaved[r * 14 + c] = gScrollTileMap[TMIDX(r + 2, c)];
    sMenuSavedValid = 1;
}

static void menu_restore(void) {
    if (!sMenuSavedValid) return;
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 14; c++)
            gScrollTileMap[TMIDX(r + 2, c)] = sMenuSaved[r * 14 + c];
    sMenuSavedValid = 0;
}

static void draw_drink_list_menu(void) {
    if (!sMenuSavedValid) menu_save();
    menu_draw_box(2, 0, 14, 2 + sDrinkCount + 1);
    char name[16];
    for (int i = 0; i < sDrinkCount; i++) {
        Inventory_DecodeASCII(sDrinkList[i], name, sizeof(name));
        menu_set(3 + i, 1, (i == sCursor) ? (uint8_t)Font_CharToTile(POKE_CURSOR) : (uint8_t)Font_CharToTile(POKE_SPACE));
        menu_put(3 + i, 2, name);
    }
}

void CeladonMartRoof_VendingMachineScript(void) {
    if (sGiftState != CG_IDLE) return;
    wDoNotWaitForButtonPress = 1;
    Text_ShowASCII(kVendingIntro);
    ElevatorMenu_QueueOpenVending();
}

void CeladonGiftScripts_OnMapLoad(void) {
    if (wCurMap == MAP_CELADON_MANSION_ROOF_HOUSE && CheckEvent(EVENT_GOT_EEVEE))
        NPC_HideSprite(NPC_CELADON_MANSION_EEVEE_BALL);
}

void CeladonGiftScripts_Tick(void) {
    switch (sGiftState) {
    case CG_IDLE:
        return;
    case CG_COINCASE_WAIT_PRETEXT_CLOSE:
        if (Text_IsOpen()) return;
        Audio_PlaySFX_GetKeyItem();
        Text_SetItemName(ITEM_COIN_CASE);
        Text_ShowASCII(kCoinCaseGifted);
        sGiftState = CG_COINCASE_WAIT_JINGLE;
        return;
    case CG_COINCASE_WAIT_JINGLE:
        if (Text_IsOpen()) return;
        if (Audio_IsSFXPlaying_GetKeyItem()) return;
        sGiftState = CG_IDLE;
        return;
    case CG_EEVEE_WAIT_RECEIVED_CLOSE:
        if (Text_IsOpen()) return;
        RomTextSplice(kEeveeNickPromptBuf, sizeof(kEeveeNickPromptBuf),
                     "DoYouWantToNicknameText", "{badge}", "EEVEE");
        YesNo_Show(kEeveeNickPromptBuf);
        sGiftState = CG_EEVEE_WAIT_NICK_YESNO;
        return;
    case CG_EEVEE_WAIT_NICK_YESNO:
        YesNo_Tick();
        if (YesNo_IsOpen()) return;
        if (YesNo_GetResult() && sEeveePartySlot >= 0 && sEeveePartySlot < PARTY_LENGTH) {
            NamingScreen_Open(NAME_MON_SCREEN, SPECIES_EEVEE, wPartyMonNicks[sEeveePartySlot]);
            sGiftState = CG_EEVEE_WAIT_NAMING;
            return;
        }
        sGiftState = CG_IDLE;
        return;
    case CG_EEVEE_WAIT_NAMING:
        if (NamingScreen_IsOpen()) return;
        sGiftState = CG_IDLE;
        return;
    case CG_TM18_WAIT_PRETEXT_CLOSE:
        if (Text_IsOpen()) return;
        if (Inventory_Add(ITEM_TM18, 1) != 0) {
            Text_ShowASCII(kTM18NoRoom);
            sGiftState = CG_IDLE;
            return;
        }
        SetEvent(EVENT_GOT_TM18);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(ITEM_TM18);
        Text_ShowASCII(kTM18Received);
        sGiftState = CG_IDLE;
        return;
    case CG_ROOF_WAIT_DRINK_YN:
        YesNo_Tick();
        if (YesNo_IsOpen()) return;
        if (!YesNo_GetResult()) {
            Text_ShowASCII(kGirlNotThirsty);
            sGiftState = CG_IDLE;
            return;
        }
        Text_ShowASCII(kGirlGiveWhichDrink);
        sCursor = 0;
        sMenuSavedValid = 0;
        sGiftState = CG_ROOF_WAIT_SELECT_DRINK;
        return;
    case CG_ROOF_WAIT_SELECT_DRINK:
        if (Text_IsOpen()) return;
        if (sDrinkCount <= 0) {
            sGiftState = CG_IDLE;
            return;
        }
        if (hJoyPressed & PAD_UP) {
            if (sCursor > 0) sCursor--;
        }
        if (hJoyPressed & PAD_DOWN) {
            if (sCursor < (sDrinkCount - 1)) sCursor++;
        }
        if (hJoyPressed & PAD_B) {
            menu_restore();
            Text_ShowASCII(kGirlNotThirsty);
            sGiftState = CG_IDLE;
            return;
        }
        if (hJoyPressed & PAD_A) {
            uint8_t item = sDrinkList[sCursor];
            menu_restore();
            sPendingDrinkItem = item;
            if (item == ITEM_FRESH_WATER && CheckEvent(EVENT_GOT_TM13)) {
                Text_ShowASCII(kGirlNotThirsty);
                sGiftState = CG_IDLE;
                return;
            }
            if (item == ITEM_SODA_POP && CheckEvent(EVENT_GOT_TM48)) {
                Text_ShowASCII(kGirlNotThirsty);
                sGiftState = CG_IDLE;
                return;
            }
            if (item == ITEM_LEMONADE && CheckEvent(EVENT_GOT_TM49)) {
                Text_ShowASCII(kGirlNotThirsty);
                sGiftState = CG_IDLE;
                return;
            }
            if (item == ITEM_FRESH_WATER) Text_ShowASCII(kGirlYayFresh);
            else if (item == ITEM_SODA_POP) Text_ShowASCII(kGirlYaySoda);
            else Text_ShowASCII(kGirlYayLemon);
            sGiftState = CG_ROOF_WAIT_GIRL_REWARD;
            return;
        }
        return;
    case CG_ROOF_WAIT_GIRL_REWARD:
        if (Text_IsOpen()) return;
        if (Inventory_Remove(sPendingDrinkItem, 1) != 0) {
            sGiftState = CG_IDLE;
            return;
        }
        if (sPendingDrinkItem == ITEM_FRESH_WATER) {
            if (Inventory_Add(ITEM_TM13, 1) != 0) {
                Text_ShowASCII(kGirlNoRoom);
                sGiftState = CG_IDLE;
                return;
            }
            SetEvent(EVENT_GOT_TM13);
            Audio_PlaySFX_GetItem1();
            Text_SetItemName(ITEM_TM13);
            snprintf(kGirlGotTM13Buf, sizeof(kGirlGotTM13Buf), "%s%s",
                     RomText("_CeladonMartRoofLittleGirlReceivedTM13Text"),
                     RomText("_CeladonMartRoofLittleGirlTM13ExplanationText"));
            Text_ShowASCII(kGirlGotTM13Buf);
        } else if (sPendingDrinkItem == ITEM_SODA_POP) {
            if (Inventory_Add(ITEM_TM48, 1) != 0) {
                Text_ShowASCII(kGirlNoRoom);
                sGiftState = CG_IDLE;
                return;
            }
            SetEvent(EVENT_GOT_TM48);
            Audio_PlaySFX_GetItem1();
            Text_SetItemName(ITEM_TM48);
            snprintf(kGirlGotTM48Buf, sizeof(kGirlGotTM48Buf), "%s%s",
                     RomText("_CeladonMartRoofLittleGirlReceivedTM48Text"),
                     RomText("_CeladonMartRoofLittleGirlTM48ExplanationText"));
            Text_ShowASCII(kGirlGotTM48Buf);
        } else {
            if (Inventory_Add(ITEM_TM49, 1) != 0) {
                Text_ShowASCII(kGirlNoRoom);
                sGiftState = CG_IDLE;
                return;
            }
            SetEvent(EVENT_GOT_TM49);
            Audio_PlaySFX_GetItem1();
            snprintf(kGirlGotTM49Buf, sizeof(kGirlGotTM49Buf), "%s%s",
                     RomText("_CeladonMartRoofLittleGirlReceivedTM49Text"),
                     RomText("_CeladonMartRoofLittleGirlTM49ExplanationText"));
            Text_ShowASCII(kGirlGotTM49Buf);
        }
        sGiftState = CG_IDLE;
        return;
    }
}

int CeladonGiftScripts_IsActive(void) {
    return sGiftState != CG_IDLE;
}

void CeladonGiftScripts_PostRender(void) {
    if (sGiftState == CG_EEVEE_WAIT_NICK_YESNO && YesNo_IsOpen())
        YesNo_PostRender();
    if (sGiftState == CG_ROOF_WAIT_SELECT_DRINK && !Text_IsOpen())
        draw_drink_list_menu();
}
