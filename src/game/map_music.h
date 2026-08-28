#pragma once
#include <stdint.h>

void MapMusic_Play(void);

void MapMusic_FadeTo(void);

void MapMusic_PlayForMap(uint8_t map_id);
void MapMusic_FadeToForMap(uint8_t map_id);

typedef enum { MAPMUSIC_CUT = 0, MAPMUSIC_FADE } map_music_transition_t;
void MapMusic_SetNextTransition(map_music_transition_t kind);

void MapMusic_Restart(void);
void MapMusic_RestartForMap(uint8_t map_id);
void MapMusic_TryRestart(void);

void MapMusic_SetDontPlayOnReload(int on);

void MapMusic_Forget(void);

void MapMusic_Adopt(void);

void MapMusic_Tick(void);
