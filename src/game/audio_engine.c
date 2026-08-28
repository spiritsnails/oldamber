
#include "audio_engine.h"

#include <string.h>

#include "assetpack_bind.h"
#include "../data/cry_types.h"
#include "sfx_ids.h"

#define CHAN1 0
#define CHAN2 1
#define CHAN3 2
#define CHAN4 3
#define CHAN5 4
#define CHAN6 5
#define CHAN7 6
#define CHAN8 7
#define NUM_MUSIC_CHANS 4
#define NUM_CHANNELS    8

#define REG_DUTY_SOUND_LEN  1
#define REG_VOLUME_ENVELOPE 2
#define REG_FREQUENCY_LO    3

#define R_AUD1SWEEP 0x10
#define R_AUD3ENA   0x1A
#define R_AUDVOL    0x24
#define R_AUDTERM   0x25
#define R_AUDENA    0x26
#define R_WAVERAM   0x30
#define AUD3WAVE_SIZE 16

#define AUD3ENA_ON  0x80
#define AUDENA_ON   0x80
#define AUD1SWEEP_DOWN      0x08
#define AUD1HIGH_LENGTH_ON  0x40

#define BIT_PERFECT_PITCH          0
#define BIT_SOUND_CALL             1
#define BIT_NOISE_OR_SFX           2
#define BIT_VIBRATO_DIRECTION      3
#define BIT_PITCH_SLIDE_ON         4
#define BIT_PITCH_SLIDE_DECREASING 5
#define BIT_ROTATE_DUTY_CYCLE      6
#define BIT_EXECUTE_MUSIC          0
#define BIT_MUTE_AUDIO             7
#define BIT_LOW_HEALTH_ALARM       7

#define CMD_PITCH_SWEEP          0x10
#define CMD_SFX_NOTE             0x20
#define CMD_DRUM_NOTE            0xB0
#define CMD_REST                 0xC0
#define CMD_NOTE_TYPE            0xD0
#define CMD_OCTAVE               0xE0
#define CMD_TOGGLE_PERFECT_PITCH 0xE8
#define CMD_VIBRATO              0xEA
#define CMD_PITCH_SLIDE          0xEB
#define CMD_DUTY_CYCLE           0xEC
#define CMD_TEMPO                0xED
#define CMD_STEREO_PANNING       0xEE
#define CMD_UNKNOWNMUSIC0XEF     0xEF
#define CMD_VOLUME               0xF0
#define CMD_EXECUTE_MUSIC        0xF8
#define CMD_DUTY_CYCLE_PATTERN   0xFC
#define CMD_SOUND_CALL           0xFD
#define CMD_SOUND_LOOP           0xFE
#define CMD_SOUND_RET            0xFF

static const uint8_t kHWBase[NUM_CHANNELS] =
    { 0x10, 0x15, 0x1A, 0x1F, 0x10, 0x15, 0x1A, 0x1F };
static const uint8_t kHWEnable[NUM_CHANNELS] =
    { 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88 };
static const uint8_t kHWDisable[NUM_CHANNELS] =
    { 0xEE, 0xDD, 0xBB, 0x77, 0xEE, 0xDD, 0xBB, 0x77 };

#define META_PITCHES       0
#define META_WAVEPOINTERS  1
#define META_CRYRET        2

static uint16_t s_command_pointers[NUM_CHANNELS];
static uint16_t s_return_addresses[NUM_CHANNELS];
static uint8_t  s_sound_ids[NUM_CHANNELS];
static uint8_t  s_flags1[NUM_CHANNELS];
static uint8_t  s_flags2[NUM_CHANNELS];
static uint8_t  s_duty_cycles[NUM_CHANNELS];
static uint8_t  s_duty_cycle_patterns[NUM_CHANNELS];
static uint8_t  s_vibrato_delay_counters[NUM_CHANNELS];
static uint8_t  s_vibrato_delay_reload[NUM_CHANNELS];
static uint8_t  s_vibrato_extents[NUM_CHANNELS];
static uint8_t  s_vibrato_rates[NUM_CHANNELS];
static uint8_t  s_frequency_low_bytes[NUM_CHANNELS];
static uint8_t  s_note_delay_counters[NUM_CHANNELS];
static uint8_t  s_note_delay_frac[NUM_CHANNELS];
static uint8_t  s_loop_counters[NUM_CHANNELS];
static uint8_t  s_note_speeds[NUM_CHANNELS];
static uint8_t  s_octaves[NUM_CHANNELS];
static uint8_t  s_volumes[NUM_CHANNELS];
static uint8_t  s_ps_length_modifiers[NUM_CHANNELS];
static uint8_t  s_ps_freq_steps[NUM_CHANNELS];
static uint8_t  s_ps_freq_steps_frac[NUM_CHANNELS];
static uint8_t  s_ps_cur_freq_frac[NUM_CHANNELS];
static uint8_t  s_ps_cur_freq_hi[NUM_CHANNELS];
static uint8_t  s_ps_cur_freq_lo[NUM_CHANNELS];
static uint8_t  s_ps_target_hi[NUM_CHANNELS];
static uint8_t  s_ps_target_lo[NUM_CHANNELS];

static uint16_t s_music_tempo;
static uint16_t s_sfx_tempo;
static uint8_t  s_stereo_panning;
static uint8_t  s_sound_id;
static uint8_t  s_saved_volume;
static uint8_t  s_disable_output_when_sfx_ends;
static uint8_t  s_mute_audio_and_pause_music;
static uint8_t  s_music_wave_instrument;
static uint8_t  s_sfx_wave_instrument;
static uint8_t  s_frequency_modifier;
static uint8_t  s_tempo_modifier;
static uint8_t  s_low_health_alarm;
static uint8_t  s_unused_music_byte;
static uint16_t s_sfx_header_pointer;

static int s_engine;
static uint8_t s_apu[0x40];

static const uint8_t *bank_image(void) {
    switch (s_engine) {
        case 0:  return gAudioBank1;
        case 1:  return gAudioBank2;
        default: return gAudioBank3;
    }
}

int AudioEngine_IsReady(void) {
    return gAudioBank1 && gAudioBank2 && gAudioBank3 && gAudioBankMeta;
}

static uint8_t bank_read(uint16_t addr) {
    if (addr < 0x4000) return CMD_SOUND_RET;
    return bank_image()[addr - 0x4000];
}

static uint16_t meta(int field) {
    int i = (s_engine * 3 + field) * 2;
    return (uint16_t)(gAudioBankMeta[i] | (gAudioBankMeta[i + 1] << 8));
}

static const uint8_t kApuReadOrMask[0x40] = {
    [0x10] = 0x80,
    [0x11] = 0x3F,
    [0x12] = 0x00,
    [0x13] = 0xFF,
    [0x14] = 0xBF,
    [0x15] = 0xFF,
    [0x16] = 0x3F,
    [0x17] = 0x00,
    [0x18] = 0xFF,
    [0x19] = 0xBF,
    [0x1A] = 0x7F,
    [0x1B] = 0xFF,
    [0x1C] = 0x9F,
    [0x1D] = 0xFF,
    [0x1E] = 0xBF,
    [0x1F] = 0xFF,
    [0x20] = 0xFF,
    [0x21] = 0x00,
    [0x22] = 0x00,
    [0x23] = 0xBF,
    [0x24] = 0x00,
    [0x25] = 0x00,
    [0x26] = 0x70,
};

static void (*s_write_hook)(uint8_t, uint8_t);

void AudioEngine_SetWriteHook(void (*fn)(uint8_t, uint8_t)) { s_write_hook = fn; }

static void apu_w(uint8_t lo, uint8_t v) {
    s_apu[lo] = v;
    if (s_write_hook) s_write_hook(lo, v);
}

static uint8_t apu_r(uint8_t lo) {
    return (uint8_t)(s_apu[lo] | kApuReadOrMask[lo & 0x3F]);
}

static uint8_t reg_addr(int c, int reg) {
    return (uint8_t)(kHWBase[c] + reg);
}

const uint8_t *AudioEngine_ApuRegs(void) { return s_apu; }

void AudioEngine_GetState(AudioEngineState *out) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        out->sound_ids[i]           = s_sound_ids[i];
        out->command_pointers[i]    = s_command_pointers[i];
        out->note_delay_counters[i] = s_note_delay_counters[i];
        out->flags1[i]              = s_flags1[i];
    }
}

int  AudioEngine_GetEngine(void) { return s_engine; }
void AudioEngine_SetEngine(int e) { if (e >= 0 && e <= 2) s_engine = e; }

static uint8_t get_next_music_byte(int c) {
    uint8_t v = bank_read(s_command_pointers[c]);
    s_command_pointers[c]++;
    return v;
}

static uint16_t multiply_add(uint8_t l_in, uint8_t a, uint16_t de) {
    uint16_t hl = l_in;
    for (;;) {
        int carry = a & 1;
        a >>= 1;
        if (carry) hl = (uint16_t)(hl + de);
        de = (uint16_t)(de << 1);
        if (a == 0) break;
    }
    return hl;
}

static uint16_t calculate_frequency(uint8_t note, uint8_t b) {
    uint16_t tbl = meta(META_PITCHES);
    uint8_t e = bank_read((uint16_t)(tbl + note * 2));
    uint8_t d = bank_read((uint16_t)(tbl + note * 2 + 1));
    uint8_t a = b;
    int guard = 0;
    while (a != 7 && guard++ < 256) {
        uint8_t carry = d & 1;
        d = (uint8_t)((d >> 1) | (d & 0x80));
        e = (uint8_t)((e >> 1) | (carry << 7));
        a++;
    }
    d = (uint8_t)(d + 8);
    return (uint16_t)((d << 8) | e);
}

static int is_cry(void) {
    uint8_t a = s_sound_ids[CHAN5];
    if (a < CRY_SFX_START) return 0;
    if (a == CRY_SFX_END) return 0;
    return a < CRY_SFX_END;
}

int AudioEngine_IsCryPlaying(void) {
    if (!AudioEngine_IsReady()) return 0;
    return is_cry();
}

static int is_battle_sfx(void) {
    uint8_t a = (uint8_t)(s_sound_ids[CHAN5] | s_sound_ids[CHAN8]);
    if (a < BATTLE_SFX_START) return 0;
    if (a == BATTLE_SFX_END) return 0;
    return a < BATTLE_SFX_END;
}

int AudioEngine_IsSoundPlaying(uint8_t rom_id) {
    if (!AudioEngine_IsReady() || rom_id == 0) return 0;
    for (int i = CHAN5; i < NUM_CHANNELS; i++)
        if (s_sound_ids[i] == rom_id) return 1;
    return 0;
}

int AudioEngine_IsMusicPlaying(void) {
    if (!AudioEngine_IsReady()) return 0;
    for (int i = CHAN1; i < NUM_MUSIC_CHANS; i++)
        if (s_sound_ids[i] != 0) return 1;
    return 0;
}

uint8_t AudioEngine_ChannelSoundId(int chan) {
    if (!AudioEngine_IsReady() || chan < 0 || chan >= NUM_CHANNELS) return 0;
    return s_sound_ids[chan];
}

int AudioEngine_IsSfxPlaying(void) {
    if (!AudioEngine_IsReady()) return 0;
    if (s_low_health_alarm & (1 << BIT_LOW_HEALTH_ALARM)) return 0;
    return (s_sound_ids[CHAN5] | s_sound_ids[CHAN6] | s_sound_ids[CHAN8]) != 0;
}

static int cry_modifiers_apply(void) {
    if (is_cry()) return 1;
    if (s_engine == 1 && is_battle_sfx()) return 1;
    return 0;
}

static void apply_frequency_modifier(int c, uint16_t de) {
    if (!cry_modifiers_apply()) return;
    uint8_t d = (uint8_t)(de >> 8), e = (uint8_t)de;
    unsigned sum = (unsigned)s_frequency_modifier + e;
    e = (uint8_t)sum;
    if (sum > 0xFF) d++;
    apu_w(reg_addr(c, REG_FREQUENCY_LO), e);
    apu_w((uint8_t)(reg_addr(c, REG_FREQUENCY_LO) + 1), d);
}

static void set_sfx_tempo(void) {
    if (cry_modifiers_apply()) {
        unsigned v = (unsigned)s_tempo_modifier + 0x80;
        s_sfx_tempo = (uint16_t)(((v > 0xFF ? 1u : 0u) << 8) | (v & 0xFF));
    } else {
        s_sfx_tempo = 0x0100;
    }
}

static int go_back_one_command_if_cry(int c) {
    if (!is_cry()) return 0;
    s_command_pointers[c]--;
    return 1;
}

static void apply_duty_cycle_pattern(int c) {
    uint8_t p = s_duty_cycle_patterns[c];
    p = (uint8_t)((p << 2) | (p >> 6));
    s_duty_cycle_patterns[c] = p;
    uint8_t d = (uint8_t)(p & 0xC0);
    uint8_t a = reg_addr(c, REG_DUTY_SOUND_LEN);
    apu_w(a, (uint8_t)((apu_r(a) & 0x3F) | d));
}

static void apply_pitch_slide(int c) {
    uint8_t d, e;
    if (!(s_flags1[c] & (1 << BIT_PITCH_SLIDE_DECREASING))) {

        e = s_ps_cur_freq_lo[c];
        d = s_ps_cur_freq_hi[c];
        uint16_t de = (uint16_t)((d << 8) | e);
        de = (uint16_t)(de + s_ps_freq_steps[c]);
        d = (uint8_t)(de >> 8);
        e = (uint8_t)de;

        unsigned sum = (unsigned)s_ps_cur_freq_frac[c] + s_ps_freq_steps_frac[c];
        s_ps_cur_freq_frac[c] = (uint8_t)sum;
        unsigned carry = sum > 0xFF;
        unsigned te = e + carry;
        e = (uint8_t)te;
        d = (uint8_t)(d + (te > 0xFF));

        if (s_ps_target_hi[c] < d) goto reached;
        if (s_ps_target_hi[c] == d && s_ps_target_lo[c] < e) goto reached;
    } else {

        uint8_t a = s_ps_cur_freq_lo[c];
        d = s_ps_cur_freq_hi[c];
        e = s_ps_freq_steps[c];
        unsigned diff = (unsigned)a - e;
        e = (uint8_t)diff;
        unsigned borrow = (diff > 0xFF);
        d = (uint8_t)(d - borrow);

        unsigned f = (unsigned)s_ps_freq_steps_frac[c] * 2;
        s_ps_freq_steps_frac[c] = (uint8_t)f;
        unsigned c2 = f > 0xFF;
        unsigned te = (unsigned)e - c2;
        e = (uint8_t)te;
        d = (uint8_t)(d - (te > 0xFF));

        if (d < s_ps_target_hi[c]) goto reached;
        if (d == s_ps_target_hi[c] && e < s_ps_target_lo[c]) goto reached;
    }

    s_ps_cur_freq_lo[c] = e;
    s_ps_cur_freq_hi[c] = d;
    apu_w(reg_addr(c, REG_FREQUENCY_LO), e);
    apu_w((uint8_t)(reg_addr(c, REG_FREQUENCY_LO) + 1), d);
    return;

reached:
    s_flags1[c] &= (uint8_t)~(1 << BIT_PITCH_SLIDE_ON);
    s_flags1[c] &= (uint8_t)~(1 << BIT_PITCH_SLIDE_DECREASING);
}

static uint16_t init_pitch_slide_vars(int c, uint16_t de) {
    uint8_t d = (uint8_t)(de >> 8), e = (uint8_t)de;
    s_ps_cur_freq_hi[c] = d;
    s_ps_cur_freq_lo[c] = e;

    unsigned a = s_note_delay_counters[c];
    unsigned sub = (unsigned)a - s_ps_length_modifiers[c];
    if (sub > 0xFF) sub = 1;
    s_ps_length_modifiers[c] = (uint8_t)sub;

    unsigned le = (unsigned)e - s_ps_target_lo[c];
    uint8_t ne = (uint8_t)le;
    unsigned borrow = le > 0xFF;
    unsigned hd = (unsigned)d - borrow;
    unsigned hi = hd - s_ps_target_hi[c];

    if (hi <= 0xFF) {
        d = (uint8_t)hi;
        e = ne;
        s_flags1[c] |= (1 << BIT_PITCH_SLIDE_DECREASING);
    } else {

        d = s_ps_cur_freq_hi[c];
        e = s_ps_cur_freq_lo[c];
        unsigned l2 = (unsigned)s_ps_target_lo[c] - e;
        e = (uint8_t)l2;
        d = (uint8_t)(d - (l2 > 0xFF));
        d = (uint8_t)(s_ps_target_hi[c] - d);
        s_flags1[c] &= (uint8_t)~(1 << BIT_PITCH_SLIDE_DECREASING);
    }

    uint8_t divisor = s_ps_length_modifiers[c];
    unsigned b = 0;
    for (;;) {
        b = (b + 1) & 0xFF;
        unsigned t = (unsigned)e - divisor;
        e = (uint8_t)t;
        if (t <= 0xFF) continue;
        if (d == 0) break;
        d--;
    }
    uint8_t rem = (uint8_t)(e + divisor);
    s_ps_freq_steps[c]      = (uint8_t)b;
    s_ps_freq_steps_frac[c] = rem;
    s_ps_cur_freq_frac[c]   = rem;
    return (uint16_t)((b << 8) | e);
}

static void enable_channel_output(int c) {
    uint8_t d = (uint8_t)(apu_r(R_AUDTERM) | kHWEnable[c]);
    int apply_panning;
    if (c == CHAN8) {
        apply_panning = 1;
    } else if (c >= CHAN5) {
        apply_panning = 0;
    } else {
        apply_panning = (s_sound_ids[CHAN5 + c] == 0);
    }
    if (apply_panning) {
        uint8_t pan = (uint8_t)(s_stereo_panning & kHWEnable[c]);
        d = (uint8_t)((apu_r(R_AUDTERM) & kHWDisable[c]) | pan);
    }
    apu_w(R_AUDTERM, d);
}

static void apply_duty_cycle_and_sound_length(int c) {
    uint8_t d = s_note_delay_counters[c];
    if (c != CHAN3 && c != CHAN7) {
        d = (uint8_t)((d & 0x3F) | s_duty_cycles[c]);
    }
    apu_w(reg_addr(c, REG_DUTY_SOUND_LEN), d);
}

static void apply_wave_pattern_and_frequency(int c, uint16_t de) {
    if (c == CHAN3 || c == CHAN7) {

        uint8_t inst = (c == CHAN3) ? s_music_wave_instrument : s_sfx_wave_instrument;
        uint16_t tbl = meta(META_WAVEPOINTERS);
        uint16_t src = (uint16_t)(bank_read((uint16_t)(tbl + inst * 2)) |
                                  (bank_read((uint16_t)(tbl + inst * 2 + 1)) << 8));
        apu_w(R_AUD3ENA, 0);
        for (int i = 0; i < AUD3WAVE_SIZE; i++)
            apu_w((uint8_t)(R_WAVERAM + i), bank_read((uint16_t)(src + i)));
        apu_w(R_AUD3ENA, AUD3ENA_ON);
    }
    uint8_t d = (uint8_t)(de >> 8), e = (uint8_t)de;

    d = (uint8_t)((d | 0x80) & 0xC7);
    apu_w(reg_addr(c, REG_FREQUENCY_LO), e);
    apu_w((uint8_t)(reg_addr(c, REG_FREQUENCY_LO) + 1), d);

    if (s_engine == 1 && c < CHAN5) return;
    apply_frequency_modifier(c, (uint16_t)((d << 8) | e));
}

static void note_length_and_pitch(int c, uint8_t d);

static void play_next_note_commands(int c) {
    for (;;) {
        uint8_t d = get_next_music_byte(c);

        if (d == CMD_SOUND_RET) {
            if (s_flags1[c] & (1 << BIT_SOUND_CALL)) {
                s_flags1[c] &= (uint8_t)~(1 << BIT_SOUND_CALL);
                s_command_pointers[c] = s_return_addresses[c];
                continue;
            }
            int disable_output = 0;
            if (c < CHAN4) {
                disable_output = 1;
            } else {
                s_flags1[c] &= (uint8_t)~(1 << BIT_NOISE_OR_SFX);
                s_flags2[c] &= (uint8_t)~(1 << BIT_EXECUTE_MUSIC);
                if (c == CHAN7) {

                    apu_w(R_AUD3ENA, 0);
                    apu_w(R_AUD3ENA, AUD3ENA_ON);
                } else if (s_disable_output_when_sfx_ends) {
                    s_disable_output_when_sfx_ends = 0;
                    disable_output = 1;
                }
            }
            if (disable_output)
                apu_w(R_AUDTERM, (uint8_t)(apu_r(R_AUDTERM) & kHWDisable[c]));

            if (is_cry()) {
                if (c != CHAN5 && go_back_one_command_if_cry(c)) return;
                apu_w(R_AUDVOL, s_saved_volume);
                s_saved_volume = 0;
            }
            s_sound_ids[c] = 0;
            return;
        }

        if (d == CMD_SOUND_CALL) {
            uint8_t lo = get_next_music_byte(c);
            uint8_t hi = get_next_music_byte(c);
            s_return_addresses[c] = s_command_pointers[c];
            s_command_pointers[c] = (uint16_t)((hi << 8) | lo);
            s_flags1[c] |= (1 << BIT_SOUND_CALL);
            continue;
        }

        if (d == CMD_SOUND_LOOP) {
            uint8_t count = get_next_music_byte(c);
            if (count != 0) {
                if (s_loop_counters[c] == count) {
                    s_loop_counters[c] = 1;
                    get_next_music_byte(c);
                    get_next_music_byte(c);
                    continue;
                }
                s_loop_counters[c]++;
            }
            uint8_t lo = get_next_music_byte(c);
            uint8_t hi = get_next_music_byte(c);
            s_command_pointers[c] = (uint16_t)((hi << 8) | lo);
            continue;
        }

        if ((d & 0xF0) == CMD_NOTE_TYPE) {
            s_note_speeds[c] = (uint8_t)(d & 0x0F);
            if (c == CHAN4) continue;
            uint8_t p = get_next_music_byte(c);
            if (c == CHAN3 || c == CHAN7) {
                if (c == CHAN3) s_music_wave_instrument = (uint8_t)(p & 0x0F);
                else            s_sfx_wave_instrument   = (uint8_t)(p & 0x0F);
                p = (uint8_t)((p & 0x30) << 1);
            }
            s_volumes[c] = p;
            continue;
        }

        if (d == CMD_TOGGLE_PERFECT_PITCH) {
            s_flags1[c] ^= (1 << BIT_PERFECT_PITCH);
            continue;
        }

        if (d == CMD_VIBRATO) {
            uint8_t delay = get_next_music_byte(c);
            s_vibrato_delay_counters[c] = delay;
            s_vibrato_delay_reload[c]   = delay;
            uint8_t p = get_next_music_byte(c);

            uint8_t n = (uint8_t)(p >> 4);
            uint8_t half = (uint8_t)(n >> 1);
            uint8_t upper = (uint8_t)(half + (n & 1));
            s_vibrato_extents[c] = (uint8_t)((upper << 4) | half);

            uint8_t rate = (uint8_t)(p & 0x0F);
            s_vibrato_rates[c] = (uint8_t)((rate << 4) | rate);
            continue;
        }

        if (d == CMD_PITCH_SLIDE) {
            s_ps_length_modifiers[c] = get_next_music_byte(c);
            uint8_t p = get_next_music_byte(c);
            uint16_t de = calculate_frequency((uint8_t)(p & 0x0F), (uint8_t)(p >> 4));
            s_ps_target_hi[c] = (uint8_t)(de >> 8);
            s_ps_target_lo[c] = (uint8_t)de;
            s_flags1[c] |= (1 << BIT_PITCH_SLIDE_ON);

            note_length_and_pitch(c, get_next_music_byte(c));
            return;
        }

        if (d == CMD_DUTY_CYCLE) {
            uint8_t v = get_next_music_byte(c);
            v = (uint8_t)(((v >> 2) | (v << 6)) & 0xC0);
            s_duty_cycles[c] = v;
            continue;
        }

        if (d == CMD_TEMPO) {
            uint8_t hi = get_next_music_byte(c);
            uint8_t lo = get_next_music_byte(c);
            if (c < CHAN5) {
                s_music_tempo = (uint16_t)((hi << 8) | lo);
                for (int i = 0; i < 4; i++) s_note_delay_frac[i] = 0;
            } else {
                s_sfx_tempo = (uint16_t)((hi << 8) | lo);
                for (int i = 4; i < 8; i++) s_note_delay_frac[i] = 0;
            }
            continue;
        }

        if (d == CMD_STEREO_PANNING) {
            s_stereo_panning = get_next_music_byte(c);
            continue;
        }

        if (d == CMD_UNKNOWNMUSIC0XEF) {
            uint8_t v = get_next_music_byte(c);
            AudioEngine_PlaySound(v);
            if (!s_disable_output_when_sfx_ends) {
                s_disable_output_when_sfx_ends = s_sound_ids[CHAN8];
                s_sound_ids[CHAN8] = 0;
            }
            continue;
        }

        if (d == CMD_DUTY_CYCLE_PATTERN) {
            uint8_t v = get_next_music_byte(c);
            s_duty_cycle_patterns[c] = v;
            s_duty_cycles[c] = (uint8_t)(v & 0xC0);
            s_flags1[c] |= (1 << BIT_ROTATE_DUTY_CYCLE);
            continue;
        }

        if (d == CMD_VOLUME) {
            apu_w(R_AUDVOL, get_next_music_byte(c));
            continue;
        }

        if (d == CMD_EXECUTE_MUSIC) {
            s_flags2[c] |= (1 << BIT_EXECUTE_MUSIC);
            continue;
        }

        if ((d & 0xF0) == CMD_OCTAVE) {
            s_octaves[c] = (uint8_t)(d & 0x0F);
            continue;
        }

        if ((d & 0xF0) == CMD_SFX_NOTE && c >= CHAN4 &&
            !(s_flags2[c] & (1 << BIT_EXECUTE_MUSIC))) {
            note_length_and_pitch(c, d);

            uint8_t len = s_note_delay_counters[c];
            apu_w(reg_addr(c, REG_DUTY_SOUND_LEN),
                  (uint8_t)(s_duty_cycles[c] | len));
            apu_w(reg_addr(c, REG_VOLUME_ENVELOPE), get_next_music_byte(c));
            uint8_t e = get_next_music_byte(c);
            uint8_t hi = 0;
            if (c != CHAN8) hi = get_next_music_byte(c);
            apply_duty_cycle_and_sound_length(c);
            enable_channel_output(c);
            apply_wave_pattern_and_frequency(c, (uint16_t)((hi << 8) | e));
            return;
        }

        if (c >= CHAN5 && (d & 0xF0) == CMD_PITCH_SWEEP &&
            !(s_flags2[c] & (1 << BIT_EXECUTE_MUSIC))) {
            apu_w(R_AUD1SWEEP, get_next_music_byte(c));
            continue;
        }

        if (c == CHAN4) {
            if ((d & 0xF0) == CMD_DRUM_NOTE) {
                uint8_t inst = get_next_music_byte(c);
                if (!s_disable_output_when_sfx_ends)
                    AudioEngine_PlaySound(inst);
                note_length_and_pitch(c, d);
                return;
            }
            if ((d & 0xF0) > CMD_DRUM_NOTE) {
                note_length_and_pitch(c, d);
                return;
            }

            uint8_t inst = (uint8_t)((d >> 4) | (d << 4));
            if (!s_disable_output_when_sfx_ends)
                AudioEngine_PlaySound(inst);
            note_length_and_pitch(c, (uint8_t)(d & 0x0F));
            return;
        }

        note_length_and_pitch(c, d);
        return;
    }
}

static void note_length_and_pitch(int c, uint8_t d) {
    uint8_t saved = d;

    uint16_t de = (uint16_t)((d & 0x0F) + 1);
    uint16_t hl = multiply_add(0, s_note_speeds[c], de);
    uint8_t  a  = (uint8_t)hl;

    uint16_t tempo;
    if (c < CHAN5) {
        tempo = s_music_tempo;
    } else if (c == CHAN8) {
        tempo = 0x0100;
    } else {
        set_sfx_tempo();
        tempo = s_sfx_tempo;
    }

    hl = multiply_add(s_note_delay_frac[c], a, tempo);
    s_note_delay_frac[c]     = (uint8_t)hl;
    s_note_delay_counters[c] = (uint8_t)(hl >> 8);

    if (!(s_flags2[c] & (1 << BIT_EXECUTE_MUSIC)) &&
        (s_flags1[c] & (1 << BIT_NOISE_OR_SFX)))
        return;

    uint8_t hi = (uint8_t)(saved & 0xF0);
    if (hi == CMD_REST) {
        if (c < CHAN5 && s_sound_ids[CHAN5 + c] != 0) return;
        if (c == CHAN3 || c == CHAN7) {
            apu_w(R_AUDTERM, (uint8_t)(apu_r(R_AUDTERM) & kHWDisable[c]));
        } else {
            apu_w(reg_addr(c, REG_VOLUME_ENVELOPE), 0x08);
            apu_w((uint8_t)(reg_addr(c, REG_FREQUENCY_LO) + 1), 0x80);
        }
        return;
    }

    uint16_t freq = calculate_frequency((uint8_t)(saved >> 4), s_octaves[c]);
    if (s_flags1[c] & (1 << BIT_PITCH_SLIDE_ON))
        freq = init_pitch_slide_vars(c, freq);

    if (c < CHAN5 && s_sound_ids[CHAN5 + c] != 0) return;

    apu_w(reg_addr(c, REG_VOLUME_ENVELOPE), s_volumes[c]);
    apply_duty_cycle_and_sound_length(c);
    enable_channel_output(c);

    uint8_t e = (uint8_t)freq, dd = (uint8_t)(freq >> 8);
    if (s_flags1[c] & (1 << BIT_PERFECT_PITCH)) {

        e++;
    }
    s_frequency_low_bytes[c] = e;
    apply_wave_pattern_and_frequency(c, (uint16_t)((dd << 8) | e));
}

static void play_next_note(int c) {
    s_vibrato_delay_counters[c] = s_vibrato_delay_reload[c];
    s_flags1[c] &= (uint8_t)~(1 << BIT_PITCH_SLIDE_ON);
    s_flags1[c] &= (uint8_t)~(1 << BIT_PITCH_SLIDE_DECREASING);

    if (s_engine == 1 && c == CHAN5 &&
        (s_low_health_alarm & (1 << BIT_LOW_HEALTH_ALARM)))
        return;

    play_next_note_commands(c);
}

static void apply_music_affects(int c) {
    if (s_note_delay_counters[c] == 1) { play_next_note(c); return; }
    s_note_delay_counters[c]--;

    if (c < CHAN5 && s_sound_ids[CHAN5 + c] != 0) return;

    if (s_flags1[c] & (1 << BIT_ROTATE_DUTY_CYCLE))
        apply_duty_cycle_pattern(c);

    if (!(s_flags2[c] & (1 << BIT_EXECUTE_MUSIC)) &&
        (s_flags1[c] & (1 << BIT_NOISE_OR_SFX)))
        return;

    if (s_flags1[c] & (1 << BIT_PITCH_SLIDE_ON)) { apply_pitch_slide(c); return; }

    if (s_vibrato_delay_counters[c] != 0) { s_vibrato_delay_counters[c]--; return; }

    uint8_t extent = s_vibrato_extents[c];
    if (extent == 0) return;

    if ((s_vibrato_rates[c] & 0x0F) != 0) { s_vibrato_rates[c]--; return; }

    uint8_t r = s_vibrato_rates[c];
    s_vibrato_rates[c] = (uint8_t)(r | (r >> 4));

    uint8_t e = s_frequency_low_bytes[c];
    uint8_t out;
    if (s_flags1[c] & (1 << BIT_VIBRATO_DIRECTION)) {
        s_flags1[c] &= (uint8_t)~(1 << BIT_VIBRATO_DIRECTION);
        uint8_t below = (uint8_t)(extent & 0x0F);
        out = (e >= below) ? (uint8_t)(e - below) : 0;
    } else {
        s_flags1[c] |= (1 << BIT_VIBRATO_DIRECTION);
        unsigned above = (unsigned)(extent >> 4) + e;
        out = (above > 0xFF) ? 0xFF : (uint8_t)above;
    }
    apu_w(reg_addr(c, REG_FREQUENCY_LO), out);
}

void AudioEngine_UpdateMusic(void) {
    if (!AudioEngine_IsReady()) return;

    for (int c = CHAN1; c < NUM_CHANNELS; c++) {
        if (s_sound_ids[c] == 0) continue;
        if (c < CHAN5 && s_mute_audio_and_pause_music) {
            if (s_mute_audio_and_pause_music & (1 << BIT_MUTE_AUDIO)) continue;
            s_mute_audio_and_pause_music |= (1 << BIT_MUTE_AUDIO);
            apu_w(R_AUDTERM, 0);
            apu_w(R_AUD3ENA, 0);
            apu_w(R_AUD3ENA, AUD3ENA_ON);
            continue;
        }
        apply_music_affects(c);
    }
}

static void clear_channel_state(int i) {
    s_command_pointers[i] = 0;
    s_return_addresses[i] = 0;
    s_sound_ids[i] = 0;
    s_flags1[i] = 0;
    s_flags2[i] = 0;
    s_duty_cycles[i] = 0;
    s_duty_cycle_patterns[i] = 0;
    s_vibrato_delay_counters[i] = 0;
    s_vibrato_extents[i] = 0;
    s_vibrato_rates[i] = 0;
    s_frequency_low_bytes[i] = 0;
    s_vibrato_delay_reload[i] = 0;
    s_ps_length_modifiers[i] = 0;
    s_ps_freq_steps[i] = 0;
    s_ps_freq_steps_frac[i] = 0;
    s_ps_cur_freq_frac[i] = 0;
    s_ps_cur_freq_hi[i] = 0;
    s_ps_cur_freq_lo[i] = 0;
    s_ps_target_hi[i] = 0;
    s_ps_target_lo[i] = 0;
    s_loop_counters[i] = 1;
    s_note_delay_counters[i] = 1;
    s_note_speeds[i] = 1;
}

static uint8_t max_sfx_id(void) {
    switch (s_engine) {
        case 0:  return MAX_SFX_ID_1;
        case 1:  return MAX_SFX_ID_2;
        default: return MAX_SFX_ID_3;
    }
}

#define HEADERS_BASE 0x4000

static void play_sound_common(void) {
    uint16_t hdr = (uint16_t)(HEADERS_BASE + s_sound_id * 3);
    uint8_t first = bank_read(hdr);
    int count = ((first & 0xC0) >> 6) + 1;
    uint16_t de = (uint16_t)(hdr + 1);

    uint8_t chan = (uint8_t)(first & 0x0F);
    for (int n = 0; n < count; n++) {
        s_sound_ids[chan] = s_sound_id;
        if (chan >= CHAN4) s_flags1[chan] |= (1 << BIT_NOISE_OR_SFX);
        uint8_t lo = bank_read(de);
        uint8_t hi = bank_read((uint16_t)(de + 1));
        s_command_pointers[chan] = (uint16_t)((hi << 8) | lo);
        de = (uint16_t)(de + 2);
        if (n + 1 < count) {
            chan = (uint8_t)(bank_read(de) & 0x0F);
            de++;
        }
    }

    if (s_sound_id >= CRY_SFX_START && s_sound_id != CRY_SFX_END &&
        s_sound_id < CRY_SFX_END) {
        for (int i = CHAN5; i < NUM_CHANNELS; i++) s_sound_ids[i] = s_sound_id;
        s_command_pointers[CHAN7] = meta(META_CRYRET);
        if (s_saved_volume == 0) {
            s_saved_volume = apu_r(R_AUDVOL);
            apu_w(R_AUDVOL, 0x77);
        }
    }
}

static void stop_all_audio(void) {
    apu_w(R_AUDENA, AUDENA_ON);
    apu_w(R_AUD3ENA, AUDENA_ON);
    apu_w(R_AUDTERM, 0);
    apu_w(0x1C, 0);
    apu_w(R_AUD1SWEEP, AUD1SWEEP_DOWN);
    apu_w(0x12, AUD1SWEEP_DOWN);
    apu_w(0x17, AUD1SWEEP_DOWN);
    apu_w(0x21, AUD1SWEEP_DOWN);
    apu_w(0x14, AUD1HIGH_LENGTH_ON);
    apu_w(0x19, AUD1HIGH_LENGTH_ON);
    apu_w(0x23, AUD1HIGH_LENGTH_ON);
    apu_w(R_AUDVOL, 0x77);

    s_unused_music_byte = 0;
    s_disable_output_when_sfx_ends = 0;
    s_mute_audio_and_pause_music = 0;
    s_music_wave_instrument = 0;
    s_sfx_wave_instrument = 0;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        s_command_pointers[i] = 0;
        s_return_addresses[i] = 0;
        s_sound_ids[i] = 0;
        s_flags1[i] = 0;
        s_flags2[i] = 0;
        s_duty_cycles[i] = 0;
        s_duty_cycle_patterns[i] = 0;
        s_vibrato_delay_counters[i] = 0;
        s_vibrato_extents[i] = 0;
        s_vibrato_rates[i] = 0;
        s_frequency_low_bytes[i] = 0;
        s_vibrato_delay_reload[i] = 0;
        s_ps_length_modifiers[i] = 0;
        s_ps_freq_steps[i] = 0;
        s_ps_freq_steps_frac[i] = 0;
        s_ps_cur_freq_frac[i] = 0;
        s_ps_cur_freq_hi[i] = 0;
        s_ps_cur_freq_lo[i] = 0;

        s_note_delay_counters[i] = 1;
        s_loop_counters[i] = 1;
        s_note_speeds[i] = 1;
    }

    s_music_tempo = 0x0100;
    s_sfx_tempo   = 0x0100;
    s_stereo_panning = 0xFF;
}

void AudioEngine_StopAll(void) {
    if (!AudioEngine_IsReady()) return;
    memset(s_apu, 0, sizeof s_apu);
    for (int i = 0; i < NUM_CHANNELS; i++) clear_channel_state(i);
    stop_all_audio();
}

static void engine_play_sound(uint8_t sound_id) {
    if (!AudioEngine_IsReady()) return;
    s_sound_id = sound_id;

    if (sound_id == (uint8_t)SFX_STOP_ALL_MUSIC) { stop_all_audio(); return; }

    if (sound_id > max_sfx_id()) {

        s_unused_music_byte = 0;
        s_disable_output_when_sfx_ends = 0;
        s_music_wave_instrument = 0;
        s_sfx_wave_instrument = 0;

        for (int i = 0; i < NUM_MUSIC_CHANS; i++) clear_channel_state(i);
        s_music_tempo = 0x0100;
        s_stereo_panning = 0xFF;
        apu_w(R_AUDVOL, 0);
        apu_w(R_AUD1SWEEP, AUD1SWEEP_DOWN);
        apu_w(R_AUDTERM, 0);
        apu_w(R_AUD3ENA, 0);
        apu_w(R_AUD3ENA, AUD3ENA_ON);
        apu_w(R_AUDVOL, 0x77);
        play_sound_common();
        return;
    }

    uint16_t hdr = (uint16_t)(HEADERS_BASE + sound_id * 3);
    s_sfx_header_pointer = hdr;
    int count = (bank_read(hdr) & 0xC0) >> 6;

    for (int n = count; ; n--) {
        uint8_t entry = bank_read((uint16_t)(hdr + n * 3));
        int e = entry & 0x0F;

        if (s_sound_ids[e] != 0) {

            int decided = 0, play = 0;
            if (e == CHAN8) {
                if (s_sound_id < NOISE_INSTRUMENTS_END) return;
                if (s_sound_ids[e] <= NOISE_INSTRUMENTS_END) { decided = 1; play = 1; }
            }
            if (!decided) play = (s_sound_id <= s_sound_ids[e]);
            if (!play) return;
        }
        clear_channel_state(e);
        if (e == CHAN5) apu_w(R_AUD1SWEEP, AUD1SWEEP_DOWN);
        if (n == 0) break;
    }
    play_sound_common();
}

static uint8_t s_new_sound_id;
static uint8_t s_audio_fade_out_control;
static uint8_t s_audio_fade_out_counter;
static uint8_t s_audio_fade_out_counter_reload;
static uint8_t s_last_music_sound_id;
static int     s_audio_saved_rom_bank;
static uint8_t s_no_audio_fade_out;

void AudioEngine_PlaySound(uint8_t sound_id) {
    if (!AudioEngine_IsReady()) return;
    uint8_t b = sound_id;

    if (s_new_sound_id != 0) {
        for (int i = CHAN5; i <= CHAN8; i++) s_sound_ids[i] = 0;
    }

    if (s_audio_fade_out_control != 0) {
        if (s_new_sound_id == 0) return;
        s_new_sound_id = 0;
        if (s_last_music_sound_id != 0xFF) {

            s_last_music_sound_id = b;
            s_audio_fade_out_counter_reload = s_audio_fade_out_control;
            s_audio_fade_out_counter        = s_audio_fade_out_control;
            s_audio_fade_out_control        = b;
            return;
        }

        s_audio_fade_out_control = 0;
    }

    s_new_sound_id = 0;
    engine_play_sound(b);
}

void AudioEngine_PlayMusic(uint8_t sound_id, int engine) {
    s_new_sound_id = sound_id;
    s_audio_fade_out_control = 0;
    s_audio_saved_rom_bank = engine;
    AudioEngine_SetEngine(engine);
    AudioEngine_PlaySound(sound_id);
}

static void fade_out_audio(void) {
    if (s_audio_fade_out_control == 0) {

        if (s_no_audio_fade_out) return;
        apu_w(R_AUDVOL, 0x77);
        return;
    }
    if (s_audio_fade_out_counter != 0) { s_audio_fade_out_counter--; return; }

    s_audio_fade_out_counter = s_audio_fade_out_counter_reload;
    uint8_t v = apu_r(R_AUDVOL);
    if (v == 0) {

        uint8_t b = s_audio_fade_out_control;
        s_audio_fade_out_control = 0;
        s_new_sound_id = (uint8_t)SFX_STOP_ALL_MUSIC;
        AudioEngine_PlaySound((uint8_t)SFX_STOP_ALL_MUSIC);
        AudioEngine_SetEngine(s_audio_saved_rom_bank);
        s_new_sound_id = b;
        AudioEngine_PlaySound(b);
        return;
    }

    uint8_t lo = (uint8_t)((v & 0x0F) - 1);
    uint8_t hi = (uint8_t)((((v & 0xF0) >> 4) - 1) & 0x0F);
    apu_w(R_AUDVOL, (uint8_t)((hi << 4) | (lo & 0x0F)));
}

static void alarm_play_tone(const uint8_t tone[4]) {

    apu_w(R_AUD1SWEEP, 0);
    apu_w(0x11, tone[0]);
    apu_w(0x12, tone[1]);
    apu_w(0x13, tone[2]);
    apu_w(0x14, tone[3]);
}

static void music_do_low_health_alarm(void) {
    static const uint8_t kToneHi[4]      = { 0xA0, 0xE2, 0x50, 0x87 };
    static const uint8_t kToneLo[4]      = { 0xB0, 0xE2, 0xEE, 0x86 };
    static const uint8_t kToneSilence[4] = { 0x00, 0x00, 0x00, 0x80 };

    uint8_t a = s_low_health_alarm;
    if (a == 0xFF) {
        s_low_health_alarm = 0;
        s_sound_ids[CHAN5] = 0;
        alarm_play_tone(kToneSilence);
        return;
    }
    if (!(a & (1 << BIT_LOW_HEALTH_ALARM))) return;

    uint8_t timer = (uint8_t)(a & 0x7F);
    if (timer == 0) {
        alarm_play_tone(kToneHi);
        timer = 30;
    } else {
        if (timer == 20) alarm_play_tone(kToneLo);
        s_sound_ids[CHAN5] = (uint8_t)CRY_SFX_END;
        timer = (uint8_t)((s_low_health_alarm & 0x7F) - 1);
    }
    s_low_health_alarm = (uint8_t)(timer | (1 << BIT_LOW_HEALTH_ALARM));
}

void AudioEngine_Tick(void) {
    if (!AudioEngine_IsReady()) return;
    fade_out_audio();
    if (s_engine == 1) music_do_low_health_alarm();
    AudioEngine_UpdateMusic();
}

void AudioEngine_SetLowHealthAlarm(uint8_t v) { s_low_health_alarm = v; }

int AudioEngine_LowHealthAlarmActive(void) { return (s_low_health_alarm & 0x80) != 0; }

void AudioEngine_LowHealthAlarmOn(void) {
    s_low_health_alarm |= (1 << BIT_LOW_HEALTH_ALARM);
}

void AudioEngine_LowHealthAlarmOff(void) {

    if (s_low_health_alarm & (1 << BIT_LOW_HEALTH_ALARM))
        s_sound_ids[CHAN5] = 0;
    s_low_health_alarm = 0;
}

void AudioEngine_LowHealthAlarmDisable(void) {
    if (s_low_health_alarm & (1 << BIT_LOW_HEALTH_ALARM))
        s_low_health_alarm = 0xFF;
}

void AudioEngine_OverwriteChannelPointer(int chan, uint16_t addr) {
    if (!AudioEngine_IsReady() || chan < 0 || chan >= NUM_CHANNELS) return;
    s_command_pointers[chan] = addr;
}
void AudioEngine_SetFadeOutControl(uint8_t v) { s_audio_fade_out_control = v; }
void AudioEngine_SetNoFadeOut(int on)         { s_no_audio_fade_out = on ? 1 : 0; }
void AudioEngine_SetCryModifiers(uint8_t freq_mod, uint8_t tempo_mod) {
    s_frequency_modifier = freq_mod;
    s_tempo_modifier     = tempo_mod;
}

void AudioEngine_PlayCry(uint8_t species) {
    if (!AudioEngine_IsReady() || !g_pokemon_cries) return;
    if (species == 0 || species > g_pokemon_cries_count) return;

    const pokemon_cry_t *cd = &g_pokemon_cries[species - 1];
    s_frequency_modifier = (uint8_t)cd->pitch_mod;
    s_tempo_modifier     = cd->tempo_mod;

    AudioEngine_PlaySound((uint8_t)(CRY_SFX_START + cd->base_cry * 3));
}
