#pragma once
#include <stdint.h>

#define MUSIC_NONE            0
#define MUSIC_PALLET_TOWN     1
#define MUSIC_POKECENTER      2
#define MUSIC_GYM             3
#define MUSIC_CITIES1         4
#define MUSIC_CITIES2         5
#define MUSIC_CELADON         6
#define MUSIC_CINNABAR        7
#define MUSIC_VERMILION       8
#define MUSIC_LAVENDER        9
#define MUSIC_SS_ANNE         10
#define MUSIC_ROUTES1         11
#define MUSIC_ROUTES2         12
#define MUSIC_ROUTES3         13
#define MUSIC_ROUTES4         14
#define MUSIC_INDIGO_PLATEAU  15
#define MUSIC_OAKS_LAB        16
#define MUSIC_DUNGEON1        17
#define MUSIC_DUNGEON2        18
#define MUSIC_DUNGEON3        19
#define MUSIC_POKEMON_TOWER   20
#define MUSIC_SILPH_CO        21
#define MUSIC_SAFARI_ZONE     22
#define MUSIC_TITLE           23
#define MUSIC_JIGGLYPUFF      24
#define MUSIC_WILD_BATTLE          25
#define MUSIC_DEFEATED_WILD_MON    26
#define MUSIC_DEFEATED_TRAINER     27
#define MUSIC_DEFEATED_GYM_LEADER  28
#define MUSIC_PKMN_HEALED          29
#define MUSIC_GYM_LEADER_BATTLE    30
#define MUSIC_TRAINER_BATTLE       31
#define MUSIC_MEET_RIVAL           32
#define MUSIC_MEET_MALE_TRAINER    33
#define MUSIC_MEET_FEMALE_TRAINER  34
#define MUSIC_MUSEUM_GUY          35
#define MUSIC_MEET_EVIL_TRAINER   36
#define MUSIC_SURFING             37
#define MUSIC_MEET_PROF_OAK       38
#define MUSIC_INTRO_BATTLE        39
#define MUSIC_GAME_CORNER         40
#define MUSIC_BIKE_RIDING         41
#define MUSIC_CINNABAR_MANSION    42
#define MUSIC_FINAL_BATTLE        43
#define MUSIC_HALL_OF_FAME        44
#define MUSIC_CREDITS             45

#define MUSIC_POKEFLUTE           46
#define MUSIC_CHAMPION_BATTLE     MUSIC_FINAL_BATTLE

void Music_Play(uint8_t music_id);

void Music_PlayCities1AlternateTempo(void);

void Music_PlayFromLoop(uint8_t music_id);

void Music_PlayRivalAlternateStart(void);

void Music_PlayRivalAlternateTempo(void);

void Music_PlayRivalAlternateStartAndTempo(void);

void Music_PlayDefaultFadeOutCurrent(uint8_t music_id);
void Music_Stop(void);

void Music_Update(void);

void Music_SuspendChannel(int ch);
void Music_ResumeChannel(int ch);

int  Music_IsChannelSuspended(int ch);

int Music_IsPlaying(void);

uint8_t Music_CurrentId(void);

uint8_t Music_GetMapID(uint8_t map_id);

uint8_t KantoMusic_ForTrackName(const char *track);
