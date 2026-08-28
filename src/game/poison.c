
#include "poison.h"
#include "constants.h"
#include "text.h"
#include "rom_text.h"
#include "pokecenter.h"
#include "map_music.h"
#include "amberscript_mapbank.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "gbc_color.h"
#include <stdio.h>
#include <string.h>

extern int gScriptedMovement;
extern void Map_Load(uint8_t map_id);
extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);
void Player_SetPos(int16_t x, int16_t y);

static void decode_poke_name(const uint8_t *src, char *dst, int dst_size) {
    int out = 0;
    if (dst_size <= 0) return;
    for (int i = 0; out < dst_size - 1 && src[i] != 0x50; i++) {
        uint8_t c = src[i];
        if      (c >= 0x80 && c <= 0x99) dst[out++] = (char)('A' + (c - 0x80));
        else if (c >= 0xA0 && c <= 0xB9) dst[out++] = (char)('a' + (c - 0xA0));
        else if (c == 0x7F)              dst[out++] = ' ';
        else if (c >= 0xF6)              dst[out++] = (char)('0' + (c - 0xF6));
        else if (c == 0xE8)              dst[out++] = '.';
        else if (c == 0xE3)              dst[out++] = '-';
        else if (c == 0xF3)              dst[out++] = '/';
        else                             dst[out++] = '?';
    }
    dst[out] = '\0';
}

typedef enum {
    PZ_NONE = 0,
    PZ_SHOWING_FAINT,
    PZ_POST_LOOP,
    PZ_FLASHING,
    PZ_SHOWING_BLACKOUT,
    PZ_BLACKOUT_FADE,
    PZ_BLACKOUT_SETTLE,
    PZ_BLACKOUT_FADE_IN,
} poison_phase_t;

static poison_phase_t s_phase = PZ_NONE;
static uint8_t s_fainted_slots[PARTY_LENGTH];
static int     s_fainted_count = 0;
static int     s_fainted_idx   = 0;
static int     s_flash_timer   = 0;
static int     s_any_alive_after_flash = 1;
static uint8_t s_flash_saved_bgp, s_flash_saved_obp0, s_flash_saved_obp1;
static int     s_bo_fade_step  = 0;
static int     s_bo_fade_timer = 0;
static int     s_bo_hold       = 0;
static uint8_t s_bo_target_bgp, s_bo_target_obp0, s_bo_target_obp1;

static const uint8_t kBlackoutFadeOut[4][3] = {
    { 0xE4, 0xD0, 0xE0 },
    { 0xF9, 0xE4, 0xE4 },
    { 0xFE, 0xFE, 0xF8 },
    { 0xFF, 0xFF, 0xFF },
};

static uint8_t darken_byte(uint8_t v) {
    uint8_t out = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t shade = (uint8_t)((v >> (i * 2)) & 3);
        if (shade < 3) shade++;
        out = (uint8_t)(out | (shade << (i * 2)));
    }
    return out;
}

void Poison_StepCheck(void) {

    if (gScriptedMovement) return;

    wStepCounter--;

    if (wPartyCount == 0) return;

    if ((wStepCounter & 3) != 0) return;

    s_fainted_count = 0;
    for (int i = 0; i < (int)wPartyCount && i < PARTY_LENGTH; i++) {
        if (!(wPartyMons[i].base.status & STATUS_PSN)) continue;
        if (wPartyMons[i].base.hp == 0) continue;
        wPartyMons[i].base.hp--;
        if (wPartyMons[i].base.hp == 0) {

            wPartyMons[i].base.status = 0;
            if (s_fainted_count < PARTY_LENGTH)
                s_fainted_slots[s_fainted_count++] = (uint8_t)i;
        }
    }
    s_fainted_idx = 0;
    s_phase = (s_fainted_count > 0) ? PZ_SHOWING_FAINT : PZ_POST_LOOP;
}

static void show_fainted_text(int slot) {
    char name[NAME_LENGTH + 8];

    if (wPartyMonNicks[slot][0] != 0x00 && wPartyMonNicks[slot][0] != 0x50)
        decode_poke_name(wPartyMonNicks[slot], name, sizeof(name));
    else
        snprintf(name, sizeof(name), "%s", PortText("#MON"));

    static char buf[64];

    snprintf(buf, sizeof(buf), "%s\nfainted!", name);
    Text_ShowASCII(buf);
}

static const struct { const char *name; int16_t x, y; } kBlackoutTownSpot[] = {
    { "PalletTown",      5,  6 },
    { "ViridianCity",   23, 26 },
    { "PewterCity",     13, 26 },
    { "CeruleanCity",   19, 18 },
    { "LavenderTown",    3,  6 },
    { "VermilionCity",  11,  4 },
    { "CeladonCity",    41, 10 },
    { "FuchsiaCity",    19, 28 },
    { "CinnabarIsland", 11, 12 },
    { "IndigoPlateau",   9,  6 },
    { "SaffronCity",     9, 30 },
    { "Route4",         11,  6 },
    { "Route10",        11, 20 },
};
#define BLACKOUT_TOWN_SPOT_COUNT (int)(sizeof(kBlackoutTownSpot) / sizeof(kBlackoutTownSpot[0]))

static int blackout_spot_for_town(const char *name, int16_t *x, int16_t *y) {
    for (int i = 0; i < BLACKOUT_TOWN_SPOT_COUNT; i++) {
        if (strcmp(kBlackoutTownSpot[i].name, name) == 0) {
            *x = kBlackoutTownSpot[i].x;
            *y = kBlackoutTownSpot[i].y;
            return 1;
        }
    }
    return 0;
}

static void poison_blackout_heal_warp_and_music(void) {
    Pokecenter_HealPartyFull();

    {
        unsigned v =
            ((wPlayerMoney[0] >> 4) & 0xF) * 100000u +
            (wPlayerMoney[0] & 0xF)        * 10000u  +
            ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   +
            (wPlayerMoney[1] & 0xF)        * 100u    +
            ((wPlayerMoney[2] >> 4) & 0xF) * 10u     +
            (wPlayerMoney[2] & 0xF);
        v /= 2;
        wPlayerMoney[0] = (uint8_t)(((v / 100000u) << 4) | ((v / 10000u) % 10u));
        wPlayerMoney[1] = (uint8_t)((((v / 1000u) % 10u) << 4) | ((v / 100u) % 10u));
        wPlayerMoney[2] = (uint8_t)((((v / 10u) % 10u) << 4) | (v % 10u));
    }

    if (wLastBlackoutMap == 0xFF) {

        AmberScript_MapWarp("PalletTown", 5, 6);
    } else if (wLastHealTownName[0] != '\0') {
        int16_t x, y;
        if (!blackout_spot_for_town(wLastHealTownName, &x, &y)) {

            x = 3; y = 3;
        }
        AmberScript_MapWarp(wLastHealTownName, x, y);
    } else {

        Game_WarpToRealMap(wLastBlackoutMap, 3, 7);
    }

    Player_SetPos(wXCoord, wYCoord);
    MapMusic_Play();
}

static void poison_finish_post_loop(void) {
    if (!s_any_alive_after_flash) {
        Text_ShowASCII(RomText("_PlayerBlackedOutText"));
        s_phase = PZ_SHOWING_BLACKOUT;
    } else {
        s_phase = PZ_NONE;
    }
}

void Poison_Tick(void) {
    switch (s_phase) {
    case PZ_NONE:
        return;

    case PZ_SHOWING_FAINT:
        if (Text_IsOpen()) return;
        if (s_fainted_idx < s_fainted_count) {
            show_fainted_text(s_fainted_slots[s_fainted_idx]);
            s_fainted_idx++;
            return;
        }
        s_phase = PZ_POST_LOOP;
        return;

    case PZ_POST_LOOP: {
        int any_poisoned = 0, any_alive = 0;
        for (int i = 0; i < (int)wPartyCount && i < PARTY_LENGTH; i++) {
            if (wPartyMons[i].base.status & STATUS_PSN) any_poisoned = 1;
            if (wPartyMons[i].base.hp > 0) any_alive = 1;
        }
        s_any_alive_after_flash = any_alive;
        if (any_poisoned) {

            Audio_PlaySFX_Poisoned();
            s_flash_saved_bgp  = Display_GetBGP();
            s_flash_saved_obp0 = Display_GetOBP0();
            s_flash_saved_obp1 = Display_GetOBP1();
            Display_SetPalette(darken_byte(s_flash_saved_bgp),
                                darken_byte(s_flash_saved_obp0),
                                darken_byte(s_flash_saved_obp1));
            s_flash_timer = 4;
            s_phase = PZ_FLASHING;
            return;
        }
        poison_finish_post_loop();
        return;
    }

    case PZ_FLASHING:
        if (--s_flash_timer > 0) return;
        Display_SetPalette(s_flash_saved_bgp, s_flash_saved_obp0, s_flash_saved_obp1);

        GbcColor_MarkDirty();
        poison_finish_post_loop();
        return;

    case PZ_SHOWING_BLACKOUT:
        if (Text_IsOpen()) return;
        s_bo_fade_step  = 0;
        s_bo_fade_timer = 8;
        Display_SetPalette(kBlackoutFadeOut[0][0], kBlackoutFadeOut[0][1], kBlackoutFadeOut[0][2]);
        s_phase = PZ_BLACKOUT_FADE;
        return;

    case PZ_BLACKOUT_FADE:
        if (--s_bo_fade_timer > 0) return;
        s_bo_fade_step++;
        if (s_bo_fade_step < 4) {
            Display_SetPalette(kBlackoutFadeOut[s_bo_fade_step][0],
                                kBlackoutFadeOut[s_bo_fade_step][1],
                                kBlackoutFadeOut[s_bo_fade_step][2]);
            s_bo_fade_timer = 8;
            return;
        }

        poison_blackout_heal_warp_and_music();

        Display_SetPalette(0xFF, 0xFF, 0xFF);
        s_bo_hold = 20;
        s_phase = PZ_BLACKOUT_SETTLE;
        return;

    case PZ_BLACKOUT_SETTLE:
        if (--s_bo_hold > 0) return;

        Display_LoadMapPalette();
        s_bo_target_bgp  = Display_GetBGP();
        s_bo_target_obp0 = Display_GetOBP0();
        s_bo_target_obp1 = Display_GetOBP1();
        Display_SetPalette(0xFF, 0xFF, 0xFF);
        if (s_bo_target_bgp == 0xE4 && s_bo_target_obp0 == 0xD0 && s_bo_target_obp1 == 0xE0) {

            s_bo_fade_step  = 2;
            s_bo_fade_timer = 8;
            s_phase = PZ_BLACKOUT_FADE_IN;
        } else {

            Display_SetPalette(s_bo_target_bgp, s_bo_target_obp0, s_bo_target_obp1);

            GbcColor_MarkDirty();
            s_phase = PZ_NONE;
        }
        return;

    case PZ_BLACKOUT_FADE_IN:
        if (--s_bo_fade_timer > 0) return;
        if (s_bo_fade_step >= 0) {

            Display_SetPalette(kBlackoutFadeOut[s_bo_fade_step][0],
                                kBlackoutFadeOut[s_bo_fade_step][1],
                                kBlackoutFadeOut[s_bo_fade_step][2]);
            s_bo_fade_step--;
            s_bo_fade_timer = 8;
            return;
        }
        Display_SetPalette(s_bo_target_bgp, s_bo_target_obp0, s_bo_target_obp1);

        GbcColor_MarkDirty();
        s_phase = PZ_NONE;
        return;

    default:
        return;
    }
}

void Poison_DebugApply(int party_slot) {
    if (party_slot < 0 || party_slot >= (int)wPartyCount || party_slot >= PARTY_LENGTH) return;
    wPartyMons[party_slot].base.status |= STATUS_PSN;
}

int Poison_IsBlackingOut(void) {
    return s_phase >= PZ_SHOWING_BLACKOUT && s_phase <= PZ_BLACKOUT_FADE_IN;
}

void Poison_StartBattleBlackout(void) {

    Display_SetPalette(0xFF, 0xFF, 0xFF);
    poison_blackout_heal_warp_and_music();
    Display_SetPalette(0xFF, 0xFF, 0xFF);
    s_bo_hold = 20;
    s_phase = PZ_BLACKOUT_SETTLE;
}
