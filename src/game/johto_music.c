
#include "johto_music.h"
#include "music.h"
#include "../platform/audio.h"
#include "crystal_audio.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const uint8_t *data;
    int      len;
    int      pc;
    int      active;

    int      is_sfx;
    int      is_cry;

    int      muted;

    uint8_t  tracks;

    int      return_pc;
    int      loop_count;
    int      loop_pc;

    int      octave;
    int      note_length;
    uint8_t  vol_env_byte;
    int      duty_cycle;
    int      noise_mode;
    int      drum_kit;

    uint8_t  duty_pattern;
    int      duty_rotate;

    uint16_t tempo;
    int      wait_frames;
    uint8_t  duration_mod;

    int      transpose_octave;
    int      transpose_pitch;

    int16_t  pitch_offset;

    int      vib_delay;
    int      vib_delay_count;
    int      vib_extent_down;
    int      vib_extent_up;
    int      vib_rate;
    int      vib_counter;
    int      vib_dir;
    uint16_t base_freq;

    uint8_t  pitch_sweep_byte;
    int      has_pitch_sweep;
} johto_ch_t;

#define JOHTO_MUSIC_SLOTS 4
#define JOHTO_SFX_SLOTS   4
#define JOHTO_SLOTS       (JOHTO_MUSIC_SLOTS + JOHTO_SFX_SLOTS)
#define JOHTO_HW(slot)    ((slot) & 3)

static johto_ch_t s_ch[JOHTO_SLOTS];
static int        s_playing = 0;

static int s_sfx_priority = 0;

static int s_stereo = 0;

void JohtoAudio_SetStereo(int on) { s_stereo = on ? 1 : 0; }
int  JohtoAudio_GetStereo(void)   { return s_stereo; }

static uint8_t s_sfx_suspend_mask = 0;

static void sfx_suspend_kanto(int hw) {
    if (s_sfx_suspend_mask & (1u << hw)) return;
    s_sfx_suspend_mask |= (uint8_t)(1u << hw);
    Music_SuspendChannel(hw);
}

static void sfx_release_kanto(int hw) {
    if (!(s_sfx_suspend_mask & (1u << hw))) return;
    s_sfx_suspend_mask &= (uint8_t)~(1u << hw);
    Music_ResumeChannel(hw);
}

#define JOHTO_MAX_VOLUME 0x77
static int s_volume = JOHTO_MAX_VOLUME;
static int s_last_volume = -1;

static void apply_volume(void) {

    Audio_WriteNR50((uint8_t)s_volume);
}

#define JOHTO_MUSIC_FADE_IN 0x80
static int s_fade = 0;
static int s_fade_count = 0;
static int s_fade_id = 0;

void JohtoMusic_FadeTo(johto_music_id_t id, int frames_per_step) {
    if (frames_per_step < 1) frames_per_step = 1;
    if (frames_per_step > 0x3F) frames_per_step = 0x3F;
    s_fade = frames_per_step;
    s_fade_count = frames_per_step;
    s_fade_id = (int)id;
}

int JohtoMusic_IsFading(void) { return s_fade != 0; }

static void fade_music_step(void) {
    if (!s_fade) return;
    if (s_fade_count) { s_fade_count--; return; }
    s_fade_count = s_fade & 0x3F;

    int vol = s_volume & 0x07;
    if (s_fade & JOHTO_MUSIC_FADE_IN) {
        if (vol >= (JOHTO_MAX_VOLUME & 0x0F)) { s_fade = 0; return; }
        vol++;
    } else {
        if (vol == 0) {
            s_volume = 0;
            apply_volume();
            s_fade = 0;
            if (s_fade_id) JohtoMusic_Play((johto_music_id_t)s_fade_id);
            return;
        }
        vol--;
    }

    s_volume = (vol << 4) | vol;
    apply_volume();
}

static johto_music_id_t s_current_music = JOHTO_MUSIC_NONE;

static const uint8_t kJohtoWavePatterns[10][16] = {
    { 0x02,0x46,0x8A,0xCE,0xFF,0xFE,0xED,0xDC,0xCB,0xA9,0x87,0x65,0x44,0x33,0x22,0x11 },
    { 0x02,0x46,0x8A,0xCE,0xEF,0xFF,0xFE,0xEE,0xDD,0xCB,0xA9,0x87,0x65,0x43,0x22,0x11 },
    { 0x13,0x69,0xBD,0xEE,0xEE,0xFF,0xFF,0xED,0xDE,0xFF,0xFF,0xEE,0xEE,0xDB,0x96,0x31 },
    { 0x02,0x46,0x8A,0xCD,0xEF,0xFE,0xDE,0xFF,0xEE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10 },
    { 0x01,0x23,0x45,0x67,0x8A,0xCD,0xEE,0xF7,0x7F,0xEE,0xDC,0xA8,0x76,0x54,0x32,0x10 },
    { 0x00,0x11,0x22,0x33,0x44,0x33,0x22,0x11,0xFF,0xEE,0xCC,0xAA,0x88,0xAA,0xCC,0xEE },
    { 0x02,0x46,0x8A,0xCE,0xCB,0xA9,0x87,0x65,0xFF,0xFE,0xED,0xDC,0x44,0x33,0x22,0x11 },
    { 0xC0,0xA9,0x87,0xF5,0xFF,0xFE,0xED,0xDC,0x44,0x33,0x22,0xF1,0x02,0x46,0x8A,0xCE },
    { 0x44,0x33,0x22,0x1F,0x00,0x46,0x8A,0xCE,0xF8,0xFE,0xED,0xDC,0xCB,0xA9,0x87,0x65 },
    { 0x11,0x00,0x00,0x08,0x00,0x13,0x57,0x9A,0xB4,0xBA,0xA9,0x98,0x87,0x65,0x43,0x21 },
};

static const uint16_t kFreqTable[] = {
    0,
    0xF82C, 0xF89D, 0xF907, 0xF96B, 0xF9CA, 0xFA23,
    0xFA77, 0xFAC7, 0xFB12, 0xFB58, 0xFB9B, 0xFBDA,
};

typedef struct { uint8_t delay; uint8_t env; uint8_t nr43; } johto_drum_step_t;

static const johto_drum_step_t kDrum00[]    = { {1, 0x11, 0x00} };
static const johto_drum_step_t kSnare1[]    = { {1, 0xC1, 0x33} };
static const johto_drum_step_t kSnare2[]    = { {1, 0xB1, 0x33} };
static const johto_drum_step_t kSnare3[]    = { {1, 0xA1, 0x33} };
static const johto_drum_step_t kSnare4[]    = { {1, 0x81, 0x33} };
static const johto_drum_step_t kDrum05[]    = { {8, 0x84, 0x37}, {7, 0x84, 0x36}, {6, 0x83, 0x35}, {5, 0x83, 0x34}, {4, 0x82, 0x33}, {3, 0x81, 0x32} };
static const johto_drum_step_t kTriangle1[] = { {1, 0x51, 0x2A} };
static const johto_drum_step_t kTriangle2[] = { {2, 0x41, 0x2B}, {1, 0x61, 0x2A} };
static const johto_drum_step_t kHiHat1[]    = { {1, 0x81, 0x10} };
static const johto_drum_step_t kSnare5[]    = { {1, 0x82, 0x23} };
static const johto_drum_step_t kSnare6[]    = { {1, 0x82, 0x25} };
static const johto_drum_step_t kSnare7[]    = { {1, 0x82, 0x26} };
static const johto_drum_step_t kHiHat2[]    = { {1, 0xA1, 0x10} };
static const johto_drum_step_t kHiHat3[]    = { {1, 0xA2, 0x11} };
static const johto_drum_step_t kSnare8[]    = { {1, 0xA2, 0x50} };
static const johto_drum_step_t kTriangle3[] = { {1, 0xA1, 0x18}, {1, 0x31, 0x33} };
static const johto_drum_step_t kTriangle4[] = { {3, 0x91, 0x28}, {1, 0x71, 0x18} };
static const johto_drum_step_t kSnare9[]    = { {1, 0x91, 0x22} };
static const johto_drum_step_t kSnare10[]   = { {1, 0x71, 0x22} };
static const johto_drum_step_t kSnare11[]   = { {1, 0x61, 0x22} };
static const johto_drum_step_t kDrum20[]    = { {1, 0x11, 0x11} };
static const johto_drum_step_t kSnare12[]   = { {1, 0x91, 0x33} };
static const johto_drum_step_t kSnare13[]   = { {1, 0x51, 0x32} };
static const johto_drum_step_t kSnare14[]   = { {1, 0x81, 0x31} };
static const johto_drum_step_t kKick1[]     = { {1, 0x88, 0x6B}, {1, 0x71, 0x00} };
static const johto_drum_step_t kTriangle5[] = { {1, 0x91, 0x18} };
static const johto_drum_step_t kDrum27[]    = { {8, 0x92, 0x10} };
static const johto_drum_step_t kDrum28[]    = { {4, 0x91, 0x00}, {4, 0x11, 0x00} };
static const johto_drum_step_t kDrum29[]    = { {4, 0x91, 0x11}, {4, 0x11, 0x00} };
static const johto_drum_step_t kCrash1[]    = { {4, 0x88, 0x15}, {1, 0x65, 0x12} };
static const johto_drum_step_t kDrum31[]    = { {4, 0x51, 0x21}, {4, 0x11, 0x11} };
static const johto_drum_step_t kDrum32[]    = { {4, 0x51, 0x50}, {4, 0x11, 0x11} };
static const johto_drum_step_t kDrum33[]    = { {1, 0xA1, 0x31} };
static const johto_drum_step_t kCrash2[]    = { {1, 0x84, 0x12} };
static const johto_drum_step_t kDrum35[]    = { {4, 0x81, 0x00}, {4, 0x11, 0x00} };
static const johto_drum_step_t kDrum36[]    = { {4, 0x81, 0x21}, {4, 0x11, 0x11} };
static const johto_drum_step_t kKick2[]     = { {1, 0xA8, 0x6B}, {1, 0x71, 0x00} };

typedef struct { const johto_drum_step_t *steps; int count; } johto_drum_inst_t;
#define DI(arr) { arr, (int)(sizeof(arr)/sizeof(arr[0])) }
static const johto_drum_inst_t kDrumInstruments[] = {
    DI(kDrum00), DI(kSnare1), DI(kSnare2), DI(kSnare3), DI(kSnare4), DI(kDrum05),
    DI(kTriangle1), DI(kTriangle2), DI(kHiHat1), DI(kSnare5), DI(kSnare6), DI(kSnare7),
    DI(kHiHat2), DI(kHiHat3), DI(kSnare8), DI(kTriangle3), DI(kTriangle4), DI(kSnare9),
    DI(kSnare10), DI(kSnare11), DI(kDrum20), {NULL,0} , DI(kSnare12),
    DI(kSnare13), DI(kSnare14), DI(kKick1), DI(kTriangle5), DI(kDrum27), DI(kDrum28),
    DI(kDrum29), DI(kCrash1), DI(kDrum31), DI(kDrum32), DI(kDrum33), DI(kCrash2),
    DI(kDrum35), DI(kDrum36), DI(kKick2),
};
#define NUM_DRUM_INSTRUMENTS ((int)(sizeof(kDrumInstruments)/sizeof(kDrumInstruments[0])))

static const int kDrumkit0[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const int kDrumkit1[] = {0, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
static const int kDrumkit2[] = {0, 1, 17, 18, 19, 5, 6, 7, 8, 9, 10, 11, 12};
static const int kDrumkit3[] = {21, 22, 23, 24, 25, 26, 20, 27, 28, 29, 21, 37, 34};
static const int kDrumkit4[] = {21, 20, 23, 24, 25, 33, 26, 35, 31, 32, 36, 37, 30};
static const int kDrumkit5[] = {0, 17, 18, 19, 27, 28, 29, 5, 6, 30, 24, 23, 37};
static const int *kDrumkits[] = { kDrumkit0, kDrumkit1, kDrumkit2, kDrumkit3, kDrumkit4, kDrumkit5 };
#define NUM_DRUMKITS 6

static const johto_drum_step_t *s_noise_sample = NULL;
static int s_noise_sample_step = 0;
static int s_noise_sample_delay = 0;
static int s_noise_sample_total = 0;

static void ch_reset(johto_ch_t *ch) {
    memset(ch, 0, sizeof(*ch));
    ch->return_pc = -1;
    ch->note_length = 1;
    ch->tempo = 256;
}

static int ch_can_write(int c, const johto_ch_t *ch) {
    if (ch->muted) return 0;

    if (ch->is_sfx || ch->is_cry) return 1;
    return !Music_IsChannelSuspended(c);
}

static uint8_t read_u8(johto_ch_t *ch) {
    if (!ch->data || ch->pc >= ch->len) return 0xFF;
    return ch->data[ch->pc++];
}

static int read_addr_le(johto_ch_t *ch) {
    uint8_t lo = read_u8(ch);
    uint8_t hi = read_u8(ch);
    return (hi << 8) | lo;
}

static uint16_t read_bigdw16(johto_ch_t *ch) {
    uint8_t hi = read_u8(ch);
    uint8_t lo = read_u8(ch);
    return (uint16_t)((hi << 8) | lo);
}

static uint16_t get_frequency(johto_ch_t *ch, int pitch) {
    int adj_pitch  = ch->transpose_pitch + pitch;
    int adj_octave = ch->transpose_octave + ch->octave;
    while (adj_pitch > 12) { adj_pitch -= 12; adj_octave++; }
    if (adj_pitch < 1) adj_pitch = 1;
    if (adj_pitch > 12) adj_pitch = 12;

    int16_t raw = (int16_t)kFreqTable[adj_pitch];

    if (adj_octave < 0) adj_octave = 0;
    if (adj_octave > 7) adj_octave = 7;
    int shifts = 7 - adj_octave;
    for (int i = 0; i < shifts; i++) raw >>= 1;

    raw = (int16_t)(raw + ch->pitch_offset);
    return (uint16_t)(raw & 0x7FF);
}

static void fire_drum_note(johto_ch_t *ch, int pitch) {
    int kit = ch->drum_kit;
    if (kit < 0) kit = 0;
    if (kit >= NUM_DRUMKITS) kit = NUM_DRUMKITS - 1;

    int inst = (pitch >= 1 && pitch <= 12) ? kDrumkits[kit][pitch] : 0;
    if (inst < 0 || inst >= NUM_DRUM_INSTRUMENTS) return;
    const johto_drum_inst_t *di = &kDrumInstruments[inst];
    if (di->count == 0) return;

    const johto_drum_step_t *s0 = &di->steps[0];

    if (ch_can_write(3, ch)) {
        Audio_WriteReg(3, 2, s0->env);
        Audio_WriteReg(3, 3, s0->nr43);
        Audio_WriteReg(3, 4, 0x80);
    }

    if (di->count > 1) {
        s_noise_sample = di->steps;
        s_noise_sample_step = 1;
        s_noise_sample_delay = s0->delay;
        s_noise_sample_total = di->count;
    } else {
        s_noise_sample = NULL;
    }
}

static void fire_note(int c, johto_ch_t *ch, uint16_t freq) {

    if (!ch_can_write(c, ch)) return;
    if (c == 0) {
        if (ch->has_pitch_sweep) {
            Audio_WriteReg(0, 0, ch->pitch_sweep_byte);
            ch->has_pitch_sweep = 0;
        } else {
            Audio_WriteReg(0, 0, 0x08);
        }
    }

    if (c == 2) {

        int wave_idx = ch->vol_env_byte & 0xF;
        if (wave_idx >= 10) wave_idx = 0;
        Audio_SetWaveRaw(kJohtoWavePatterns[wave_idx]);
        uint8_t vol_shift = (uint8_t)((ch->vol_env_byte >> 4) & 0x3);
        Audio_WriteReg(2, 2, (uint8_t)(vol_shift << 4));
        Audio_WriteReg(2, 3, (uint8_t)(freq & 0xFF));
        Audio_WriteReg(2, 4, (uint8_t)(((freq >> 8) & 0x07) | 0x80));
        return;
    }

    uint8_t duty_byte = (uint8_t)((ch->duty_cycle << 6) | 0x3F);
    Audio_WriteReg(c, 1, duty_byte);
    Audio_WriteReg(c, 2, ch->vol_env_byte);
    Audio_WriteReg(c, 3, (uint8_t)(freq & 0xFF));
    Audio_WriteReg(c, 4, (uint8_t)(((freq >> 8) & 0x07) | 0x80));
}

static void tick_vibrato(int c, johto_ch_t *ch) {
    if (!ch_can_write(c, ch)) return;
    if ((ch->vib_extent_down == 0 && ch->vib_extent_up == 0) || ch->vib_rate == 0) return;
    if (ch->vib_delay_count > 0) { ch->vib_delay_count--; return; }
    ch->vib_counter++;
    if (ch->vib_counter < ch->vib_rate) return;
    ch->vib_counter = 0;

    int offset = ch->vib_dir ? ch->vib_extent_up : -ch->vib_extent_down;
    ch->vib_dir = !ch->vib_dir;

    int freq = (int)ch->base_freq + offset;
    if (freq < 0) freq = 0;
    if (freq > 0x7FF) freq = 0x7FF;
    Audio_WriteReg(c, 3, (uint8_t)(freq & 0xFF));
    Audio_WriteReg(c, 4, (uint8_t)((freq >> 8) & 0x07));
}

static void tick_duty_rotation(int c, johto_ch_t *ch) {
    if (!ch_can_write(c, ch)) return;
    if (!ch->duty_rotate) return;
    uint8_t p = ch->duty_pattern;
    p = (uint8_t)((p << 2) | (p >> 6));
    ch->duty_pattern = p;
    ch->duty_cycle = (p >> 6) & 3;
    Audio_WriteReg(c, 1, (uint8_t)((ch->duty_cycle << 6) | 0x3F));
}

static int execute_until_note(int c, johto_ch_t *ch) {
    int safety = 4000;
    while (safety-- > 0) {
        if (!ch->data || ch->pc >= ch->len) return 0;
        uint8_t cmd = read_u8(ch);

        if (cmd <= 0xCF && (ch->is_sfx || ch->is_cry)) {

            int raw = (ch->note_length * (cmd + 1)) & 0xFF;
            int product = raw * ch->tempo + ch->duration_mod;
            ch->wait_frames  = product >> 8;
            ch->duration_mod = (uint8_t)(product & 0xFF);
            if (ch->wait_frames < 1) ch->wait_frames = 1;

            ch->vol_env_byte = read_u8(ch);
            uint16_t freq = read_u8(ch);
            if (c != 3) freq |= (uint16_t)(read_u8(ch) << 8);

            ch->base_freq = freq;
            ch->vib_dir = 0;
            ch->vib_delay_count = ch->vib_delay;
            ch->vib_counter = 0;
            fire_note(c, ch, freq);
            return 1;
        }

        if (cmd <= 0xCF) {
            int pitch  = (cmd >> 4) & 0xF;
            int length = cmd & 0xF;

            int raw = (ch->note_length * (length + 1)) & 0xFF;
            int product = raw * ch->tempo + ch->duration_mod;
            ch->wait_frames  = product >> 8;
            ch->duration_mod = (uint8_t)(product & 0xFF);
            if (ch->wait_frames < 1) ch->wait_frames = 1;

            ch->vib_delay_count = ch->vib_delay;
            ch->vib_counter = 0;

            if (pitch == 0) {
                if (ch_can_write(c, ch)) {
                    Audio_WriteReg(c, 2, 0x00);
                    Audio_WriteReg(c, 4, 0x00);
                }
            } else if (ch->noise_mode && c == 3) {
                fire_drum_note(ch, pitch);
            } else {
                uint16_t freq = get_frequency(ch, pitch);
                ch->base_freq = freq;
                ch->vib_dir = 0;
                fire_note(c, ch, freq);
            }
            return 1;
        }

        switch (cmd) {
            case 0xD0: case 0xD1: case 0xD2: case 0xD3:
            case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                ch->octave = cmd & 7;
                break;

            case 0xD8:
                ch->note_length = read_u8(ch);
                if (c < 3) ch->vol_env_byte = read_u8(ch);
                break;

            case 0xD9: {
                uint8_t b = read_u8(ch);
                ch->transpose_octave = (b >> 4) & 0xF;
                ch->transpose_pitch  = b & 0xF;
                break;
            }

            case 0xDA: {

                uint16_t t = read_bigdw16(ch);
                int base = (ch >= &s_ch[JOHTO_MUSIC_SLOTS]) ? JOHTO_MUSIC_SLOTS : 0;
                for (int i = base; i < base + JOHTO_MUSIC_SLOTS; i++) {
                    s_ch[i].tempo = t;
                    s_ch[i].duration_mod = 0;
                }
                break;
            }

            case 0xDB:
                ch->duty_cycle = read_u8(ch) & 3;
                ch->duty_rotate = 0;
                break;

            case 0xDC:
                ch->vol_env_byte = read_u8(ch);
                break;

            case 0xDD:
                ch->pitch_sweep_byte = read_u8(ch);
                ch->has_pitch_sweep = 1;
                break;

            case 0xDE: {
                uint8_t pattern = read_u8(ch);
                ch->duty_pattern = pattern;
                ch->duty_rotate = 1;
                ch->duty_cycle = (pattern >> 6) & 3;
                break;
            }

            case 0xDF:

                ch->is_sfx = !ch->is_sfx;
                break;

            case 0xE0:
                read_u8(ch); read_u8(ch);
                break;

            case 0xE1: {
                ch->vib_delay = read_u8(ch);
                uint8_t b = read_u8(ch);
                int extent = (b >> 4) & 0xF;
                ch->vib_extent_down = (extent + 1) >> 1;
                ch->vib_extent_up   = extent >> 1;
                ch->vib_rate = b & 0xF;
                break;
            }

            case 0xE2:
                read_u8(ch);
                break;

            case 0xE3:
                if (!ch->noise_mode) {
                    ch->noise_mode = 1;
                    ch->drum_kit = read_u8(ch);
                } else {
                    ch->noise_mode = 0;
                }
                break;

            case 0xE4:

                ch->tracks = (uint8_t)((0x11u << c) & read_u8(ch));
                break;

            case 0xE5:
                s_volume = read_u8(ch);
                apply_volume();
                break;

            case 0xE6:
                ch->pitch_offset = (int16_t)read_bigdw16(ch);
                break;

            case 0xE7:
            case 0xE8:
                read_u8(ch);
                break;

            case 0xE9: {
                int8_t delta = (int8_t)read_u8(ch);
                int nt = ch->tempo + delta;
                if (nt < 1) nt = 1;
                if (nt > 65535) nt = 65535;
                ch->tempo = (uint16_t)nt;
                break;
            }

            case 0xEA:
                read_u8(ch); read_u8(ch);
                break;

            case 0xEB:
                read_u8(ch); read_u8(ch);
                break;

            case 0xEC:  s_sfx_priority = 1; break;
            case 0xED:  s_sfx_priority = 0; break;

            case 0xEE:
                read_u8(ch); read_u8(ch);
                break;

            case 0xEF:

                if (!s_stereo) { read_u8(ch); break; }
                ch->tracks = (uint8_t)((0x11u << c) & read_u8(ch));
                break;

            case 0xF0:
                if (!ch->noise_mode) {
                    ch->noise_mode = 1;
                    ch->drum_kit = read_u8(ch);
                } else {
                    ch->noise_mode = 0;
                }
                break;

            case 0xF1: case 0xF2: case 0xF3: case 0xF4:
            case 0xF5: case 0xF6: case 0xF7: case 0xF8: case 0xF9:
                break;

            case 0xFA:
                read_u8(ch);
                break;

            case 0xFB:
                read_u8(ch);
                read_addr_le(ch);
                break;

            case 0xFC:
                ch->pc = read_addr_le(ch);
                break;

            case 0xFD: {
                uint8_t count = read_u8(ch);
                int addr = read_addr_le(ch);
                if (count == 0) {
                    ch->pc = addr;
                } else if (ch->loop_count == 0) {
                    ch->loop_count = count - 1;
                    ch->loop_pc = ch->pc;
                    ch->pc = addr;
                } else {
                    ch->loop_count--;
                    if (ch->loop_count > 0) ch->pc = addr;
                }
                break;
            }

            case 0xFE: {
                int addr = read_addr_le(ch);
                ch->return_pc = ch->pc;
                ch->pc = addr;
                break;
            }

            case 0xFF:
                if (ch->return_pc >= 0) {
                    ch->pc = ch->return_pc;
                    ch->return_pc = -1;
                } else {
                    return 0;
                }
                break;

            default:
                break;
        }
    }
    return 0;
}

static void load_track(const crystal_audio_track_t *t, int base,
                       int as_sfx, int as_cry);

void JohtoMusic_Play(johto_music_id_t id) {

    if (!s_sfx_suspend_mask && Music_IsPlaying()) Music_Stop();

    if (id == s_current_music && s_playing) return;

    JohtoMusic_Stop();
    if (id == JOHTO_MUSIC_NONE || (int)id >= CRYSTAL_NUM_MUSIC)
        return;

    s_current_music = id;
    load_track(&gCrystalMusic[id], 0, 0, 0);
    s_playing = 1;
    s_noise_sample = NULL;

    apply_volume();
}

void JohtoMusic_Stop(void) {
    s_playing = 0;
    s_current_music = JOHTO_MUSIC_NONE;
    for (int c = 0; c < JOHTO_MUSIC_SLOTS; c++) {
        s_ch[c].active = 0;

        if (!Music_IsChannelSuspended(c) && !s_ch[c + JOHTO_MUSIC_SLOTS].active) {
            Audio_WriteReg(c, 2, 0x00);
            Audio_WriteReg(c, 4, 0x00);
        }
    }
    s_noise_sample = NULL;

    Audio_WriteNR50(0x77);
    Audio_WriteNR51(0xFF);
}

int JohtoMusic_IsPlaying(void) { return s_playing; }

int JohtoAudio_IsCryPlaying(void) {
    for (int i = JOHTO_MUSIC_SLOTS; i < JOHTO_SLOTS; i++)
        if (s_ch[i].active && s_ch[i].is_cry) return 1;
    return 0;
}

int JohtoMusic_IsCurrentTrack(johto_music_id_t id) {
    return s_playing && id == s_current_music;
}

void JohtoMusic_Update(void) {

    if (s_noise_sample) {
        if (--s_noise_sample_delay <= 0) {
            if (s_noise_sample_step >= s_noise_sample_total) {
                s_noise_sample = NULL;
            } else {
                const johto_drum_step_t *s = &s_noise_sample[s_noise_sample_step];
                if (!Music_IsChannelSuspended(3)) {
                    Audio_WriteReg(3, 2, s->env);
                    Audio_WriteReg(3, 3, s->nr43);
                    Audio_WriteReg(3, 4, 0x80);
                }
                s_noise_sample_delay = s->delay;
                s_noise_sample_step++;
            }
        }
    }

    int any_sfx = 0;
    for (int i = JOHTO_MUSIC_SLOTS; i < JOHTO_SLOTS; i++)
        if (s_ch[i].active) any_sfx = 1;

    if (!s_playing && !any_sfx) return;

    uint8_t sound_output = 0;
    int any_active = 0;
    for (int s = 0; s < JOHTO_SLOTS; s++) {
        johto_ch_t *ch = &s_ch[s];
        if (!ch->active) continue;
        any_active = 1;

        int c = JOHTO_HW(s);
        int is_music_slot = (s < JOHTO_MUSIC_SLOTS);

        int taken_over = is_music_slot && s_ch[s + JOHTO_MUSIC_SLOTS].active;
        int rested     = is_music_slot && s_sfx_priority && any_sfx;
        int muted      = taken_over || rested;

        if (!taken_over) sound_output |= ch->tracks;

        if (ch->wait_frames >= 2) {
            ch->wait_frames--;
            if (!muted) {
                if (c < 2) {
                    tick_vibrato(c, ch);
                    tick_duty_rotation(c, ch);
                } else if (c == 2) {
                    tick_vibrato(c, ch);
                }
            }
            continue;
        }

        ch->muted = muted;
        int alive = execute_until_note(c, ch);
        ch->muted = 0;
        if (!alive) {
            ch->active = 0;

            if (ch->is_cry && s == JOHTO_MUSIC_SLOTS) {
                if (s_last_volume >= 0) {
                    s_volume = s_last_volume;
                    s_last_volume = -1;
                    apply_volume();
                }
                s_sfx_priority = 0;
            }

            if (ch->is_sfx || ch->is_cry) sfx_release_kanto(c);
            ch->is_sfx = ch->is_cry = 0;
        }
    }

    int music_active = 0;
    for (int i = 0; i < JOHTO_MUSIC_SLOTS; i++)
        if (s_ch[i].active) music_active = 1;
    if (!music_active) s_playing = 0;
    (void)any_active;

    fade_music_step();
    Audio_WriteNR51(sound_output);
}

static void load_track(const crystal_audio_track_t *t, int base,
                       int as_sfx, int as_cry) {
    if (!t || !t->channels) return;
    for (int i = 0; i < t->num_channels; i++) {
        const crystal_audio_channel_t *cd = &t->channels[i];
        int slot = base + JOHTO_HW(cd->channel - 1);
        johto_ch_t *ch = &s_ch[slot];
        ch_reset(ch);
        ch->data   = cd->data;
        ch->len    = cd->len;
        ch->pc     = cd->entry;
        ch->active = 1;
        ch->is_sfx = as_sfx;
        ch->is_cry = as_cry;

        ch->tracks = (uint8_t)(0x11u << JOHTO_HW(slot));
        if (as_sfx || as_cry) sfx_suspend_kanto(JOHTO_HW(slot));
    }
}

void JohtoAudio_PlaySFX(int sfx_id) {
    if (sfx_id < 0 || sfx_id >= CRYSTAL_NUM_SFX) return;

    for (int i = JOHTO_MUSIC_SLOTS; i < JOHTO_SLOTS; i++) {
        s_ch[i].active = 0;
        Audio_WriteReg(JOHTO_HW(i), 2, 0x08);
        Audio_WriteReg(JOHTO_HW(i), 4, 0x80);
    }
    load_track(&gCrystalSfx[sfx_id], JOHTO_MUSIC_SLOTS, 1, 0);
}

void JohtoAudio_PlayCryModified(int species, int pitch_add, int tempo_add) {
    if (species < 0 || species >= CRYSTAL_NUM_MON_CRIES) return;
    const crystal_mon_cry_t *mc = &gCrystalMonCries[species];
    if (mc->index >= CRYSTAL_NUM_CRIES) return;

    int pitch  = (int)mc->pitch  + pitch_add;
    int length = (int)mc->length + tempo_add;

    for (int i = JOHTO_MUSIC_SLOTS; i < JOHTO_SLOTS; i++)
        s_ch[i].active = 0;
    load_track(&gCrystalCries[mc->index], JOHTO_MUSIC_SLOTS, 0, 1);

    for (int i = JOHTO_MUSIC_SLOTS; i < JOHTO_SLOTS; i++) {
        if (!s_ch[i].active) continue;
        s_ch[i].pitch_offset = (int16_t)pitch;
        if (JOHTO_HW(i) != 3) s_ch[i].tempo = (uint16_t)length;
    }

    if (s_last_volume < 0) {
        s_last_volume = s_volume;
        s_volume = JOHTO_MAX_VOLUME;
        apply_volume();
    }
    s_sfx_priority = 1;
}

void JohtoAudio_PlayCry(int species) {
    JohtoAudio_PlayCryModified(species, 0, 0);
}

static int track_name_matches(const char *dsl, const char *sym) {
    if (!dsl || !sym) return 0;
    if (strncasecmp(sym, "Music_", 6) == 0) sym += 6;
    while (*dsl && *sym) {
        if (*dsl == '_') { dsl++; continue; }
        if (*sym == '_') { sym++; continue; }
        if (tolower((unsigned char)*dsl) != tolower((unsigned char)*sym)) return 0;
        dsl++; sym++;
    }
    while (*dsl == '_') dsl++;
    while (*sym == '_') sym++;
    return *dsl == 0 && *sym == 0;
}

johto_music_id_t JohtoMusic_ForTrackName(const char *track_name) {
    if (!track_name) return JOHTO_MUSIC_NONE;

    for (int i = 0; i < CRYSTAL_NUM_MUSIC; i++) {
        if (track_name_matches(track_name, gCrystalMusic[i].name))
            return (johto_music_id_t)i;
    }
    return JOHTO_MUSIC_NONE;
}
