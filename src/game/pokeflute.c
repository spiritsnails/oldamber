
#include "pokeflute.h"
#include "npc.h"
#include "text.h"
#include "music.h"
#include "map_music.h"
#include "overworld.h"
#include "amberscript_mapbank.h"
#include "../data/map_data.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <string.h>

#define MAP_ROUTE12  0x17
#define MAP_ROUTE16  0x1b

static const int kR12Coords[][2] = {
    {  9, 62 },
    { 10, 61 },
    { 10, 63 },
    { 11, 62 },
    { -1, -1 }
};
static const int kR16Coords[][2] = {
    { 27, 10 },
    { 25, 10 },
    { -1, -1 }
};

static int s_fight_map     = 0;
static int s_fight_pending = 0;
static int s_in_battle     = 0;

enum {
    PF_IDLE = 0,
    PF_FLUTE_TEXT,
    PF_JINGLE,
    PF_WOKE_TEXT
};
static int s_state = PF_IDLE;
static int s_jingle_started = 0;
static int s_jingle_frames  = 0;

static int map_is_route(uint8_t map, uint8_t rom_id, const char *vmap_name) {
    if (map == rom_id) return 1;
    if (map < PKS_VIRTUAL_MAP_FIRST || map > PKS_VIRTUAL_MAP_LAST) return 0;
    {

        const char *n = AmberScript_MapBank_NameForRealId(map);
        return n && strcasecmp(n, vmap_name) == 0;
    }
}

static int coords_match(const int c[][2]) {
    for (int i = 0; c[i][0] != -1; i++) {
        if ((int)wXCoord == c[i][0] && (int)wYCoord == c[i][1])
            return 1;
    }
    return 0;
}

static void hide_snorlax(int x, int y, int fallback_slot) {
    int idx = AmberScript_MapFindLiveNpcByDeclaredTile((int)wCurMap, x, y);
    if (idx < 0) idx = NPC_FindAtTile(x, y);
    if (idx < 0) idx = fallback_slot;
    if (idx >= 0) NPC_HideSprite(idx);
}

void PokeFlute_Use(void) {

    if (map_is_route(wCurMap, MAP_ROUTE12, "Route12") &&
        !CheckEvent(EVENT_BEAT_ROUTE12_SNORLAX) && coords_match(kR12Coords)) {
        s_fight_map = MAP_ROUTE12;
    } else if (map_is_route(wCurMap, MAP_ROUTE16, "Route16") &&
               !CheckEvent(EVENT_BEAT_ROUTE16_SNORLAX) && coords_match(kR16Coords)) {
        s_fight_map = MAP_ROUTE16;
    } else {

        Text_ShowASCII("Played the #\nFLUTE.\fNow, that's a\ncatchy tune!");
        return;
    }

    Text_HoldAfterPrompt();
    Text_ShowASCII("{PLAYER} played the\n# FLUTE.");
    s_jingle_started = 0;
    s_jingle_frames  = 0;
    s_state = PF_FLUTE_TEXT;
}

void PokeFlute_Tick(void) {
    switch (s_state) {
    case PF_IDLE:
        return;

    case PF_FLUTE_TEXT:

        if (!Text_IsHeldAfterPrompt()) return;
        Music_Stop();
        Music_Play(MUSIC_POKEFLUTE);
        s_jingle_started = 0;
        s_jingle_frames  = 0;
        s_state = PF_JINGLE;
        return;

    case PF_JINGLE:

        s_jingle_frames++;
        if (!s_jingle_started) {
            if (Music_IsPlaying()) s_jingle_started = 1;
            else if (s_jingle_frames < 60) return;
        }
        if (Music_IsPlaying() && s_jingle_frames < 900) return;

        MapMusic_Restart();
        Text_Close();

        SetEvent(s_fight_map == MAP_ROUTE12 ? EVENT_FIGHT_ROUTE12_SNORLAX
                                            : EVENT_FIGHT_ROUTE16_SNORLAX);
        s_fight_map = 0;
        s_state = PF_IDLE;
        return;

    case PF_WOKE_TEXT:

        s_state = PF_IDLE;
        return;
    }
}

int PokeFlute_ConsumeSnorlaxBattle(void) {
    if (!s_fight_pending) return 0;
    s_fight_pending = 0;
    s_in_battle     = 1;
    return 1;
}

int PokeFlute_ConsumeSnorlaxPostBattle(void) {
    if (!s_in_battle) return 0;
    s_in_battle = 0;
    return 1;
}

void PokeFlute_OnSnorlaxVictory(void) {

    if (s_fight_map == MAP_ROUTE12) {
        SetEvent(EVENT_BEAT_ROUTE12_SNORLAX);
        ClearEvent(EVENT_FIGHT_ROUTE12_SNORLAX);

        Text_ShowASCII("SNORLAX calmed\ndown! With a big\nyawn, it returned\nto the mountains!");
    } else if (s_fight_map == MAP_ROUTE16) {
        SetEvent(EVENT_BEAT_ROUTE16_SNORLAX);
        ClearEvent(EVENT_FIGHT_ROUTE16_SNORLAX);

        Text_ShowASCII("With a big yawn,\nSNORLAX returned\nto the mountains!");
    }
    s_fight_map = 0;
}

void PokeFlute_OnSnorlaxCaught(void) {

    if (s_fight_map == MAP_ROUTE12) {
        SetEvent(EVENT_BEAT_ROUTE12_SNORLAX);
        ClearEvent(EVENT_FIGHT_ROUTE12_SNORLAX);
    } else if (s_fight_map == MAP_ROUTE16) {
        SetEvent(EVENT_BEAT_ROUTE16_SNORLAX);
        ClearEvent(EVENT_FIGHT_ROUTE16_SNORLAX);
    }
    s_fight_map = 0;
}

void PokeFlute_LoadMap(void) {

    if (map_is_route(wCurMap, MAP_ROUTE12, "Route12") &&
        CheckEvent(EVENT_BEAT_ROUTE12_SNORLAX))
        hide_snorlax(10, 62, 0);
    if (map_is_route(wCurMap, MAP_ROUTE16, "Route16") &&
        CheckEvent(EVENT_BEAT_ROUTE16_SNORLAX))
        hide_snorlax(26, 10, 6);

    s_state = PF_IDLE;
}
