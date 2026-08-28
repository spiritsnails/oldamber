
#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdint.h>

void AudioEngine_SetEngine(int engine);
int  AudioEngine_GetEngine(void);

void AudioEngine_PlayMusic(uint8_t sound_id, int engine);

void AudioEngine_PlaySound(uint8_t sound_id);

void AudioEngine_Tick(void);

void AudioEngine_UpdateMusic(void);

void AudioEngine_SetLowHealthAlarm(uint8_t v);
int  AudioEngine_LowHealthAlarmActive(void);

void AudioEngine_LowHealthAlarmOn(void);
void AudioEngine_LowHealthAlarmOff(void);
void AudioEngine_LowHealthAlarmDisable(void);

void AudioEngine_OverwriteChannelPointer(int chan, uint16_t addr);
void AudioEngine_SetFadeOutControl(uint8_t v);
void AudioEngine_SetNoFadeOut(int on);
void AudioEngine_SetCryModifiers(uint8_t freq_mod, uint8_t tempo_mod);

void AudioEngine_PlayCry(uint8_t species);

void AudioEngine_StopAll(void);

int AudioEngine_IsSoundPlaying(uint8_t rom_id);
int AudioEngine_IsSfxPlaying(void);
int AudioEngine_IsCryPlaying(void);

int AudioEngine_IsMusicPlaying(void);

#define AUDIO_CHAN1 0
#define AUDIO_CHAN2 1
#define AUDIO_CHAN3 2
#define AUDIO_CHAN4 3
#define AUDIO_CHAN5 4
#define AUDIO_CHAN6 5
#define AUDIO_CHAN7 6
#define AUDIO_CHAN8 7
uint8_t AudioEngine_ChannelSoundId(int chan);

int  AudioEngine_IsReady(void);

const uint8_t *AudioEngine_ApuRegs(void);

void AudioEngine_SetWriteHook(void (*fn)(uint8_t lo, uint8_t value));

typedef struct {
    uint8_t  sound_ids[8];
    uint16_t command_pointers[8];
    uint8_t  note_delay_counters[8];
    uint8_t  flags1[8];
} AudioEngineState;

void AudioEngine_GetState(AudioEngineState *out);

#endif
