
#include "map_music.h"

#include "music.h"
#include "johto_music.h"
#include "overworld.h"
#include "amberscript_core.h"
#include "amberscript_mapbank.h"
#include "../data/map_data.h"
#include "../platform/hardware.h"
#include <string.h>

typedef struct { int johto; int id; } track_t;

#define TRACK_NONE ((track_t){ 0, 0 })

static track_t s_cur = { 0, 0 };
static int     s_dont_play_on_reload = 0;

static track_t s_pending = { 0, 0 };
static int     s_pending_delay = 0;

static int track_eq(track_t a, track_t b) {
    return a.johto == b.johto && a.id == b.id;
}

static int on_vmap(uint8_t map_id) {
    return AmberScript_IsEnabled() &&
           map_id >= PKS_VIRTUAL_MAP_FIRST && map_id <= PKS_VIRTUAL_MAP_LAST;
}

static track_t get_map_music(uint8_t map_id) {
    track_t t = TRACK_NONE;
    if (on_vmap(map_id)) {
        char name[32];
        if (AmberScript_MapBank_GetMusicForRealId(map_id, name, sizeof(name))) {
            uint8_t kanto = KantoMusic_ForTrackName(name);
            if (kanto != MUSIC_NONE) { t.johto = 0; t.id = kanto; return t; }
            johto_music_id_t j = JohtoMusic_ForTrackName(name);
            if (j != JOHTO_MUSIC_NONE) { t.johto = 1; t.id = (int)j; return t; }
        }
        return t;
    }
    t.johto = 0;
    t.id = Music_GetMapID(map_id);
    return t;
}

static track_t resolve(uint8_t map_id) {
    track_t t = get_map_music(map_id);
    if (wWalkBikeSurfState == 2) {
        t.id = t.johto ? CRYSTAL_MUSIC_SURF : MUSIC_SURFING;
        return t;
    }
    if (wWalkBikeSurfState == 1) {
        t.id = t.johto ? CRYSTAL_MUSIC_BICYCLE : MUSIC_BIKE_RIDING;
        return t;
    }
    return t;
}

static void start(track_t t) {

    if (t.id == 0) return;
    if (t.johto) JohtoMusic_Play((johto_music_id_t)t.id);
    else         Music_Play((uint8_t)t.id);
}

static map_music_transition_t s_next_transition = MAPMUSIC_CUT;

void MapMusic_SetNextTransition(map_music_transition_t kind) {
    s_next_transition = kind;
}

void MapMusic_PlayForMap(uint8_t map_id) {
    if (s_next_transition == MAPMUSIC_FADE) {
        s_next_transition = MAPMUSIC_CUT;
        MapMusic_FadeToForMap(map_id);
        return;
    }
    track_t want = resolve(map_id);
    if (track_eq(want, s_cur) && !s_pending_delay) return;

    if (Music_IsPlaying()) Music_Stop();
    if (JohtoMusic_IsPlaying()) JohtoMusic_Stop();

    s_cur = want;
    s_pending = want;
    s_pending_delay = 1;
}

void MapMusic_FadeToForMap(uint8_t map_id) {
    track_t want = resolve(map_id);
    if (track_eq(want, s_cur)) return;

    if (want.johto && s_cur.johto && JohtoMusic_IsPlaying()) {
        s_cur = want;
        JohtoMusic_FadeTo((johto_music_id_t)want.id, 8);
        return;
    }
    if (!want.johto && !s_cur.johto && Music_IsPlaying()) {
        s_cur = want;
        Music_PlayDefaultFadeOutCurrent((uint8_t)want.id);
        return;
    }
    MapMusic_PlayForMap(map_id);
}

void MapMusic_Play(void)   { MapMusic_PlayForMap(wCurMap); }
void MapMusic_FadeTo(void) { MapMusic_FadeToForMap(wCurMap); }

void MapMusic_Restart(void) {

    s_cur = TRACK_NONE;
    MapMusic_PlayForMap(wCurMap);
}

void MapMusic_RestartForMap(uint8_t map_id) {
    s_cur = TRACK_NONE;
    MapMusic_PlayForMap(map_id);
}

void MapMusic_TryRestart(void) {
    if (s_dont_play_on_reload) {

        s_dont_play_on_reload = 0;
        s_cur = TRACK_NONE;
        s_pending_delay = 0;
        if (Music_IsPlaying()) Music_Stop();
        if (JohtoMusic_IsPlaying()) JohtoMusic_Stop();
        return;
    }
    MapMusic_Restart();
}

void MapMusic_SetDontPlayOnReload(int on) { s_dont_play_on_reload = on ? 1 : 0; }

void MapMusic_Forget(void) {
    s_cur = TRACK_NONE;
    s_pending_delay = 0;
}

void MapMusic_Adopt(void) {
    s_pending_delay = 0;
    if (!JohtoMusic_IsPlaying() && Music_IsPlaying()) {
        s_cur.johto = 0;
        s_cur.id    = Music_CurrentId();
        return;
    }
    s_cur = TRACK_NONE;
}

void MapMusic_Tick(void) {
    if (!s_pending_delay) return;
    if (--s_pending_delay == 0) start(s_pending);
}
