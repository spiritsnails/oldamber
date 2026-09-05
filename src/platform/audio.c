
#include "audio.h"
#include "gb_apu.h"
#include "game/audio_engine.h"
#include "sfx_rom_ids.h"

#include <stdio.h>

static FILE *s_apu_log; static int s_apu_log_frames; static int s_apu_arms = 40;
static unsigned s_apu_frame;
static void apu_log_write(const char *src, uint8_t lo, uint8_t val) {
    if (s_apu_log && s_apu_log_frames > 0)
        fprintf(s_apu_log, "%u %s %02X=%02X\n", s_apu_frame, src, lo, val);
}
static void apu_log_arm(unsigned dense, unsigned rom) {
    if (s_apu_arms <= 0) return;
    if (!s_apu_log) {
        s_apu_log = fopen("apu_trace.txt", "w");
        if (!s_apu_log) { s_apu_arms = 0; return; }
        setvbuf(s_apu_log, NULL, _IOLBF, 0);
    }
    s_apu_arms--; s_apu_log_frames = 200;
    fprintf(s_apu_log, "=== arm sfx=%u rom=%u frame=%u\n", dense, rom, s_apu_frame);
}

static void engine_write(uint8_t lo, uint8_t val) {
    apu_log_write("eng", lo, val);
    GbApu_WriteReg(lo, val);
}

static void play_sfx_rom(uint16_t dense) {
    if (dense >= (uint16_t)(sizeof kSfxRom / sizeof kSfxRom[0])) return;

    apu_log_arm(dense, kSfxRom[dense].rom_id);
    AudioEngine_PlaySound(kSfxRom[dense].rom_id);
}

static int sfx_rom_playing(uint16_t dense) {
    if (dense >= (uint16_t)(sizeof kSfxRom / sizeof kSfxRom[0])) return 0;
    return AudioEngine_IsSoundPlaying(kSfxRom[dense].rom_id);
}

static inline uint8_t apu_reg_addr(int channel, int reg) {
    return (uint8_t)(0x10 + channel * 5 + reg);
}

#include "../game/music.h"
#include "../game/johto_music.h"
#include "../game/gen2_species.h"
#include "../game/map_music.h"
#include "../data/cry_data.h"
#include "../data/move_sfx_structs.h"
#include "hardware.h"
#include <SDL.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

typedef struct {
    int      enabled;
    uint32_t phase;
    uint32_t phase_inc;
    uint8_t  volume;
    uint8_t  duty;

    uint8_t  wave_ram[16];
    int      wave_pos;
    uint32_t wave_start_phase;
    uint8_t  wave_playing;
    uint8_t  wave_vol_shift;

    uint16_t lfsr;
    int      lfsr_narrow;
    uint16_t ch4_attack_boost;

    uint16_t freq_reg;

    uint8_t  sweep_pace;
    uint8_t  sweep_dir;
    uint8_t  sweep_shift;
    uint32_t sweep_timer;

    uint8_t  env_pace;
    uint8_t  env_dir;
    uint8_t  env_initial;
    uint32_t env_counter;
} gb_channel_t;

static gb_channel_t ch[4];
static SDL_AudioDeviceID audio_dev  = 0;
static SDL_mutex        *audio_mutex = NULL;
static int               s_cry_mix_boost = 0;
static int               s_pc_sfx_mix_boost = 0;
static float             s_master_mix_current = 1.0f;

#define AUDIO_VOL_MAX 10
static int   s_vol_master = AUDIO_VOL_MAX;
static int   s_vol_music  = AUDIO_VOL_MAX;
static int   s_vol_sfx    = AUDIO_VOL_MAX;
static float s_user_master = 1.0f;
static int   s_output_mono;
static int   s_focus_muted;

static uint8_t s_nr50 = 0x77;
static uint8_t s_nr51 = 0xFF;
typedef struct {
    uint8_t active;
    uint16_t cmd_pos;
    int timer;
    uint8_t rotate_duty;
    uint8_t duty_state;
    uint8_t fixed_duty;
    uint8_t sweep_reg;
    int8_t loop_rem[64];

    uint16_t cmd_first;
    uint16_t cmd_count;
} move_sfx_chan_rt_t;
static struct {
    uint8_t active;
    uint8_t suspended[4];
    int8_t pitch_add;
    uint16_t tempo;
    move_sfx_chan_rt_t ch[4];
} sMoveSfx = {0};
static int sMoveSfxDebug = 0;

static uint32_t freq_to_phase_inc(uint16_t gb_freq, int is_wave) {
    uint32_t base  = is_wave ? 65536u : 131072u;
    uint32_t denom = (uint32_t)(2048u - (gb_freq & 0x7FFu));
    if (denom == 0) return 0;
    return (uint32_t)((uint64_t)base * (1u << 24) /
                      ((uint64_t)AUDIO_SAMPLE_RATE * denom));
}

static uint32_t noise_freq_to_phase_inc(uint8_t nr43) {
    uint32_t shift = (nr43 >> 4) & 0xF;
    uint32_t r     = nr43 & 7;
    if (shift >= 15) return 0;
    uint64_t hz = (r == 0) ? ((uint64_t)1048576 >> (shift + 1))
                           : ((uint64_t)524288 / r >> (shift + 1));
    if (hz == 0) return 0;
    return (uint32_t)(hz * (1u << 24) / AUDIO_SAMPLE_RATE);
}

static void audio_callback(void *userdata, uint8_t *stream, int len) {
    (void)userdata;
    int16_t *out     = (int16_t *)stream;

    int      samples = len / (int)(sizeof(int16_t) * AUDIO_CHANNELS);

    SDL_LockMutex(audio_mutex);

    {

        static float fbuf[AUDIO_BUFFER_SIZE * 4 * AUDIO_CHANNELS];
        int done = 0;
        while (done < samples) {
            int chunk = samples - done;
            if (chunk > AUDIO_BUFFER_SIZE * 4) chunk = AUDIO_BUFFER_SIZE * 4;
            GbApu_RenderSamples(fbuf, chunk);
            for (int i = 0; i < chunk; i++) {
                float l = fbuf[i * AUDIO_CHANNELS + 0];
                float r = fbuf[i * AUDIO_CHANNELS + 1];
                if (s_output_mono) l = r = (l + r) * 0.5f;
                if (s_focus_muted) l = r = 0.0f;
                l *= s_master_mix_current * s_user_master;
                r *= s_master_mix_current * s_user_master;
                if (l >  1.0f) l =  1.0f;
                if (l < -1.0f) l = -1.0f;
                if (r >  1.0f) r =  1.0f;
                if (r < -1.0f) r = -1.0f;

                *out++ = (int16_t)(l * 16320.0f);
                *out++ = (int16_t)(r * 16320.0f);
            }
            done += chunk;
        }
    }

    SDL_UnlockMutex(audio_mutex);
}

static void set_wave_pattern(const uint8_t pattern[16]) {
    for (int i = 0; i < 16; i++)
        GbApu_WriteReg((uint8_t)(0x30 + i), pattern[i]);

    uint32_t start_phase = 0;
    for (int i = 1; i < 32; i++) {
        int prev = (pattern[(i-1)/2] >> ((i-1)&1 ? 0 : 4)) & 0xF;
        int curr = (pattern[i  /2] >> (i    &1 ? 0 : 4)) & 0xF;
        if (prev < 8 && curr >= 8) { start_phase = (uint32_t)i * (1u << 19); break; }
    }
    if (audio_mutex) SDL_LockMutex(audio_mutex);
    memcpy(ch[2].wave_ram, pattern, 16);
    ch[2].wave_start_phase = start_phase;
    if (audio_mutex) SDL_UnlockMutex(audio_mutex);
}

void Audio_SetWaveInstrument(int idx) {
    if (idx < 0 || idx >= 5) idx = 0;
    set_wave_pattern(kWavePatterns[idx]);
}

void Audio_SetWaveRaw(const uint8_t pattern[16]) {
    set_wave_pattern(pattern);
}

void Audio_WriteNR50(uint8_t v) {
    GbApu_WriteReg(0x24, v);
    if (!audio_mutex) { s_nr50 = v; return; }
    SDL_LockMutex(audio_mutex);
    s_nr50 = v;
    SDL_UnlockMutex(audio_mutex);
}

void Audio_WriteNR51(uint8_t v) {
    GbApu_WriteReg(0x25, v);
    if (!audio_mutex) { s_nr51 = v; return; }
    SDL_LockMutex(audio_mutex);
    s_nr51 = v;
    SDL_UnlockMutex(audio_mutex);
}

void Audio_SetMixVolume(uint8_t level) {
    if (level > 15) level = 15;
    float target = (float)level / 15.0f;
    if (!audio_mutex) {
        s_master_mix_current = target;
        return;
    }
    SDL_LockMutex(audio_mutex);
    s_master_mix_current = target;
    SDL_UnlockMutex(audio_mutex);
}

void Audio_SetMixVolumeImmediate(uint8_t level) {
    if (level > 15) level = 15;
    float target = (float)level / 15.0f;
    if (!audio_mutex) {
        s_master_mix_current = target;
        return;
    }
    SDL_LockMutex(audio_mutex);
    s_master_mix_current = target;
    SDL_UnlockMutex(audio_mutex);
}

float Audio_GetMixLevel(void) {
    if (!audio_mutex) return s_master_mix_current;
    SDL_LockMutex(audio_mutex);
    float current = s_master_mix_current;
    SDL_UnlockMutex(audio_mutex);
    return current;
}

int Audio_Init(void) {

    GbApu_Reset();
    GbApu_SetOutputRate(AUDIO_SAMPLE_RATE);
    GbApu_WriteReg(0x26, 0x80);
    GbApu_WriteReg(0x24, 0x77);
    GbApu_WriteReg(0x25, 0xFF);
    AudioEngine_SetWriteHook(engine_write);

    memset(ch, 0, sizeof(ch));
    s_master_mix_current = 1.0f;

    ch[3].lfsr = 0x7FFF;

    Audio_SetWaveInstrument(0);

    audio_mutex = SDL_CreateMutex();
    if (!audio_mutex) return -1;

    SDL_AudioSpec want = {0}, have;
    want.freq     = AUDIO_SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = AUDIO_CHANNELS;
    want.samples  = AUDIO_BUFFER_SIZE;
    want.callback = audio_callback;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                    SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!audio_dev) {
        printf("[audio] exact open failed (%s); retrying with a flexible rate\n",
               SDL_GetError());
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                        SDL_AUDIO_ALLOW_SAMPLES_CHANGE |
                                        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    }
    if (!audio_dev) {
        printf("[audio] no audio device: %s (driver=%s)\n",
               SDL_GetError(),
               SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none");
        fflush(stdout);
        return -1;
    }
    printf("[audio] driver=%s rate=%d ch=%d buf=%d\n",
           SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?",
           have.freq, have.channels, have.samples);
    if (have.freq != want.freq)
        printf("[audio] WARNING: device rate %d != %d, pitch will be off\n",
               have.freq, want.freq);
    fflush(stdout);

    SDL_PauseAudioDevice(audio_dev, 0);
    return 0;
}

void Audio_WriteReg(int channel, int reg, uint8_t value) {
    if (channel < 0 || channel > 3) return;
    SDL_LockMutex(audio_mutex);
    if (reg >= 0 && reg <= 4) {

        if (channel == 2) {
            if (reg == 4 && (value & 0x80))         GbApu_WriteReg(0x1A, 0x80);
            else if (reg == 2 && ((value >> 4) & 0xF) == 0) GbApu_WriteReg(0x1A, 0x00);
        }
        apu_log_write("raw", apu_reg_addr(channel, reg), value);
        GbApu_WriteReg(apu_reg_addr(channel, reg), value);
    }
    switch (reg) {
        case 0:
            if (channel == 0) {
                ch[0].sweep_pace  = (value >> 4) & 0x7;
                ch[0].sweep_dir   = (value >> 3) & 0x1;
                ch[0].sweep_shift = value & 0x7;
                ch[0].sweep_timer = 0;
            }
            break;
        case 1:
            if (channel < 2)
                ch[channel].duty = (value >> 6) & 3;
            break;
        case 2:
            if (channel == 2) {

                uint8_t vol_code = (value >> 4) & 0xF;

                if (vol_code > 3) vol_code = 1;
                ch[2].wave_vol_shift = vol_code;
                ch[2].env_pace = 0;
                if (vol_code == 0) {
                    ch[2].wave_playing = 0;
                }
            } else {
                ch[channel].env_initial = (value >> 4) & 0xF;
                ch[channel].env_dir     = (value >> 3) & 0x1;
                ch[channel].env_pace    = value & 0x7;
                ch[channel].volume      = ch[channel].env_initial;
                if (ch[channel].volume == 0)
                    ch[channel].enabled = 0;
            }
            break;
        case 3:
            if (channel == 3) {

                ch[3].freq_reg    = value;
                ch[3].lfsr_narrow = (value >> 3) & 1;
            } else {
                ch[channel].freq_reg = (ch[channel].freq_reg & 0x700u) |
                                        (uint16_t)value;
            }
            break;
        case 4:
            if (channel != 3) {
                ch[channel].freq_reg = (ch[channel].freq_reg & 0x00FFu) |
                                       ((uint16_t)(value & 0x07) << 8);
            }
            if (value & 0x80) {
                ch[channel].enabled = 1;
                if (channel == 2) {

                    if (!ch[2].wave_playing)
                        ch[2].phase = ch[2].wave_start_phase;
                    ch[2].wave_playing = 1;
                } else {
                    ch[channel].phase = 0u;
                }
                ch[channel].phase_inc = (channel == 3)
                    ? noise_freq_to_phase_inc((uint8_t)ch[3].freq_reg)
                    : freq_to_phase_inc(ch[channel].freq_reg, channel == 2);

                if (channel != 2) {
                    ch[channel].volume      = ch[channel].env_initial;
                    ch[channel].env_counter = 0;
                }
                if (channel == 3) {
                    ch[channel].lfsr = 0x7FFF;
                    ch[channel].ch4_attack_boost = (uint16_t)(AUDIO_SAMPLE_RATE / 125);
                }

                if (channel == 0) {
                    ch[0].sweep_timer = 0;
                    if (ch[0].sweep_pace > 0 || ch[0].sweep_shift > 0) {
                        if (ch[0].sweep_shift > 0) {
                            uint16_t delta = ch[0].freq_reg >> ch[0].sweep_shift;
                            uint16_t new_freq;
                            if (ch[0].sweep_dir == 0) {
                                new_freq = ch[0].freq_reg + delta;
                            } else {
                                new_freq = (delta > ch[0].freq_reg) ? 0 : ch[0].freq_reg - delta;
                            }
                            if (new_freq >= 2048) {
                                ch[0].enabled = 0;
                            }
                        }
                    }
                }
            }
            break;
    }
    SDL_UnlockMutex(audio_mutex);
}

typedef struct { uint16_t freq; uint8_t env_byte; uint8_t frames; } sfx_note_t;

static int sfx_cmd_len_frames(uint8_t len_field) { return (int)len_field + 1; }
static int sfx_cmd_len_frames_tempo(uint8_t len_field, uint16_t tempo) {
    int raw = (int)len_field + 1;
    int frames = (int)(((uint32_t)raw * (uint32_t)tempo) >> 8);
    return (frames > 0) ? frames : 1;
}

static uint8_t move_sfx_env_byte(int vol, int fade) {
    uint8_t nib = (fade < 0) ? (uint8_t)(0x8 | ((-fade) & 0x7)) : (uint8_t)(fade & 0x7);
    return (uint8_t)(((vol & 0xF) << 4) | nib);
}

static uint8_t move_sfx_sweep_byte(int pace, int slope) {
    uint8_t p = (uint8_t)(pace & 0xF);
    uint8_t s = (uint8_t)(slope < 0 ? (-slope) : slope) & 0x7;
    uint8_t d = (slope < 0) ? 0x8 : 0x0;
    return (uint8_t)((p << 4) | d | s);
}

static void move_sfx_channel_silence(uint8_t hw) {
    if (hw >= 4) return;
    if (hw == 0) Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(hw, 2, 0x00);
    Audio_WriteReg(hw, 4, 0x00);
}

static void move_sfx_fire_square(uint8_t hw, move_sfx_chan_rt_t *st, const move_sfx_cmd_t *cmd) {
    int len = cmd->p0;
    int vol = cmd->p1;
    int fade = cmd->p2;
    uint16_t base_freq = (uint16_t)(cmd->p3 & 0x7FF);
    uint8_t lo = (uint8_t)(base_freq & 0xFFu);
    uint8_t hi = (uint8_t)((base_freq >> 8) & 0x07u);
    uint16_t lo_sum = (uint16_t)lo + (uint8_t)sMoveSfx.pitch_add;

    lo = (uint8_t)(lo_sum & 0xFFu);
    if (lo_sum > 0xFFu) hi = (uint8_t)(hi + 1u);
    uint8_t duty = st->rotate_duty ? (uint8_t)((st->duty_state >> 6) & 0x3) : st->fixed_duty;
    if (hw == 0) Audio_WriteReg(0, 0, st->sweep_reg);
    Audio_WriteReg(hw, 1, (uint8_t)((duty << 6) | 0x3F));
    Audio_WriteReg(hw, 2, move_sfx_env_byte(vol, fade));
    Audio_WriteReg(hw, 3, lo);
    Audio_WriteReg(hw, 4, (uint8_t)((hi & 0x07u) | 0x80u));
    st->timer = sfx_cmd_len_frames_tempo((uint8_t)len, sMoveSfx.tempo);
}

static void move_sfx_fire_noise(move_sfx_chan_rt_t *st, const move_sfx_cmd_t *cmd) {
    int len = cmd->p0;
    int vol = cmd->p1;
    int fade = cmd->p2;
    int nr43 = cmd->p3;
    Audio_WriteReg(3, 2, move_sfx_env_byte(vol, fade));
    Audio_WriteReg(3, 3, (uint8_t)nr43);
    Audio_WriteReg(3, 4, 0x80);
    st->timer = sfx_cmd_len_frames_tempo((uint8_t)len, sMoveSfx.tempo);
}

static void move_sfx_advance_channel(uint8_t hw) {
    move_sfx_chan_rt_t *st;
    if (hw >= 4) return;
    st = &sMoveSfx.ch[hw];
    if (!st->active || !st->cmd_count) return;

    while (st->cmd_pos < st->cmd_count) {
        const move_sfx_cmd_t *cmd = &gSfxCmds[st->cmd_first + st->cmd_pos];
        switch (cmd->type) {
            case MOVE_SFX_CMD_DUTY_CYCLE:
                st->fixed_duty = (uint8_t)(cmd->p0 & 0x3);
                st->rotate_duty = 0;
                st->cmd_pos++;
                continue;
            case MOVE_SFX_CMD_DUTY_CYCLE_PATTERN:
                st->duty_state = (uint8_t)(((cmd->p0 & 0x3) << 6) |
                                           ((cmd->p1 & 0x3) << 4) |
                                           ((cmd->p2 & 0x3) << 2) |
                                           (cmd->p3 & 0x3));
                st->rotate_duty = 1;
                st->cmd_pos++;
                continue;
            case MOVE_SFX_CMD_PITCH_SWEEP:
                if (hw == 0) {
                    st->sweep_reg = move_sfx_sweep_byte(cmd->p0, cmd->p1);
                    Audio_WriteReg(0, 0, st->sweep_reg);
                }
                st->cmd_pos++;
                continue;
            case MOVE_SFX_CMD_SQUARE_NOTE:
                if (hw == 0 || hw == 1) {
                    move_sfx_fire_square(hw, st, cmd);
                }
                st->cmd_pos++;
                return;
            case MOVE_SFX_CMD_NOISE_NOTE:
                if (hw == 3) {
                    move_sfx_fire_noise(st, cmd);
                }
                st->cmd_pos++;
                return;
            case MOVE_SFX_CMD_SOUND_LOOP: {
                int count = cmd->p0;
                int target = cmd->p1;
                if (count == 0) {
                    st->cmd_pos = (uint16_t)((target >= 0) ? target : 0);
                    continue;
                }
                if (st->cmd_pos < 64u && st->loop_rem[st->cmd_pos] < 0) {
                    st->loop_rem[st->cmd_pos] = (int8_t)(count - 1);
                }
                if (st->cmd_pos < 64u && st->loop_rem[st->cmd_pos] > 0) {
                    st->loop_rem[st->cmd_pos]--;
                    st->cmd_pos = (uint16_t)((target >= 0) ? target : 0);
                    continue;
                }
                if (st->cmd_pos < 64u) st->loop_rem[st->cmd_pos] = -1;
                st->cmd_pos++;
                continue;
            }
            case MOVE_SFX_CMD_SOUND_RET:
            default:
                st->active = 0;
                st->timer = 0;
                move_sfx_channel_silence(hw);
                return;
        }
    }

    st->active = 0;
    st->timer = 0;
    move_sfx_channel_silence(hw);
}

int Audio_PlaySfx(uint16_t sfx_index) {
    return Audio_PlaySfxModified(sfx_index, 0, 0x80);
}

void Audio_SetMoveSfxDebug(int on) {
    sMoveSfxDebug = on ? 1 : 0;
}

int Audio_IsMoveSfxDebug(void) {
    return sMoveSfxDebug;
}

int Audio_PlaySfxModified(uint16_t sfx_index, int8_t pitch_add, uint8_t tempo_mod) {

    if (sfx_index >= (uint16_t)(sizeof kSfxRom / sizeof kSfxRom[0])) return 0;

    AudioEngine_SetCryModifiers((uint8_t)pitch_add, tempo_mod);
    apu_log_arm(sfx_index, kSfxRom[sfx_index].rom_id);
    AudioEngine_PlaySound(kSfxRom[sfx_index].rom_id);
    return 1;
}

static const sfx_note_t kPressAB[] = {
    { 1984, 0x91,  1 },
    { 2000, 0x81,  1 },
    { 1984, 0x91,  1 },
    { 2000, 0xA1, 13 },
};

static int sfx_step  = -1;
static int sfx_timer = 0;

static void cancel_pressab_sfx(void) {
    if (sfx_step >= 0) {
        sfx_step = -1;
        Music_ResumeChannel(0);
    }
}

static void sfx_fire(int step) {
    const sfx_note_t *n = &kPressAB[step];

    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env_byte);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_PressAB(void) {
    play_sfx_rom(SFX_PRESS_AB_1);
}

#define LEDGE_SFX_FRAMES 20
static int ledge_sfx_active = 0;
static int ledge_sfx_timer  = 0;

void Audio_PlaySFX_Ledge(void) {
    play_sfx_rom(SFX_LEDGE_1);
}

#define COLLISION_SFX_FRAMES 18
static int collision_sfx_active = 0;
static int collision_sfx_timer  = 0;

void Audio_PlaySFX_Collision(void) {

    if (AudioEngine_ChannelSoundId(AUDIO_CHAN5) == kSfxRom[SFX_COLLISION_1].rom_id)
        return;
    play_sfx_rom(SFX_COLLISION_1);
}

void Audio_PlaySFX_CollisionRetrigger(void) {
    play_sfx_rom(SFX_COLLISION_1);
}

typedef struct { uint8_t nr43; uint8_t env_byte; uint8_t frames; } noise_note_t;

static const noise_note_t kGoInside[] = {
    { 0x44, 0xF1,  9 },
    { 0x43, 0xD1,  8 },
};
static const noise_note_t kGoOutside[] = {
    { 0x54, 0xF1,  2 },
    { 0x23, 0x71, 12 },
    { 0x54, 0xB1,  2 },
    { 0x23, 0x61, 12 },
    { 0x54, 0x41,  6 },
};

static const noise_note_t *noise_sfx_seq   = NULL;
static int                 noise_sfx_count = 0;
static int                 noise_sfx_step  = -1;
static int                 noise_sfx_timer = 0;
static int                 noise_music_suspended = 0;

static void noise_sfx_fire(int step) {
    const noise_note_t *n = &noise_sfx_seq[step];
    Audio_WriteReg(3, 2, n->env_byte);
    Audio_WriteReg(3, 3, n->nr43);
    Audio_WriteReg(3, 4, 0x80);
    noise_sfx_timer = sfx_cmd_len_frames(n->frames);
}

static void play_noise_sfx(const noise_note_t *seq, int count) {
    if (!noise_music_suspended) {
        Music_SuspendChannel(3);
        noise_music_suspended = 1;
    }
    noise_sfx_seq   = seq;
    noise_sfx_count = count;
    noise_sfx_step  = 0;
    noise_sfx_fire(0);
}

static const noise_note_t kStartMenu[] = {
    { 0x33, 0xE2,  1 },
    { 0x22, 0xE1,  8 },
};

static const sfx_note_t kTurnOnPC[] = {
    { 1984, 0xF2, 15 },
    {    0, 0x00, 15 },
    { 1920, 0xA1,  3 },
    { 1792, 0xA1,  3 },
    { 1856, 0xA1,  3 },
    { 1792, 0xA1,  3 },
    { 1920, 0xA1,  3 },
    { 1792, 0xA1,  3 },
    { 1984, 0xA1,  3 },
    { 1792, 0xA1,  8 },
};
static const sfx_note_t kEnterPC[] = {
    { 1792, 0xF0, 6 },
    {    0, 0x00, 4 },
    { 1792, 0xF0, 6 },
    {    0, 0x00, 1 },
};
static const sfx_note_t kTurnOffPC[] = {
    { 1536, 0xF0, 4 },
    { 1024, 0xF0, 4 },
    {  512, 0xF0, 4 },
    {    0, 0x00, 1 },
};

static const sfx_note_t kWithdrawDeposit[] = {
    { 1280, 0xF2,  4 },
    { 1280, 0xE2,  4 },
    { 1792, 0xF2,  4 },
    { 1792, 0xE2, 15 },
};

static const sfx_note_t *pc_sfx_seq   = NULL;
static int               pc_sfx_count = 0;
static int               pc_sfx_step  = -1;
static int               pc_sfx_timer = 0;

static void pc_sfx_fire(int step) {
    const sfx_note_t *n = &pc_sfx_seq[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(0, 2, n->env_byte);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    pc_sfx_timer = sfx_cmd_len_frames(n->frames);
}

static void play_pc_sfx(const sfx_note_t *seq, int count) {
    cancel_pressab_sfx();
    Music_SuspendChannel(0);
    s_pc_sfx_mix_boost = 1;
    pc_sfx_seq   = seq;
    pc_sfx_count = count;
    pc_sfx_step  = 0;
    pc_sfx_fire(0);
}

void Audio_PlaySFX_StartMenu(void) {
    play_sfx_rom(SFX_START_MENU_1);
}

static const noise_note_t kTeleportEnter2[] = {
    { 0x32, 0xF1, 2 },
    { 0x00, 0x00, 2 },
    { 0x22, 0xF1, 2 },
    { 0x00, 0x00, 1 },
};

void Audio_PlaySFX_TeleportEnter2(void) {
    play_sfx_rom(SFX_TELEPORT_ENTER2_1);
}

void Audio_PlaySFX_TurnOnPC(void) {
    play_sfx_rom(SFX_TURN_ON_PC_1);
}

void Audio_PlaySFX_EnterPC(void) {
    play_sfx_rom(SFX_ENTER_PC_1);
}

void Audio_PlaySFX_TurnOffPC(void) {
    play_sfx_rom(SFX_TURN_OFF_PC_1);
}

void Audio_PlaySFX_WithdrawDeposit(void) {
    play_sfx_rom(SFX_WITHDRAW_DEPOSIT_1);
}

void Audio_PlaySFX_GoInside(void) {
    play_sfx_rom(SFX_GO_INSIDE_1);
}

void Audio_PlaySFX_GoOutside(void) {
    play_sfx_rom(SFX_GO_OUTSIDE_1);
}

static const noise_note_t kDamage[] = {
    { 0x44, 0xF4,  2 },
    { 0x14, 0xF4,  2 },
    { 0x32, 0xF1, 15 },
};
static const noise_note_t kSuperEffective[] = {
    { 0x34, 0xF1,  4 },
    { 0x64, 0xF2, 15 },
};
static const noise_note_t kNotVeryEffective[] = {
    { 0x55, 0x8F,  4 },
    { 0x44, 0xF4,  2 },
    { 0x22, 0xF4,  8 },
    { 0x21, 0xF2, 15 },
};

void Audio_PlaySFX_BattleHit(uint8_t dmg_mult) {
    if (dmg_mult == 0) return;
    if (dmg_mult >= 20)
        play_noise_sfx(kSuperEffective,   2);
    else if (dmg_mult <= 9)
        play_noise_sfx(kNotVeryEffective, 4);
    else
        play_noise_sfx(kDamage,           3);
}

typedef struct { uint16_t freq; uint8_t env; uint8_t frames; } silph_scope_note_t;
static const silph_scope_note_t kSilphScope[] = {
    { 1792, 0xD2,  0 },
    { 1856, 0xD2,  0 },
    { 1920, 0xD2,  0 },
    { 1984, 0xD2,  0 },
    { 2016, 0xE1, 10 },
    {    0, 0x00,  1 },
};
static int silph_scope_step = -1;
static int silph_scope_timer = 0;
static uint8_t silph_scope_music_suspended[4] = {0, 0, 0, 0};

static void silph_scope_fire(int step) {
    const silph_scope_note_t *n = &kSilphScope[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (0 << 6) | 0x3F);
    if (n->freq != 0) {
        Audio_WriteReg(0, 2, n->env);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    silph_scope_timer = sfx_cmd_len_frames(n->frames);
}

static void silph_scope_advance(void) {
    while (silph_scope_step >= 0) {
        if (silph_scope_timer > 0) return;
        silph_scope_step++;
        if (silph_scope_step >= (int)(sizeof(kSilphScope) / sizeof(kSilphScope[0]))) {
        silph_scope_step = -1;
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
        for (int c = 0; c < 4; c++) {
            if (silph_scope_music_suspended[c]) {
                Music_ResumeChannel(c);
                silph_scope_music_suspended[c] = 0;
            }
        }
        return;
    }
        silph_scope_fire(silph_scope_step);
        if (silph_scope_timer > 0) return;
    }
}

void Audio_PlaySFX_SilphScope(void) {

    AudioEngine_SetCryModifiers(0x00, 0x80);
    play_sfx_rom(SFX_SILPH_SCOPE);
}

int Audio_IsSFXPlaying_SilphScope(void) {
    return sfx_rom_playing(SFX_SILPH_SCOPE);
}

static const noise_note_t kBallPoof[] = {
    { 0x22, 0xA2, 15 },
};
static int ball_poof_timer = 0;

void Audio_PlaySFX_BallPoof(void) {
    play_sfx_rom(SFX_BALL_POOF);
}

static const noise_note_t kBattle24Noise[] = {
    { 0x22, 0x3F, 15 },
    { 0x21, 0xF2, 15 },
};
static int battle24_timer = 0;
void Audio_PlaySFX_Battle24(void) {
    play_sfx_rom(SFX_BATTLE_24);
}

static const noise_note_t kBattle28Noise[] = {
    { 73, 0xD1, 1 }, { 41, 0xD1, 1 },
    { 73, 0xD1, 1 }, { 41, 0xD1, 1 },
    { 73, 0xD1, 1 }, { 41, 0xD1, 1 },
    { 73, 0xD1, 1 }, { 41, 0xD1, 1 },
    { 73, 0xD1, 1 }, { 41, 0xD1, 1 },
    { 73, 0xD1, 1 }, { 41, 0xD1, 1 },
};
static int battle28_step = -1;
static int battle28_timer = 0;
static int battle28_active = 0;
static int battle28_music_suspended = 0;

static void battle28_ch1_fire(int step) {
    uint16_t freq = (step & 1) ? 1792u : 1984u;
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (0u << 6) | 0x3Fu);
    Audio_WriteReg(0, 2, 0xF1);
    Audio_WriteReg(0, 3, (uint8_t)(freq & 0xFFu));
    Audio_WriteReg(0, 4, (uint8_t)(((freq >> 8) & 0x07u) | 0x80u));
}

static void battle28_ch2_fire(int step) {
    static const uint8_t duty_pattern[4] = { 2u, 3u, 0u, 3u };
    uint16_t freq = (step & 1) ? 1793u : 1985u;
    uint8_t duty = duty_pattern[(uint8_t)step & 3u];
    Audio_WriteReg(1, 1, (uint8_t)((duty << 6) | 0x3Fu));
    Audio_WriteReg(1, 2, 0xE1);
    Audio_WriteReg(1, 3, (uint8_t)(freq & 0xFFu));
    Audio_WriteReg(1, 4, (uint8_t)(((freq >> 8) & 0x07u) | 0x80u));
}

void Audio_PlaySFX_Battle28(void) {
    play_sfx_rom(SFX_BATTLE_28);
}

typedef struct { uint8_t duty; uint16_t freq; uint8_t env; uint8_t frames; } battle29_sq_note_t;
static const battle29_sq_note_t kBattle29Ch1[] = {
    { 3, 288, 0xF3, 11 }, { 0, 336, 0xD3, 9 },
    { 2, 288, 0xF3, 11 }, { 1, 336, 0xD3, 9 },
    { 3, 288, 0xF3, 11 }, { 0, 336, 0xD3, 9 },
    { 2, 288, 0xF3, 11 }, { 1, 336, 0xD3, 9 },
    { 3, 288, 0xF3, 11 }, { 0, 336, 0xD3, 9 },
    { 2, 304, 0xE3, 8 },  { 1, 272, 0xC2, 15 },
};
static const noise_note_t kBattle29Noise[] = {
    { 53, 0xF3, 10 }, { 69, 0xF6, 14 },
    { 53, 0xF3, 10 }, { 69, 0xF6, 14 },
    { 53, 0xF3, 10 }, { 69, 0xF6, 14 },
    { 53, 0xF3, 10 }, { 69, 0xF6, 14 },
    { 188, 0xF4, 12 }, { 156, 0xF5, 12 }, { 172, 0xF4, 15 },
};
static int battle29_step = -1;
static int battle29_timer = 0;
static int battle29_active = 0;
static int battle29_music_suspended = 0;

static void battle29_ch1_fire(int step) {
    const battle29_sq_note_t *n = &kBattle29Ch1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (uint8_t)((n->duty << 6) | 0x3Fu));
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFFu));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07u) | 0x80u));
    battle29_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Battle29(void) {
    play_sfx_rom(SFX_BATTLE_29);
}

typedef struct { uint8_t duty; uint16_t freq; uint8_t env; uint8_t frames; } battle_sq_note_t;
static const battle_sq_note_t kBattle2ACh1[] = {
    { 0, 1536, 0xF4, 4 }, { 3, 1280, 0xC4, 3 }, { 2, 1536, 0xB5, 5 }, { 1, 1728, 0xE2, 13 },
    { 0, 1536, 0xF4, 4 }, { 3, 1280, 0xC4, 3 }, { 2, 1536, 0xB5, 5 }, { 1, 1728, 0xE2, 13 },
    { 0, 1536, 0xF4, 4 }, { 3, 1280, 0xC4, 3 }, { 2, 1536, 0xB5, 5 }, { 1, 1728, 0xE2, 13 },
    { 0, 1536, 0xD1, 8 },
};
static const battle_sq_note_t kBattle2ACh2[] = {
    { 2, 1504, 0xE4, 5 }, { 0, 1248, 0xB4, 4 }, { 3, 1512, 0xA5, 6 }, { 1, 1696, 0xD1, 14 },
    { 2, 1504, 0xE4, 5 }, { 0, 1248, 0xB4, 4 }, { 3, 1512, 0xA5, 6 }, { 1, 1696, 0xD1, 14 },
    { 2, 1504, 0xE4, 5 }, { 0, 1248, 0xB4, 4 }, { 3, 1512, 0xA5, 6 }, { 1, 1696, 0xD1, 14 },
};
static const noise_note_t kBattle2ANoise[] = {
    { 0x33, 0xC3, 5 }, { 0x43, 0x92, 3 }, { 0x33, 0xB5, 10 }, { 0x32, 0xC3, 15 },
    { 0x33, 0xC3, 5 }, { 0x43, 0x92, 3 }, { 0x33, 0xB5, 10 }, { 0x32, 0xC3, 15 },
};
static int battle2a_ch1_step = -1, battle2a_ch1_timer = 0;
static int battle2a_ch2_step = -1, battle2a_ch2_timer = 0;
static int battle2a_active = 0;
static int battle2a_music_suspended = 0;
static void battle2a_ch1_fire(int step) {
    const battle_sq_note_t *n = &kBattle2ACh1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (uint8_t)((n->duty << 6) | 0x3F));
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    battle2a_ch1_timer = sfx_cmd_len_frames(n->frames);
}
static void battle2a_ch2_fire(int step) {
    const battle_sq_note_t *n = &kBattle2ACh2[step];
    Audio_WriteReg(1, 1, (uint8_t)((n->duty << 6) | 0x3F));
    Audio_WriteReg(1, 2, n->env);
    Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    battle2a_ch2_timer = sfx_cmd_len_frames(n->frames);
}
void Audio_PlaySFX_Battle2A(void) {
    play_sfx_rom(SFX_BATTLE_2A);
}

static const noise_note_t kBattle0DNoise[] = {
    { 0x34, 0x8F, 15 }, { 0x35, 0xF2, 8 }, { 0x55, 0xF1, 10 },
};
void Audio_PlaySFX_Battle0D(void) {
    play_sfx_rom(SFX_BATTLE_0D);
}

static int faint_fall_only_timer = 0;
void Audio_PlaySFX_FaintFallOnly(void) {
    play_sfx_rom(SFX_FAINT_FALL);
}

static int ball_toss_ch1_timer = 0;
static int ball_toss_ch2_timer = 0;
static int ball_toss_active    = 0;

void Audio_PlaySFX_BallToss(void) {
    play_sfx_rom(SFX_BALL_TOSS);
}

typedef struct { uint8_t sweep; uint16_t freq; uint8_t env; uint8_t frames; } tink_sfx_note_t;
static const tink_sfx_note_t kTinkSFX[] = {
    { 0x3A, 512, 0xF2, 4 },
    { 0x22, 512, 0xE2, 8 },
};
#define TINK_SFX_NOTES 2
static int tink_sfx_step  = -1;
static int tink_sfx_timer = 0;

static void tink_sfx_fire(int step) {
    const tink_sfx_note_t *n = &kTinkSFX[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    tink_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Tink(void) {
    play_sfx_rom(SFX_TINK_1);
}

typedef struct { uint8_t sweep; uint16_t freq; uint8_t env; uint8_t frames; } poisoned_sfx_note_t;
static const poisoned_sfx_note_t kPoisonedSFX[] = {
    { 0x14, 1536, 0xF2, 4 },
    { 0x14, 1536, 0xF2, 4 },
    { 0x14, 1536, 0xF2, 4 },
    { 0x14, 1536, 0xF2, 4 },
    { 0x14, 1536, 0xF3, 15 },
};
#define POISONED_SFX_NOTES 5
static int poisoned_sfx_step  = -1;
static int poisoned_sfx_timer = 0;

static void poisoned_sfx_fire(int step) {
    const poisoned_sfx_note_t *n = &kPoisonedSFX[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (0 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    poisoned_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Poisoned(void) {
    play_sfx_rom(SFX_POISONED_1);
}

static const tink_sfx_note_t kShrinkSFX[] = {
    { 0x17, 1536, 0xD7, 15 },
    { 0x17, 1408, 0xB7, 15 },
    { 0x17, 1280, 0x87, 15 },
    { 0x17, 1152, 0x47, 15 },
    { 0x17, 1024, 0x17, 15 },
};
#define SHRINK_SFX_NOTES 5
static int shrink_sfx_step  = -1;
static int shrink_sfx_timer = 0;

static void shrink_sfx_fire(int step) {
    const tink_sfx_note_t *n = &kShrinkSFX[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (1 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    shrink_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Shrink(void) {
    play_sfx_rom(SFX_SHRINK_1);
}

#define SFX_POKEFLUTE_CH5_ADDR 0x6322
#define SFX_POKEFLUTE_CH6_ADDR 0x6325
#define SFX_POKEFLUTE_CH7_ADDR 0x449B

void Audio_PlayPokeFluteInBattle(void) {
    play_sfx_rom(SFX_CAUGHT_MON);
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN5, SFX_POKEFLUTE_CH5_ADDR);
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN6, SFX_POKEFLUTE_CH6_ADDR);
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN7, SFX_POKEFLUTE_CH7_ADDR);
}

int Audio_IsPokeFluteInBattlePlaying(void) {
    return AudioEngine_ChannelSoundId(AUDIO_CHAN7) != 0;
}

typedef struct { uint16_t freq; uint8_t env; uint8_t frames; } caught_sq_note_t;
static const caught_sq_note_t kCaughtMonCh1[] = {
    { 1650, 0xB2, 12 },
    { 1694, 0xB2, 12 },
    { 1732, 0xB2, 12 },
    { 1732, 0xB2,  6 },
    { 1732, 0xB2,  6 },
    { 1783, 0xB2, 12 },
    { 1812, 0xB2, 12 },
    { 1838, 0xB2, 12 },
    { 1838, 0xB2,  6 },
    { 1838, 0xB2,  6 },
    { 1849, 0xB5, 48 },
};
static const caught_sq_note_t kCaughtMonCh2[] = {
    { 1891, 0xC2, 12 },
    { 1891, 0xC2,  6 },
    { 1891, 0xC2,  6 },
    { 1849, 0xC2, 12 },
    { 1849, 0xC2,  6 },
    { 1849, 0xC2,  6 },
    { 1915, 0xC2, 12 },
    { 1915, 0xC2,  6 },
    { 1915, 0xC2,  6 },
    { 1899, 0xC2, 12 },
    { 1899, 0xC2,  6 },
    { 1899, 0xC2,  6 },
    { 1891, 0xC5, 48 },
};
#define CAUGHT_MON_CH1_NOTES 11
#define CAUGHT_MON_CH2_NOTES 13
static int caught_mon_ch1_step  = -1;
static int caught_mon_ch1_timer = 0;
static int caught_mon_ch2_step  = -1;
static int caught_mon_ch2_timer = 0;
static int caught_mon_active    = 0;

static void caught_mon_ch1_fire(int step) {
    const caught_sq_note_t *n = &kCaughtMonCh1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (3 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    caught_mon_ch1_timer = n->frames;
}

static void caught_mon_ch2_fire(int step) {
    const caught_sq_note_t *n = &kCaughtMonCh2[step];
    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(1, 2, n->env);
    Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    caught_mon_ch2_timer = n->frames;
}

void Audio_PlaySFX_CaughtMon(void) {
    play_sfx_rom(SFX_CAUGHT_MON);
}

static const noise_note_t kFaintThudNoise[] = {
    { 0x33, 0xF5,  4 },
    { 0x22, 0xF4,  8 },
    { 0x21, 0xF2, 15 },
};

typedef enum { FAINT_IDLE = 0, FAINT_FALL, FAINT_THUD } faint_sfx_state_t;
static faint_sfx_state_t faint_state = FAINT_IDLE;
static int               faint_timer = 0;

static const noise_note_t kRunSFX[] = {
    { 0x23, 0x61,  2 },
    { 0x33, 0xA1,  2 },
    { 0x33, 0xC1,  2 },
    { 0x11, 0x51,  2 },
    { 0x33, 0xF1,  2 },
    { 0x11, 0x41,  2 },
    { 0x33, 0xC1,  2 },
    { 0x11, 0x31,  2 },
    { 0x33, 0x81,  2 },
    { 0x11, 0x31,  2 },
    { 0x33, 0x41,  8 },
};

void Audio_PlaySFX_Run(void) {
    play_sfx_rom(SFX_RUN);
}

static const noise_note_t kCutSFX[] = {
    { 0x24, 0xF7,  2 },
    { 0x34, 0xF7,  2 },
    { 0x44, 0xF7,  4 },
    { 0x55, 0xF4,  8 },
    { 0x44, 0xF1,  8 },
};

void Audio_PlaySFX_Cut(void) {
    play_sfx_rom(SFX_CUT_1);
}

static const noise_note_t kPushBoulderSFX[] = {
    { 0x23, 0xA2,  4 },
    { 0x34, 0xF1,  8 },
    { 0x00, 0x00, 15 },
    { 0x24, 0xF7,  2 },
    { 0x34, 0xF7,  2 },
    { 0x44, 0xF7,  4 },
    { 0x55, 0xF4,  8 },
    { 0x44, 0xF1,  8 },
};

void Audio_PlaySFX_PushBoulder(void) {
    play_sfx_rom(SFX_PUSH_BOULDER_1);
}

static const sfx_note_t kTeleportEnter1SFX[] = {
    { 1792, 0xD7, 15 },
    { 1664, 0xB7, 15 },
    { 1536, 0x87, 15 },
    { 1408, 0x47, 15 },
    { 1280, 0x17, 15 },
};
static int teleport_enter1_step = -1;
static int teleport_enter1_timer = 0;
static int teleport_exit1_step = -1;
static int teleport_exit1_timer = 0;

static void teleport_enter1_fire(int step) {
    const sfx_note_t *n = &kTeleportEnter1SFX[step];
    Audio_WriteReg(0, 0, 0x17);
    Audio_WriteReg(0, 1, (1 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env_byte);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    teleport_enter1_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_TeleportEnter1(void) {
    play_sfx_rom(SFX_TELEPORT_ENTER1_1);
}

static const sfx_note_t kTeleportExit1SFX[] = {
    { 1280, 0xD7, 15 },
    { 1408, 0xB7, 15 },
    { 1536, 0x87, 15 },
    { 1664, 0x47, 15 },
    { 1792, 0x17, 15 },
};

static void teleport_exit1_fire(int step) {
    const sfx_note_t *n = &kTeleportExit1SFX[step];
    Audio_WriteReg(0, 0, 0x17);
    Audio_WriteReg(0, 1, (1 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env_byte);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    teleport_exit1_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_TeleportExit1(void) {
    play_sfx_rom(SFX_TELEPORT_EXIT1_1);
}

static const sfx_note_t kTeleportExit2SFX[] = {
    { 1280, 0xD2, 15 },
};
#define TELEPORT_EXIT2_NOTES 1
static int teleport_exit2_step  = -1;
static int teleport_exit2_timer =  0;

static void teleport_exit2_fire(int step) {
    const sfx_note_t *n = &kTeleportExit2SFX[step];
    Audio_WriteReg(0, 0, 0x16);
    Audio_WriteReg(0, 1, (1 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env_byte);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    teleport_exit2_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_TeleportExit2(void) {
    play_sfx_rom(SFX_TELEPORT_EXIT2_1);
}

typedef struct { uint16_t freq; uint8_t env; uint8_t frames; } sq_sfx_note_t;

static const sq_sfx_note_t kLvlUpCh1[] = {
      { 1861, 0xB4, 24 },
      { 1798, 0xB2,  8 },
           { 1861, 0xB2,  8 },
           { 1798, 0xB2,  8 },
      { 1838, 0xB3, 12 },
           { 1838, 0xB3, 12 },
           { 1850, 0xB3, 12 },
      { 1861, 0xB4, 48 },
};

static const sq_sfx_note_t kLvlUpCh2[] = {
      { 1899, 0xC4, 24 },
      { 1899, 0xC2,  8 },
           { 1899, 0xC2,  8 },
           { 1899, 0xC2,  8 },
      { 1907, 0xC4, 12 },
           { 1907, 0xC4, 12 },
           { 1907, 0xC4, 12 },
      { 1899, 0xC4, 48 },
};

#define LVL_UP_NOTES 8
static int lvl_up_step  = -1;
static int lvl_up_timer =  0;

static void lvl_up_fire(int step) {
    const sq_sfx_note_t *n1 = &kLvlUpCh1[step];
    const sq_sfx_note_t *n2 = &kLvlUpCh2[step];

    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n1->env);
    Audio_WriteReg(0, 3, (uint8_t)(n1->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n1->freq >> 8) & 0x07) | 0x80));

    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(1, 2, n2->env);
    Audio_WriteReg(1, 3, (uint8_t)(n2->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n2->freq >> 8) & 0x07) | 0x80));
    lvl_up_timer = n1->frames;
}

typedef struct { uint8_t sweep; uint16_t freq; uint8_t env; uint8_t frames; } heal_sfx_note_t;

static const heal_sfx_note_t kHealSFX[] = {
    { 0x2C, 1280, 0xF2, 4 },
    { 0x22, 1280, 0xF1, 2 },
    { 0x00,    0, 0x00, 1 },
};
#define HEAL_SFX_NOTES  3

static int heal_sfx_step  = -1;
static int heal_sfx_timer =  0;

static void heal_sfx_fire(int step) {
    const heal_sfx_note_t *n = &kHealSFX[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(0, 2, n->env);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    heal_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_HealingMachine(void) {
    play_sfx_rom(SFX_HEALING_MACHINE_1);
}

static const heal_sfx_note_t kHealHpSFX[] = {
    { 0x17, 1264, 0xF0, 15 },
    { 0x17, 1616, 0xF2, 15 },
    { 0x08,    0, 0x00,  0 },
};
#define HEAL_HP_SFX_NOTES 3
static int heal_hp_sfx_step  = -1;
static int heal_hp_sfx_timer =  0;

static void heal_hp_sfx_fire(int step) {
    const heal_sfx_note_t *n = &kHealHpSFX[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(0, 2, n->env);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    heal_hp_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_HealHP(void) {
    play_sfx_rom(SFX_HEAL_HP_1);
}

static const heal_sfx_note_t kHealAilmentSFX[] = {
    { 0x14, 1536, 0xF2,  4 },
    { 0x14, 1536, 0xF2,  4 },
    { 0x17, 1536, 0xF2, 15 },
    { 0x08,    0, 0x00,  0 },
};
#define HEAL_AILMENT_SFX_NOTES 4
static int heal_ailment_sfx_step  = -1;
static int heal_ailment_sfx_timer =  0;

static void heal_ailment_sfx_fire(int step) {
    const heal_sfx_note_t *n = &kHealAilmentSFX[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(0, 2, n->env);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    heal_ailment_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_HealAilment(void) {
    play_sfx_rom(SFX_HEAL_AILMENT_1);
}

typedef struct {
    uint16_t freq5; uint8_t env5;
    uint16_t freq6; uint8_t env6;
    uint8_t  frames;
} save_sfx_note_t;

static const save_sfx_note_t kSaveSFX[] = {

    { 1792, 0xF4,     0, 0x00,  4 },
    { 1536, 0xE4,  1793, 0xD4,  2 },
    { 1664, 0xE4,  1537, 0xC4,  2 },
    { 1728, 0xE4,  1665, 0xC4,  2 },
    { 1792, 0xE4,  1729, 0xC4,  2 },
    { 1952, 0xE4,  1793, 0xC4,  2 },
    { 2016, 0xF2,  2017, 0xD2, 15 },
};
#define SAVE_SFX_NOTES ((int)(sizeof(kSaveSFX) / sizeof(kSaveSFX[0])))
static int save_sfx_step  = -1;
static int save_sfx_timer =  0;

static void save_sfx_fire(int step) {
    const save_sfx_note_t *n = &kSaveSFX[step];

    Audio_WriteReg(0, 0, 0x00);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env5);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq5 & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq5 >> 8) & 0x07) | 0x80));

    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    if (n->env6) {
        Audio_WriteReg(1, 2, n->env6);
        Audio_WriteReg(1, 3, (uint8_t)(n->freq6 & 0xFF));
        Audio_WriteReg(1, 4, (uint8_t)(((n->freq6 >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(1, 2, 0x00);
        Audio_WriteReg(1, 4, 0x00);
    }
    save_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Save(void) {
    play_sfx_rom(SFX_SAVE_1);
}

void Audio_PlaySFX_LevelUp(void) {
    play_sfx_rom(SFX_LEVEL_UP);
}

static const sq_sfx_note_t kPurchaseCh1[] = {
    { 1792, 0xE1, 4 },
    { 2016, 0xF2, 8 },
};
static const sq_sfx_note_t kPurchaseCh2[] = {
    {    0, 0x08, 1 },
    { 1729, 0x91, 4 },
    { 1953, 0xA2, 8 },
};
#define PURCHASE_CH1_NOTES 2
#define PURCHASE_CH2_NOTES 3

static int purchase_ch1_step  = -1;
static int purchase_ch1_timer =  0;
static int purchase_ch2_step  = -1;
static int purchase_ch2_timer =  0;
static int purchase_active    =  0;

static void purchase_ch1_fire(int step) {
    const sq_sfx_note_t *n = &kPurchaseCh1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    purchase_ch1_timer = sfx_cmd_len_frames(n->frames);
}

static void purchase_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kPurchaseCh2[step];
    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    if (n->freq == 0) {

        Audio_WriteReg(1, 2, 0x00);
        Audio_WriteReg(1, 4, 0x00);
    } else {
        Audio_WriteReg(1, 2, n->env);
        Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    }
    purchase_ch2_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Purchase(void) {
    play_sfx_rom(SFX_PURCHASE_1);
}

static const sq_sfx_note_t kGetKeyCh1[] = {
    { 1767, 0xA4, 20 },
    { 1798, 0xB1, 10 },
    { 1798, 0xB1,  5 },
    { 1798, 0xB1,  5 },
    { 1838, 0xA4, 20 },
    { 1861, 0xB1, 10 },
    { 1861, 0xB1,  5 },
    { 1861, 0xB1,  5 },
    { 1908, 0xB4, 40 },
};
static const sq_sfx_note_t kGetKeyCh2[] = {
    { 1881, 0xD1, 10 },
    { 1881, 0xD1,  5 },
    { 1881, 0xD1,  5 },
    { 1838, 0xC4, 20 },
    { 1891, 0xD1, 10 },
    { 1891, 0xD1,  5 },
    { 1891, 0xD1,  5 },
    { 1908, 0xD1, 10 },
    { 1908, 0xD1,  5 },
    { 1908, 0xD1,  5 },
    { 1943, 0xC4, 40 },
};
#define GET_KEY_CH1_NOTES  9
#define GET_KEY_CH2_NOTES  11

static int get_key_ch1_step  = -1;
static int get_key_ch1_timer =  0;
static int get_key_ch2_step  = -1;
static int get_key_ch2_timer =  0;
static int get_key_active    =  0;

static void get_key_ch1_fire(int step) {
    const sq_sfx_note_t *n = &kGetKeyCh1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    get_key_ch1_timer = n->frames;
}

static void get_key_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kGetKeyCh2[step];
    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(1, 2, n->env);
    Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    get_key_ch2_timer = n->frames;
}

int Audio_IsSFXPlaying_GetKeyItem(void) { return sfx_rom_playing(SFX_GET_KEY_ITEM_1); }

static const sfx_note_t kSwitch[] = {
    {    0, 0x00, 4 },
    { 1664, 0xF1, 2 },
    {    0, 0x00, 1 },
    { 1920, 0xF1, 4 },
    {    0, 0x00, 4 },
};
#define SWITCH_SFX_NOTES 5
static int switch_sfx_step  = -1;
static int switch_sfx_timer =  0;

static void switch_sfx_fire(int step) {
    const sfx_note_t *n = &kSwitch[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(0, 2, n->env_byte);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    switch_sfx_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Switch(void) {
    play_sfx_rom(SFX_SWITCH_1);
}

static const sfx_note_t kSwapCh5[] = {
    { 1856, 0xE1, 8 },
};
static const sfx_note_t kSwapCh6[] = {
    {    0, 0x08, 2 },
    { 1857, 0xB1, 8 },
};
#define SWAP_CH5_NOTES 1
#define SWAP_CH6_NOTES 2
static int swap_ch5_step = -1, swap_ch5_timer = 0;
static int swap_ch6_step = -1, swap_ch6_timer = 0;

static void swap_ch_fire(int hw, const sfx_note_t *n) {
    if (hw == 0) Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(hw, 1, (2 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(hw, 2, n->env_byte);
        Audio_WriteReg(hw, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(hw, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(hw, 2, 0x00);
        Audio_WriteReg(hw, 4, 0x00);
    }
}

void Audio_PlaySFX_Swap(void) {
    play_sfx_rom(SFX_SWAP_1);
}

void Audio_PlaySFX_ArrowTiles(void) {
    play_sfx_rom(SFX_ARROW_TILES_1);
}

void Audio_PlaySFX_SafariZonePA(void) {
    play_sfx_rom(SFX_SAFARI_ZONE_PA);
}

void Audio_PlaySFX_TradeMachine(void) {
    play_sfx_rom(SFX_TRADE_MACHINE_1);
}

void Audio_PlaySFX_SlotsNewSpin(void) {
    play_sfx_rom(SFX_SLOTS_NEW_SPIN);
}

void Audio_PlaySFX_SlotsStopWheel(void) {
    play_sfx_rom(SFX_SLOTS_STOP_WHEEL);
}

void Audio_PlaySFX_SlotsReward(void) {
    play_sfx_rom(SFX_SLOTS_REWARD);
}

static const heal_sfx_note_t kDeniedCh1[] = {
    { 0x5A, 1280, 0xF0,  4 },
    { 0x08,    0, 0x00,  4 },
    { 0x08, 1280, 0xF0, 15 },
    { 0x08,    0, 0x00,  1 },
};
static const sq_sfx_note_t kDeniedCh2[] = {
    { 1025, 0xF0,  4 },
    {    0, 0x00,  4 },
    { 1025, 0xF0, 15 },
    {    0, 0x00,  1 },
};
#define DENIED_NOTES 4

static int denied_ch1_step  = -1;
static int denied_ch1_timer =  0;
static int denied_ch2_step  = -1;
static int denied_ch2_timer =  0;
static int denied_active    =  0;

static void denied_ch1_fire(int step) {
    const heal_sfx_note_t *n = &kDeniedCh1[step];
    Audio_WriteReg(0, 0, n->sweep);
    Audio_WriteReg(0, 1, (3 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(0, 2, n->env);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(0, 2, 0x00);
        Audio_WriteReg(0, 4, 0x00);
    }
    denied_ch1_timer = sfx_cmd_len_frames(n->frames);
}

static void denied_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kDeniedCh2[step];
    Audio_WriteReg(1, 1, (3 << 6) | 0x3F);
    if (n->freq > 0) {
        Audio_WriteReg(1, 2, n->env);
        Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {
        Audio_WriteReg(1, 2, 0x00);
        Audio_WriteReg(1, 4, 0x00);
    }
    denied_ch2_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_PlaySFX_Denied(void) {
    play_sfx_rom(SFX_DENIED_1);
}

typedef struct {
    uint8_t  duty;
    uint16_t freq;
    uint8_t  env_byte;
    uint8_t  frames;
} shooting_star_note_t;

static const shooting_star_note_t kShootingStarSFX[] = {
    { 3, 2016, 0x40,  4 },
    { 2, 2016, 0x60,  4 },
    { 1, 2016, 0x80,  4 },
    { 0, 2016, 0xA0,  8 },
    { 3, 2016, 0xA0,  8 },
    { 2, 2016, 0x80,  8 },
    { 1, 2016, 0x60,  8 },
    { 0, 2016, 0x30,  8 },
    { 3, 2016, 0x12, 15 },
};
#define SHOOTING_STAR_NOTES ((int)(sizeof(kShootingStarSFX) / sizeof(kShootingStarSFX[0])))

static int shooting_star_step  = -1;
static int shooting_star_timer = 0;

static void shooting_star_fire(int step) {
    const shooting_star_note_t *n = &kShootingStarSFX[step];
    Audio_WriteReg(0, 1, (uint8_t)((n->duty << 6) | 0x3F));
    Audio_WriteReg(0, 2, n->env_byte);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    shooting_star_timer = sfx_cmd_len_frames(n->frames);
}

void Audio_UseTitleScreenBank(void) {
    AudioEngine_SetEngine(2);
}

void Audio_PlaySFX_ShootingStar(void) {
    play_sfx_rom(SFX_SHOOTING_STAR);
}

static int intro_sq_timer = 0;

static void play_intro_sq(uint16_t freq) {
    Music_SuspendChannel(0);
    Audio_WriteReg(0, 0, 0x26);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, 0xC2);
    Audio_WriteReg(0, 3, (uint8_t)(freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((freq >> 8) & 0x07) | 0x80));
    intro_sq_timer = sfx_cmd_len_frames(12);
}

void Audio_PlaySFX_IntroHip(void) {
    play_sfx_rom(SFX_INTRO_HIP);
}
void Audio_PlaySFX_IntroHop(void) {
    play_sfx_rom(SFX_INTRO_HOP);
}

static const noise_note_t kIntroRaise[] = {
    { 0x21, 0x67,  2 },
    { 0x31, 0xA7,  2 },
    { 0x41, 0xF2, 15 },
};
static const noise_note_t kIntroCrash[] = {
    { 0x32, 0xD2,  2 },
    { 0x43, 0xF2, 15 },
};
static const noise_note_t kIntroLunge[] = {
    { 0x10, 0x20,  6 },
    { 0x40, 0x27,  6 },
    { 0x41, 0x47,  6 },
    { 0x41, 0x87,  6 },
    { 0x42, 0xC7,  6 },
    { 0x42, 0xD7,  8 },
    { 0x43, 0xE7, 15 },
    { 0x43, 0xF2, 15 },
};

void Audio_PlaySFX_IntroRaise(void) {
    play_sfx_rom(SFX_INTRO_RAISE);
}

void Audio_PlaySFX_IntroWhoosh(void) {
    play_sfx_rom(SFX_INTRO_WHOOSH);
}

void Audio_PlaySFX_IntroCrash(void) {
    play_sfx_rom(SFX_INTRO_CRASH);
}
void Audio_PlaySFX_IntroLunge(void) {
    play_sfx_rom(SFX_INTRO_LUNGE);
}

static int get_item1_active = 0;
static int get_item2_active = 0;

int Audio_IsSFXPlaying(void) {
    if (AudioEngine_IsSfxPlaying()) return 1;

    return sfx_step >= 0
        || pc_sfx_step >= 0
        || teleport_enter1_step >= 0
        || teleport_exit1_step >= 0
        || teleport_exit2_step >= 0
        || ledge_sfx_active
        || collision_sfx_active
        || noise_sfx_step >= 0
        || ball_toss_active
        || tink_sfx_step >= 0
        || shrink_sfx_step >= 0
        || caught_mon_active
        || lvl_up_step >= 0
        || heal_sfx_step >= 0
        || heal_hp_sfx_step >= 0
        || heal_ailment_sfx_step >= 0
        || save_sfx_step >= 0
        || purchase_active
        || get_key_active
        || get_item1_active
        || get_item2_active
        || switch_sfx_step >= 0
        || swap_ch5_step >= 0
        || swap_ch6_step >= 0
        || shooting_star_step >= 0
        || silph_scope_step >= 0
        || denied_active;
}

void Audio_PlaySFX_GetKeyItem(void) {
    play_sfx_rom(SFX_GET_KEY_ITEM_1);
}

static const sq_sfx_note_t kGetItem1Ch1[] = {
    { 1732, 0xB1,  8 },
    { 1732, 0xB1,  8 },
    { 1732, 0xB1,  8 },
    { 1849, 0xB3, 48 },
};
static const sq_sfx_note_t kGetItem1Ch2[] = {
    { 1849, 0xC1,  8 },
    { 1849, 0xC1,  8 },
    { 1849, 0xC1,  8 },
    { 1915, 0xC3, 48 },
};
#define GET_ITEM1_CH1_NOTES 4
#define GET_ITEM1_CH2_NOTES 4

static int get_item1_ch1_step  = -1;
static int get_item1_ch1_timer =  0;
static int get_item1_ch2_step  = -1;
static int get_item1_ch2_timer =  0;

static void get_item1_ch1_fire(int step) {
    const sq_sfx_note_t *n = &kGetItem1Ch1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    get_item1_ch1_timer = n->frames;
}

static void get_item1_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kGetItem1Ch2[step];
    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(1, 2, n->env);
    Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    get_item1_ch2_timer = n->frames;
}

void Audio_PlaySFX_GetItem1(void) {
    play_sfx_rom(SFX_GET_ITEM1_1);
}

static const sq_sfx_note_t kGetItem2Ch1[] = {
    { 1825, 0xB4, 20 },
    { 1798, 0xB4, 20 },
    { 1750, 0xB4, 40 },
    { 1838, 0xB2, 10 },
    { 1838, 0xB2, 10 },
    { 1825, 0xB2, 10 },
    { 1798, 0xB2, 10 },
    { 1798, 0xB2, 10 },
    { 1767, 0xB2, 10 },
    { 1798, 0xB4, 40 },
};
static const sq_sfx_note_t kGetItem2Ch2[] = {
    { 1899, 0xC5, 20 },
    { 1861, 0xC5, 20 },
    { 1798, 0xC5, 40 },
    { 1907, 0xC2, 10 },
    { 1907, 0xC2, 10 },
    { 1907, 0xC2, 10 },
    { 1881, 0xC2, 10 },
    { 1881, 0xC2, 10 },
    { 1907, 0xC2, 10 },
    { 1899, 0xC4, 40 },
};
#define GET_ITEM2_CH1_NOTES 10
#define GET_ITEM2_CH2_NOTES 10

static int get_item2_ch1_step  = -1;
static int get_item2_ch1_timer =  0;
static int get_item2_ch2_step  = -1;
static int get_item2_ch2_timer =  0;

static void get_item2_ch1_fire(int step) {
    const sq_sfx_note_t *n = &kGetItem2Ch1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    get_item2_ch1_timer = n->frames;
}

static void get_item2_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kGetItem2Ch2[step];
    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(1, 2, n->env);
    Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    get_item2_ch2_timer = n->frames;
}

void Audio_PlaySFX_GetItem2(void) {
    play_sfx_rom(SFX_GET_ITEM2_1);
}

static const sq_sfx_note_t kSSAnneHornCh1[] = {
    { 1280, 0xF0, 15 },
    {    0, 0x00,  4 },
    { 1280, 0xF0, 15 },
    { 1280, 0xF0, 15 },
    { 1280, 0xF0, 15 },
    { 1280, 0xF0, 15 },
    { 1280, 0xF0, 15 },
    { 1280, 0xF2, 15 },
};
static const sq_sfx_note_t kSSAnneHornCh2[] = {
    { 1154, 0xF0, 15 },
    {    0, 0x00,  4 },
    { 1154, 0xF0, 15 },
    { 1154, 0xF0, 15 },
    { 1154, 0xF0, 15 },
    { 1154, 0xF0, 15 },
    { 1154, 0xF0, 15 },
    { 1154, 0xF2, 15 },
};
#define HORN_CH_NOTES 8

static int horn_ch1_step  = -1;
static int horn_ch1_timer =  0;
static int horn_ch2_step  = -1;
static int horn_ch2_timer =  0;
static int horn_active    =  0;

static void horn_ch1_fire(int step) {
    const sq_sfx_note_t *n = &kSSAnneHornCh1[step];
    if (n->freq == 0) {
        Audio_WriteReg(0, 4, 0x00);
    } else {
        Audio_WriteReg(0, 0, 0x08);
        Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
        Audio_WriteReg(0, 2, n->env);
        Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    }
    horn_ch1_timer = sfx_cmd_len_frames(n->frames);
}

static void horn_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kSSAnneHornCh2[step];
    if (n->freq == 0) {
        Audio_WriteReg(1, 4, 0x00);
    } else {
        Audio_WriteReg(1, 1, (3 << 6) | 0x3F);
        Audio_WriteReg(1, 2, n->env);
        Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
        Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    }
    horn_ch2_timer = sfx_cmd_len_frames(n->frames);
}

int Audio_IsSFXPlaying_SSAnneHorn(void) { return sfx_rom_playing(SFX_SS_ANNE_HORN_1); }

void Audio_PlaySFX_SSAnneHorn(void) {
    play_sfx_rom(SFX_SS_ANNE_HORN_1);
}

static int faint_thud_pending = 0;

void Audio_PlaySFX_Faint(void) {

    AudioEngine_SetCryModifiers(0x00, 0x00);
    play_sfx_rom(SFX_FAINT_FALL);
    faint_thud_pending = 1;
}

static void faint_thud_tick(void) {
    if (!faint_thud_pending) return;

    if (AudioEngine_ChannelSoundId(AUDIO_CHAN5) == kSfxRom[SFX_FAINT_FALL].rom_id)
        return;
    play_sfx_rom(SFX_FAINT_THUD);
    faint_thud_pending = 0;
}

typedef struct {
    int     step;
    int     timer;
    uint8_t duty_state;
    uint8_t rotate_duty;
} cry_ch_t;

static cry_ch_t           cry_ch5   = { -1, 0, 0, 0 };
static cry_ch_t           cry_ch6   = { -1, 0, 0, 0 };
static cry_ch_t           cry_ch8_s = { -1, 0, 0, 0 };
static const cry_def_t   *cry_cur   = NULL;
static int8_t             cry_pitch = 0;
static uint16_t           cry_tempo = 256;

static int cry_frames(uint8_t len) {
    int f = (int)(((uint32_t)len * (uint32_t)cry_tempo) >> 8);
    return f > 0 ? f : 1;
}

static void cry_sq_fire(int hw_ch, cry_ch_t *st, const cry_sq_ch_t *def) {
    const cry_sq_note_t *n = &def->notes[st->step];
    int freq = (int)n->freq + (int)cry_pitch;
    if (freq < 0)    freq = 0;
    if (freq > 2047) freq = 2047;
    uint8_t duty    = (st->duty_state >> 6) & 3;
    uint8_t env     = (uint8_t)((n->vol << 4) | n->fade);
    uint8_t nrx1    = (uint8_t)((duty << 6) | 0x3F);
    if (hw_ch == 0) Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(hw_ch, 1, nrx1);
    Audio_WriteReg(hw_ch, 2, env);
    Audio_WriteReg(hw_ch, 3, (uint8_t)(freq & 0xFF));
    Audio_WriteReg(hw_ch, 4, (uint8_t)(((freq >> 8) & 0x07) | 0x80));
    st->timer = cry_frames(n->len);
}

static void cry_noise_fire(cry_ch_t *st, const cry_noise_ch_t *def) {
    const cry_noise_note_t *n = &def->notes[st->step];
    Audio_WriteReg(3, 2, (uint8_t)((n->vol << 4) | n->fade));
    Audio_WriteReg(3, 3, n->nr43);
    Audio_WriteReg(3, 4, 0x80);
    st->timer = cry_frames(n->len);
}

static int s_cry_style = AUDIO_CRIES_GEN1;

void Audio_SetCryStyle(int style) {
    s_cry_style = (style == AUDIO_CRIES_CRYSTAL) ? AUDIO_CRIES_CRYSTAL
                                                 : AUDIO_CRIES_GEN1;
}
int Audio_GetCryStyle(void) { return s_cry_style; }

static int crystal_cry_index(uint8_t species) {
    uint8_t dex = Species_Dex(species);
    if (dex >= 1 && dex <= 251) return (int)dex - 1;
    return -1;
}

static int use_crystal_cry(uint8_t species) {
    if (crystal_cry_index(species) < 0) return 0;
    if (Gen2Species_InternalToDex(species) >= 152) return 1;
    return s_cry_style == AUDIO_CRIES_CRYSTAL;
}

void Audio_PlayCryModified(uint8_t species, int8_t pitch_add, uint8_t tempo_add) {

    if (use_crystal_cry(species)) {
        JohtoAudio_PlayCryModified(crystal_cry_index(species), pitch_add, tempo_add);
        return;
    }
    if (species == 0 || species > NUM_POKEMON_CRIES) return;
    {
        const pokemon_cry_t *pc = &g_pokemon_cries[species - 1];

        AudioEngine_SetCryModifiers((uint8_t)(pc->pitch_mod + pitch_add),
                                    (uint8_t)(pc->tempo_mod + tempo_add));
        AudioEngine_PlayCry(species);
    }
}

void Audio_PlayCry(uint8_t species) {

    Audio_PlayCryModified(species, 0, 0);
}

int Audio_IsCryPlaying(void) {
    return AudioEngine_IsCryPlaying() || JohtoAudio_IsCryPlaying();
}

int Audio_StillSounding(void) {

    if (AudioEngine_LowHealthAlarmActive()) return 0;
    return Audio_IsSFXPlaying() || Audio_IsCryPlaying();
}

int Audio_IsMoveSFXPlaying(void) {
    return AudioEngine_IsSfxPlaying();
}

static const sq_sfx_note_t kDexRatingCh1[] = {
    { 1752, 0xB1, 10 },
    { 1752, 0xB1, 10 },
    { 1716, 0xB1, 10 },
    { 1716, 0xB1, 10 },
    { 1674, 0xB1, 10 },
    { 1652, 0xB1, 10 },
    { 1674, 0xB1, 10 },
    { 1752, 0xB1, 10 },
    { 1798, 0xB1, 20 },
    { 1850, 0xB1, 20 },
    { 1674, 0xB1, 20 },
};
static const sq_sfx_note_t kDexRatingCh2[] = {
    { 1954, 0xC2, 10 },
    { 1949, 0xC2,  5 },
    {    0, 0x00,  5 },
    { 1936, 0xC2, 10 },
    { 1923, 0xC2,  5 },
    {    0, 0x00,  5 },
    { 1908, 0xC2, 10 },
    { 1923, 0xC2, 10 },
    { 1936, 0xC2, 10 },
    { 1949, 0xC2, 10 },
    { 1954, 0xC2, 20 },
};
#define DEX_RATING_CH1_NOTES  11
#define DEX_RATING_CH2_NOTES  11

static int dex_rating_ch1_step  = -1;
static int dex_rating_ch1_timer =  0;
static int dex_rating_ch2_step  = -1;
static int dex_rating_ch2_timer =  0;
static int dex_rating_active    =  0;

static void dex_rating_ch1_fire(int step) {
    const sq_sfx_note_t *n = &kDexRatingCh1[step];
    Audio_WriteReg(0, 0, 0x08);
    Audio_WriteReg(0, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(0, 2, n->env);
    Audio_WriteReg(0, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(0, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    dex_rating_ch1_timer = n->frames;
}

static void dex_rating_ch2_fire(int step) {
    const sq_sfx_note_t *n = &kDexRatingCh2[step];
    Audio_WriteReg(1, 1, (2 << 6) | 0x3F);
    Audio_WriteReg(1, 2, n->env);
    Audio_WriteReg(1, 3, (uint8_t)(n->freq & 0xFF));
    Audio_WriteReg(1, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    dex_rating_ch2_timer = n->frames;
}

void Audio_PlaySFX_DexRating(void) {
    play_sfx_rom(SFX_POKEDEX_RATING_1);
}

#define LHA_TIMER_RELOAD 30
#define LHA_TONE_LO_AT   20

static int s_lha_on       = 0;
static int s_lha_disabled = 0;
static int s_lha_timer    = 0;

static const uint8_t kLhaToneHi[4]      = { 0xA0, 0xE2, 0x50, 0x87 };
static const uint8_t kLhaToneLo[4]      = { 0xB0, 0xE2, 0xEE, 0x86 };
static const uint8_t kLhaToneSilence[4] = { 0x00, 0x00, 0x00, 0x80 };

static void lha_write_tone(const uint8_t t[4]) {

    Audio_WriteReg(0, 0, 0x00);
    Audio_WriteReg(0, 1, t[0]);
    Audio_WriteReg(0, 2, t[1]);
    Audio_WriteReg(0, 3, t[2]);
    Audio_WriteReg(0, 4, t[3]);
}

void Audio_SetLowHealthAlarm(int on) {
    on = on && !s_lha_disabled;
    if (on == s_lha_on) return;
    s_lha_on = on;
    if (on) AudioEngine_LowHealthAlarmOn();
    else    AudioEngine_LowHealthAlarmOff();
}

void Audio_DisableLowHealthAlarm(void) {
    Audio_SetLowHealthAlarm(0);
    s_lha_disabled = 1;
}

void Audio_ResetLowHealthAlarm(void) {

    s_lha_disabled = 0;
    s_lha_on = 0;
    AudioEngine_LowHealthAlarmOff();
}

int Audio_IsLowHealthAlarmOn(void) { return s_lha_on; }

static void lha_tick(void) {
    if (!s_lha_on) return;
    if (s_lha_timer == 0) {
        lha_write_tone(kLhaToneHi);
        s_lha_timer = LHA_TIMER_RELOAD;
        return;
    }
    if (s_lha_timer == LHA_TONE_LO_AT)
        lha_write_tone(kLhaToneLo);
    s_lha_timer--;
}

void Audio_Update(void) {
    Audio_UpdateMusic();
    Audio_UpdateSfx();
}

static void vol_store(int *dst, int level) {

    if (level < 0) level = 0;
    if (level > AUDIO_VOL_MAX) level = AUDIO_VOL_MAX;
    if (audio_mutex) SDL_LockMutex(audio_mutex);
    *dst = level;
    s_user_master = (float)s_vol_master / (float)AUDIO_VOL_MAX;
    if (audio_mutex) SDL_UnlockMutex(audio_mutex);
}

void Audio_SetMasterVolume(int level) { vol_store(&s_vol_master, level); }
void Audio_SetMusicVolume(int level)  { vol_store(&s_vol_music,  level); }
void Audio_SetSfxVolume(int level)    { vol_store(&s_vol_sfx,    level); }

int Audio_GetMasterVolume(void) { return s_vol_master; }
int Audio_GetMusicVolume(void)  { return s_vol_music; }
int Audio_GetSfxVolume(void)    { return s_vol_sfx; }

void Audio_SetOutputMono(int enabled) {
    if (audio_mutex) SDL_LockMutex(audio_mutex);
    s_output_mono = enabled ? 1 : 0;
    if (audio_mutex) SDL_UnlockMutex(audio_mutex);
}

int Audio_GetOutputMono(void) { return s_output_mono; }

void Audio_SetFocusMuted(int muted) {
    if (audio_mutex) SDL_LockMutex(audio_mutex);
    s_focus_muted = muted ? 1 : 0;
    if (audio_mutex) SDL_UnlockMutex(audio_mutex);
}

int Audio_GetFocusMuted(void) { return s_focus_muted; }

void Audio_ApplyChannelVolumes(void) {
    AudioEngineState st;
    float music_g, sfx_g;

    AudioEngine_GetState(&st);
    music_g = (float)s_vol_music / (float)AUDIO_VOL_MAX;
    sfx_g   = (float)s_vol_sfx   / (float)AUDIO_VOL_MAX;

    if (audio_mutex) SDL_LockMutex(audio_mutex);
    for (int h = 0; h < 4; h++) {
        int sfx_owned = (st.sound_ids[h + 4] != 0) || sMoveSfx.ch[h].active;
        GbApu_SetChannelGain(h, sfx_owned ? sfx_g : music_g);
    }
    if (audio_mutex) SDL_UnlockMutex(audio_mutex);
}

void Audio_UpdateMusic(void) {

    s_apu_frame++;
    if (s_apu_log && s_apu_log_frames > 0) {
        AudioEngineState st; AudioEngine_GetState(&st);
        fprintf(s_apu_log, "%u ids %u,%u,%u,%u,%u,%u,%u,%u\n", s_apu_frame,
                st.sound_ids[0],st.sound_ids[1],st.sound_ids[2],st.sound_ids[3],
                st.sound_ids[4],st.sound_ids[5],st.sound_ids[6],st.sound_ids[7]);
        if (--s_apu_log_frames == 0) fprintf(s_apu_log, "=== end\n");
    }
    AudioEngine_Tick();
    Music_Update();
    JohtoMusic_Update();

    MapMusic_Tick();

    Audio_ApplyChannelVolumes();
}

void Audio_UpdateSfx(void) {
    faint_thud_tick();

    if (sMoveSfx.active) {
        uint8_t hw;
        uint8_t any_active = 0;
        for (hw = 0; hw < 4; hw++) {
            move_sfx_chan_rt_t *st = &sMoveSfx.ch[hw];
            if (!st->active) continue;
            any_active = 1;
            if (st->rotate_duty && st->timer > 0 && (hw == 0 || hw == 1)) {
                st->duty_state = (uint8_t)((st->duty_state << 2) | (st->duty_state >> 6));
                Audio_WriteReg(hw, 1, (uint8_t)((((st->duty_state >> 6) & 0x3) << 6) | 0x3F));
            }
            if (--st->timer <= 0) {
                move_sfx_advance_channel(hw);
            }
        }
        if (!any_active) {
            for (hw = 0; hw < 4; hw++) {
                if (sMoveSfx.suspended[hw]) {
                    Music_ResumeChannel(hw);
                    sMoveSfx.suspended[hw] = 0;
                }
            }
            sMoveSfx.active = 0;
        }
    }

    if (sfx_step >= 0) {
        if (--sfx_timer <= 0) {
            sfx_step++;
            int total = (int)(sizeof(kPressAB) / sizeof(kPressAB[0]));
            if (sfx_step >= total) {
                sfx_step = -1;
                Music_ResumeChannel(0);
            } else {
                sfx_fire(sfx_step);
            }
        }
    }

    if (collision_sfx_active) {
        if (--collision_sfx_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Music_ResumeChannel(0);
            collision_sfx_active = 0;
        }
    }

    if (ledge_sfx_active) {
        if (--ledge_sfx_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Music_ResumeChannel(0);
            ledge_sfx_active = 0;
        }
    }

    if (intro_sq_timer > 0) {
        if (--intro_sq_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Music_ResumeChannel(0);
        }
    }

    if (silph_scope_step >= 0) {
        if (--silph_scope_timer <= 0) {
            silph_scope_timer = 0;
            silph_scope_advance();
        }
    }

    if (shooting_star_step >= 0) {
        if (--shooting_star_timer <= 0) {
            shooting_star_step++;
            if (shooting_star_step >= SHOOTING_STAR_NOTES) {
                shooting_star_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                shooting_star_fire(shooting_star_step);
            }
        }
    }

    if (ball_poof_timer > 0) {
        if (--ball_poof_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Music_ResumeChannel(0);
        }
    }

    if (battle24_timer > 0) {
        if (--battle24_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Music_ResumeChannel(0);
        }
    }

    if (battle28_active) {
        if (--battle28_timer <= 0) {
            battle28_step++;
            if (battle28_step >= 24) {
                battle28_active = 0;
                battle28_step = -1;
                Audio_WriteReg(0, 2, 0x00);
                Audio_WriteReg(0, 4, 0x00);
                Audio_WriteReg(1, 2, 0x00);
                Audio_WriteReg(1, 4, 0x00);
                if (battle28_music_suspended) {
                    Music_ResumeChannel(0);
                    Music_ResumeChannel(1);
                    battle28_music_suspended = 0;
                }
            } else {
                battle28_ch1_fire(battle28_step);
                battle28_ch2_fire(battle28_step);
                battle28_timer = sfx_cmd_len_frames(1);
            }
        }
    }

    if (battle29_active) {
        if (--battle29_timer <= 0) {
            battle29_step++;
            if (battle29_step >= (int)(sizeof(kBattle29Ch1) / sizeof(kBattle29Ch1[0]))) {
                battle29_active = 0;
                battle29_step = -1;
                Audio_WriteReg(0, 2, 0x00);
                Audio_WriteReg(0, 4, 0x00);
                if (battle29_music_suspended) {
                    Music_ResumeChannel(0);
                    battle29_music_suspended = 0;
                }
            } else {
                battle29_ch1_fire(battle29_step);
            }
        }
    }

    if (battle2a_active) {
        if (battle2a_ch1_step >= 0 && --battle2a_ch1_timer <= 0) {
            battle2a_ch1_step++;
            if (battle2a_ch1_step >= (int)(sizeof(kBattle2ACh1) / sizeof(kBattle2ACh1[0]))) {
                battle2a_ch1_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Audio_WriteReg(0, 2, 0x00);
                Audio_WriteReg(0, 4, 0x00);
            } else {
                battle2a_ch1_fire(battle2a_ch1_step);
            }
        }
        if (battle2a_ch2_step >= 0 && --battle2a_ch2_timer <= 0) {
            battle2a_ch2_step++;
            if (battle2a_ch2_step >= (int)(sizeof(kBattle2ACh2) / sizeof(kBattle2ACh2[0]))) {
                battle2a_ch2_step = -1;
                Audio_WriteReg(1, 2, 0x00);
                Audio_WriteReg(1, 4, 0x00);
            } else {
                battle2a_ch2_fire(battle2a_ch2_step);
            }
        }
        if (battle2a_ch1_step < 0 && battle2a_ch2_step < 0) {
            battle2a_active = 0;
            if (battle2a_music_suspended) {
                Music_ResumeChannel(0);
                Music_ResumeChannel(1);
                battle2a_music_suspended = 0;
            }
        }
    }

    if (faint_fall_only_timer > 0) {
        if (--faint_fall_only_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Music_ResumeChannel(0);
        }
    }

    if (pc_sfx_step >= 0) {
        if (--pc_sfx_timer <= 0) {
            pc_sfx_step++;
            if (pc_sfx_step >= pc_sfx_count) {
                pc_sfx_step = -1;
                s_pc_sfx_mix_boost = 0;
                Music_ResumeChannel(0);
            } else {
                pc_sfx_fire(pc_sfx_step);
            }
        }
    }

    if (teleport_enter1_step >= 0) {
        if (--teleport_enter1_timer <= 0) {
            teleport_enter1_step++;
            if (teleport_enter1_step >= (int)(sizeof(kTeleportEnter1SFX) / sizeof(kTeleportEnter1SFX[0]))) {
                teleport_enter1_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                teleport_enter1_fire(teleport_enter1_step);
            }
        }
    }
    if (teleport_exit1_step >= 0) {
        if (--teleport_exit1_timer <= 0) {
            teleport_exit1_step++;
            if (teleport_exit1_step >= (int)(sizeof(kTeleportExit1SFX) / sizeof(kTeleportExit1SFX[0]))) {
                teleport_exit1_step = -1;

                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                teleport_exit1_fire(teleport_exit1_step);
            }
        }
    }

    if (teleport_exit2_step >= 0) {
        if (--teleport_exit2_timer <= 0) {
            teleport_exit2_step++;
            if (teleport_exit2_step >= TELEPORT_EXIT2_NOTES) {
                teleport_exit2_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                teleport_exit2_fire(teleport_exit2_step);
            }
        }
    }

    if (ball_toss_active) {
        if (ball_toss_ch1_timer > 0 && --ball_toss_ch1_timer <= 0) {
            Audio_WriteReg(0, 0, 0x08);
            Audio_WriteReg(0, 2, 0x00);
            Audio_WriteReg(0, 4, 0x00);
        }
        if (ball_toss_ch2_timer > 0 && --ball_toss_ch2_timer <= 0) {
            Audio_WriteReg(1, 2, 0x00);
            Audio_WriteReg(1, 4, 0x00);
        }
        if (ball_toss_ch1_timer <= 0 && ball_toss_ch2_timer <= 0) {
            ball_toss_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
            Music_ResumeChannel(2);
            Music_ResumeChannel(3);
        }
    }

    if (tink_sfx_step >= 0) {
        if (--tink_sfx_timer <= 0) {
            tink_sfx_step++;
            if (tink_sfx_step >= TINK_SFX_NOTES) {
                tink_sfx_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                tink_sfx_fire(tink_sfx_step);
            }
        }
    }

    if (poisoned_sfx_step >= 0) {
        if (--poisoned_sfx_timer <= 0) {
            poisoned_sfx_step++;
            if (poisoned_sfx_step >= POISONED_SFX_NOTES) {
                poisoned_sfx_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                poisoned_sfx_fire(poisoned_sfx_step);
            }
        }
    }

    if (shrink_sfx_step >= 0) {
        if (--shrink_sfx_timer <= 0) {
            shrink_sfx_step++;
            if (shrink_sfx_step >= SHRINK_SFX_NOTES) {
                shrink_sfx_step = -1;
                Audio_WriteReg(0, 0, 0x08);
                Music_ResumeChannel(0);
            } else {
                shrink_sfx_fire(shrink_sfx_step);
            }
        }
    }

    if (switch_sfx_step >= 0) {
        if (--switch_sfx_timer <= 0) {
            switch_sfx_step++;
            if (switch_sfx_step >= SWITCH_SFX_NOTES) {
                switch_sfx_step = -1;
                Music_ResumeChannel(0);
            } else {
                switch_sfx_fire(switch_sfx_step);
            }
        }
    }

    if (swap_ch5_step >= 0) {
        if (--swap_ch5_timer <= 0) {
            swap_ch5_step++;
            if (swap_ch5_step >= SWAP_CH5_NOTES) {
                swap_ch5_step = -1;
                Music_ResumeChannel(0);
            } else {
                swap_ch5_timer = sfx_cmd_len_frames(kSwapCh5[swap_ch5_step].frames);
                swap_ch_fire(0, &kSwapCh5[swap_ch5_step]);
            }
        }
    }
    if (swap_ch6_step >= 0) {
        if (--swap_ch6_timer <= 0) {
            swap_ch6_step++;
            if (swap_ch6_step >= SWAP_CH6_NOTES) {
                swap_ch6_step = -1;
                Music_ResumeChannel(1);
            } else {
                swap_ch6_timer = sfx_cmd_len_frames(kSwapCh6[swap_ch6_step].frames);
                swap_ch_fire(1, &kSwapCh6[swap_ch6_step]);
            }
        }
    }

    if (denied_active) {
        if (denied_ch1_step >= 0) {
            if (--denied_ch1_timer <= 0) {
                denied_ch1_step++;
                if (denied_ch1_step >= DENIED_NOTES)
                    denied_ch1_step = -1;
                else
                    denied_ch1_fire(denied_ch1_step);
            }
        }
        if (denied_ch2_step >= 0) {
            if (--denied_ch2_timer <= 0) {
                denied_ch2_step++;
                if (denied_ch2_step >= DENIED_NOTES)
                    denied_ch2_step = -1;
                else
                    denied_ch2_fire(denied_ch2_step);
            }
        }
        if (denied_ch1_step < 0 && denied_ch2_step < 0) {
            denied_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (noise_sfx_step >= 0) {
        if (--noise_sfx_timer <= 0) {
            noise_sfx_step++;
            if (noise_sfx_step >= noise_sfx_count) {
                noise_sfx_step = -1;
                if (noise_music_suspended) {
                    Music_ResumeChannel(3);
                    noise_music_suspended = 0;
                }
            } else {
                noise_sfx_fire(noise_sfx_step);
            }
        }
    }

    if (heal_sfx_step >= 0) {
        if (--heal_sfx_timer <= 0) {
            heal_sfx_step++;
            if (heal_sfx_step >= HEAL_SFX_NOTES) {
                heal_sfx_step = -1;
                Music_ResumeChannel(0);
            } else {
                heal_sfx_fire(heal_sfx_step);
            }
        }
    }

    if (heal_hp_sfx_step >= 0) {
        if (--heal_hp_sfx_timer <= 0) {
            heal_hp_sfx_step++;
            if (heal_hp_sfx_step >= HEAL_HP_SFX_NOTES) {
                heal_hp_sfx_step = -1;
                Music_ResumeChannel(0);
            } else {
                heal_hp_sfx_fire(heal_hp_sfx_step);
            }
        }
    }

    if (heal_ailment_sfx_step >= 0) {
        if (--heal_ailment_sfx_timer <= 0) {
            heal_ailment_sfx_step++;
            if (heal_ailment_sfx_step >= HEAL_AILMENT_SFX_NOTES) {
                heal_ailment_sfx_step = -1;
                Music_ResumeChannel(0);
            } else {
                heal_ailment_sfx_fire(heal_ailment_sfx_step);
            }
        }
    }

    if (save_sfx_step >= 0) {
        if (--save_sfx_timer <= 0) {
            save_sfx_step++;
            if (save_sfx_step >= SAVE_SFX_NOTES) {
                save_sfx_step = -1;
                Music_ResumeChannel(0);
                Music_ResumeChannel(1);
            } else {
                save_sfx_fire(save_sfx_step);
            }
        }
    }

    if (purchase_active) {
        if (purchase_ch1_step >= 0) {
            if (--purchase_ch1_timer <= 0) {
                purchase_ch1_step++;
                if (purchase_ch1_step >= PURCHASE_CH1_NOTES)
                    purchase_ch1_step = -1;
                else
                    purchase_ch1_fire(purchase_ch1_step);
            }
        }
        if (purchase_ch2_step >= 0) {
            if (--purchase_ch2_timer <= 0) {
                purchase_ch2_step++;
                if (purchase_ch2_step >= PURCHASE_CH2_NOTES)
                    purchase_ch2_step = -1;
                else
                    purchase_ch2_fire(purchase_ch2_step);
            }
        }
        if (purchase_ch1_step < 0 && purchase_ch2_step < 0) {
            purchase_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (get_key_active) {
        if (get_key_ch1_step >= 0) {
            if (--get_key_ch1_timer <= 0) {
                get_key_ch1_step++;
                if (get_key_ch1_step >= GET_KEY_CH1_NOTES)
                    get_key_ch1_step = -1;
                else
                    get_key_ch1_fire(get_key_ch1_step);
            }
        }
        if (get_key_ch2_step >= 0) {
            if (--get_key_ch2_timer <= 0) {
                get_key_ch2_step++;
                if (get_key_ch2_step >= GET_KEY_CH2_NOTES)
                    get_key_ch2_step = -1;
                else
                    get_key_ch2_fire(get_key_ch2_step);
            }
        }
        if (get_key_ch1_step < 0 && get_key_ch2_step < 0) {
            get_key_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (dex_rating_active) {
        if (dex_rating_ch1_step >= 0) {
            if (--dex_rating_ch1_timer <= 0) {
                dex_rating_ch1_step++;
                if (dex_rating_ch1_step >= DEX_RATING_CH1_NOTES)
                    dex_rating_ch1_step = -1;
                else
                    dex_rating_ch1_fire(dex_rating_ch1_step);
            }
        }
        if (dex_rating_ch2_step >= 0) {
            if (--dex_rating_ch2_timer <= 0) {
                dex_rating_ch2_step++;
                if (dex_rating_ch2_step >= DEX_RATING_CH2_NOTES)
                    dex_rating_ch2_step = -1;
                else
                    dex_rating_ch2_fire(dex_rating_ch2_step);
            }
        }
        if (dex_rating_ch1_step < 0 && dex_rating_ch2_step < 0) {
            dex_rating_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (get_item1_active) {
        if (get_item1_ch1_step >= 0) {
            if (--get_item1_ch1_timer <= 0) {
                get_item1_ch1_step++;
                if (get_item1_ch1_step >= GET_ITEM1_CH1_NOTES)
                    get_item1_ch1_step = -1;
                else
                    get_item1_ch1_fire(get_item1_ch1_step);
            }
        }
        if (get_item1_ch2_step >= 0) {
            if (--get_item1_ch2_timer <= 0) {
                get_item1_ch2_step++;
                if (get_item1_ch2_step >= GET_ITEM1_CH2_NOTES)
                    get_item1_ch2_step = -1;
                else
                    get_item1_ch2_fire(get_item1_ch2_step);
            }
        }
        if (get_item1_ch1_step < 0 && get_item1_ch2_step < 0) {
            get_item1_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (get_item2_active) {
        if (get_item2_ch1_step >= 0) {
            if (--get_item2_ch1_timer <= 0) {
                get_item2_ch1_step++;
                if (get_item2_ch1_step >= GET_ITEM2_CH1_NOTES)
                    get_item2_ch1_step = -1;
                else
                    get_item2_ch1_fire(get_item2_ch1_step);
            }
        }
        if (get_item2_ch2_step >= 0) {
            if (--get_item2_ch2_timer <= 0) {
                get_item2_ch2_step++;
                if (get_item2_ch2_step >= GET_ITEM2_CH2_NOTES)
                    get_item2_ch2_step = -1;
                else
                    get_item2_ch2_fire(get_item2_ch2_step);
            }
        }
        if (get_item2_ch1_step < 0 && get_item2_ch2_step < 0) {
            get_item2_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (caught_mon_active) {
        if (caught_mon_ch1_step >= 0) {
            if (--caught_mon_ch1_timer <= 0) {
                caught_mon_ch1_step++;
                if (caught_mon_ch1_step >= CAUGHT_MON_CH1_NOTES)
                    caught_mon_ch1_step = -1;
                else
                    caught_mon_ch1_fire(caught_mon_ch1_step);
            }
        }
        if (caught_mon_ch2_step >= 0) {
            if (--caught_mon_ch2_timer <= 0) {
                caught_mon_ch2_step++;
                if (caught_mon_ch2_step >= CAUGHT_MON_CH2_NOTES)
                    caught_mon_ch2_step = -1;
                else
                    caught_mon_ch2_fire(caught_mon_ch2_step);
            }
        }
        if (caught_mon_ch1_step < 0 && caught_mon_ch2_step < 0) {
            caught_mon_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
            Music_ResumeChannel(2);
        }
    }

    if (horn_active) {
        if (horn_ch1_step >= 0) {
            if (--horn_ch1_timer <= 0) {
                horn_ch1_step++;
                if (horn_ch1_step >= HORN_CH_NOTES) horn_ch1_step = -1;
                else horn_ch1_fire(horn_ch1_step);
            }
        }
        if (horn_ch2_step >= 0) {
            if (--horn_ch2_timer <= 0) {
                horn_ch2_step++;
                if (horn_ch2_step >= HORN_CH_NOTES) horn_ch2_step = -1;
                else horn_ch2_fire(horn_ch2_step);
            }
        }
        if (horn_ch1_step < 0 && horn_ch2_step < 0) {
            horn_active = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
        }
    }

    if (lvl_up_step >= 0) {
        if (--lvl_up_timer <= 0) {
            lvl_up_step++;
            if (lvl_up_step >= LVL_UP_NOTES) {
                lvl_up_step = -1;
                Music_ResumeChannel(0);
                Music_ResumeChannel(1);
            } else {
                lvl_up_fire(lvl_up_step);
            }
        }
    }

    if (faint_state == FAINT_FALL) {
        if (--faint_timer <= 0) {

            Audio_WriteReg(0, 0, 0x08);
            Audio_WriteReg(0, 1, (1 << 6) | 0x3F);
            Audio_WriteReg(0, 2, 0xD1);
            Audio_WriteReg(0, 3, (uint8_t)(512 & 0xFF));
            Audio_WriteReg(0, 4, (uint8_t)(((512 >> 8) & 0x07) | 0x80));

            play_noise_sfx(kFaintThudNoise, 3);
            faint_state = FAINT_THUD;
            faint_timer = 30;
        }
    } else if (faint_state == FAINT_THUD) {
        if (--faint_timer <= 0) {
            Music_ResumeChannel(0);
            faint_state = FAINT_IDLE;
        }
    }

    if (cry_cur != NULL) {

        if (cry_ch5.step >= 0) {
            if (cry_ch5.rotate_duty) {
                uint8_t s = cry_ch5.duty_state;
                s = (uint8_t)((s << 2) | (s >> 6));
                cry_ch5.duty_state = s;
                Audio_WriteReg(0, 1, (uint8_t)(((s >> 6) << 6) | 0x3F));
            }
            if (--cry_ch5.timer <= 0) {
                cry_ch5.step++;
                if (cry_ch5.step >= cry_cur->ch5.n_notes) {
                    cry_ch5.step = -1;
                } else {
                    cry_sq_fire(0, &cry_ch5, &cry_cur->ch5);
                }
            }
        }

        if (cry_ch6.step >= 0) {
            if (cry_ch6.rotate_duty) {
                uint8_t s = cry_ch6.duty_state;
                s = (uint8_t)((s << 2) | (s >> 6));
                cry_ch6.duty_state = s;
                Audio_WriteReg(1, 1, (uint8_t)(((s >> 6) << 6) | 0x3F));
            }
            if (--cry_ch6.timer <= 0) {
                cry_ch6.step++;
                if (cry_ch6.step >= cry_cur->ch6.n_notes) {
                    cry_ch6.step = -1;
                } else {
                    cry_sq_fire(1, &cry_ch6, &cry_cur->ch6);
                }
            }
        }

        if (cry_ch8_s.step >= 0) {
            if (--cry_ch8_s.timer <= 0) {
                cry_ch8_s.step++;
                if (cry_ch8_s.step >= cry_cur->ch8.n_notes) {
                    cry_ch8_s.step = -1;
                } else {
                    cry_noise_fire(&cry_ch8_s, &cry_cur->ch8);
                }
            }
        }

        if (cry_ch5.step < 0 && cry_ch6.step < 0 && cry_ch8_s.step < 0) {
            cry_cur = NULL;
            s_cry_mix_boost = 0;
            Music_ResumeChannel(0);
            Music_ResumeChannel(1);
            Music_ResumeChannel(2);
            Music_ResumeChannel(3);
        }
    }
}

void Audio_Quit(void) {
    if (audio_dev)   SDL_CloseAudioDevice(audio_dev);
    if (audio_mutex) SDL_DestroyMutex(audio_mutex);
}
