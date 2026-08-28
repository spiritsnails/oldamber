#include "reds_house1f_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "music.h"
#include "pokemon.h"
#include "overworld.h"
#include "npc.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include "map_music.h"

#define MAP_REDS_HOUSE_1F 0x25

typedef enum {
    RHS_IDLE = 0,
    RHS_WAIT_REST_TEXT_CLOSE,
    RHS_FADE_OUT,
    RHS_WAIT_JINGLE,
    RHS_FADE_IN,
    RHS_WAIT_LOOKING_GREAT_CLOSE,
} reds_house_state_t;

static reds_house_state_t g_state = RHS_IDLE;
static int g_fade_step = 0;
static int g_fade_timer = 0;

#define kMomWakeUpText (RomText("RedsHouse1FMomText.WakeUpText"))

#define kMomYouShouldRestText (RomText("RedsHouse1FMomYouShouldRestText"))

#define kMomLookingGreatText (RomText("RedsHouse1FMomLookingGreatText"))

static const uint8_t kFadeOutToWhite[3][3] = {
    {0x90, 0x80, 0x90},
    {0x40, 0x40, 0x40},
    {0x00, 0x00, 0x00},
};

static const uint8_t kFadeInFromWhite[3][3] = {
    {0x40, 0x40, 0x40},
    {0x90, 0x80, 0x90},
    {0xE4, 0xD0, 0xE0},
};

void RedsHouse1FScripts_OnMapLoad(void) {
    if (wCurMap != MAP_REDS_HOUSE_1F) return;
    g_state = RHS_IDLE;
    g_fade_step = 0;
    g_fade_timer = 0;
    Display_LoadMapPalette();
}

int RedsHouse1FScripts_IsActive(void) {
    return g_state != RHS_IDLE;
}

void RedsHouse1FScripts_Tick(void) {
    if (wCurMap != MAP_REDS_HOUSE_1F) {
        g_state = RHS_IDLE;
        return;
    }

    switch (g_state) {
    case RHS_IDLE:
        return;

    case RHS_WAIT_REST_TEXT_CLOSE:
        if (Text_IsOpen()) { Text_Update(); return; }
        g_fade_step = 0;
        g_fade_timer = 8;
        Display_SetPalette(kFadeOutToWhite[0][0], kFadeOutToWhite[0][1], kFadeOutToWhite[0][2]);
        g_state = RHS_FADE_OUT;
        return;

    case RHS_FADE_OUT:
        if (--g_fade_timer > 0) return;
        g_fade_step++;
        if (g_fade_step < 3) {
            Display_SetPalette(kFadeOutToWhite[g_fade_step][0],
                               kFadeOutToWhite[g_fade_step][1],
                               kFadeOutToWhite[g_fade_step][2]);
            g_fade_timer = 8;
            return;
        }

        Map_BuildScrollView();
        NPC_BuildView(0, 0);
        Pokemon_HealParty();
        Music_Play(MUSIC_PKMN_HEALED);
        g_state = RHS_WAIT_JINGLE;
        return;

    case RHS_WAIT_JINGLE:
        if (Music_IsPlaying()) return;
        MapMusic_Restart();
        g_fade_step = 0;
        g_fade_timer = 8;
        Display_SetPalette(kFadeInFromWhite[0][0], kFadeInFromWhite[0][1], kFadeInFromWhite[0][2]);
        g_state = RHS_FADE_IN;
        return;

    case RHS_FADE_IN:
        if (--g_fade_timer > 0) return;
        g_fade_step++;
        if (g_fade_step < 3) {
            Display_SetPalette(kFadeInFromWhite[g_fade_step][0],
                               kFadeInFromWhite[g_fade_step][1],
                               kFadeInFromWhite[g_fade_step][2]);
            g_fade_timer = 8;
            return;
        }
        Display_LoadMapPalette();
        Text_ShowASCII(kMomLookingGreatText);
        g_state = RHS_WAIT_LOOKING_GREAT_CLOSE;
        return;

    case RHS_WAIT_LOOKING_GREAT_CLOSE:
        if (Text_IsOpen()) { Text_Update(); return; }
        g_state = RHS_IDLE;
        return;
    }
}
