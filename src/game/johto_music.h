#pragma once
#include <stdint.h>

#include "crystal_audio.h"

typedef int johto_music_id_t;
#define JOHTO_MUSIC_NONE CRYSTAL_MUSIC_NONE

void JohtoMusic_Play(johto_music_id_t id);
void JohtoMusic_Stop(void);

void JohtoMusic_Update(void);

void JohtoAudio_PlaySFX(int sfx_id);
void JohtoAudio_PlayCry(int species);

void JohtoAudio_PlayCryModified(int species, int pitch_add, int tempo_add);

int  JohtoAudio_IsCryPlaying(void);

void JohtoAudio_SetStereo(int on);
int  JohtoAudio_GetStereo(void);

void JohtoMusic_FadeTo(johto_music_id_t id, int frames_per_step);
int  JohtoMusic_IsFading(void);
int  JohtoMusic_IsPlaying(void);

int  JohtoMusic_IsCurrentTrack(johto_music_id_t id);

johto_music_id_t JohtoMusic_ForTrackName(const char *track_name);
