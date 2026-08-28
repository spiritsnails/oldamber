
#include "gb_apu.h"

#include <string.h>

#define R_NR10 0x10
#define R_NR52 0x26
#define R_WAVE 0x30

static uint8_t s_reg[0x40];

static const uint8_t kReadOrMask[0x40] = {
    [0x10] = 0x80, [0x11] = 0x3F, [0x12] = 0x00, [0x13] = 0xFF, [0x14] = 0xBF,
    [0x15] = 0xFF, [0x16] = 0x3F, [0x17] = 0x00, [0x18] = 0xFF, [0x19] = 0xBF,
    [0x1A] = 0x7F, [0x1B] = 0xFF, [0x1C] = 0x9F, [0x1D] = 0xFF, [0x1E] = 0xBF,
    [0x1F] = 0xFF, [0x20] = 0xFF, [0x21] = 0x00, [0x22] = 0x00, [0x23] = 0xBF,
    [0x24] = 0x00, [0x25] = 0x00, [0x26] = 0x70,
};

typedef struct {
    int      enabled;
    int      dac_on;
    uint16_t freq;
    int32_t  timer;
    uint8_t  duty_pos;
    uint16_t length;
    int      length_enabled;
    uint8_t  volume;
    uint8_t  env_period;
    int      env_direction;
    uint8_t  env_timer;

    uint8_t  sweep_period, sweep_shift;
    int      sweep_negate;
    uint8_t  sweep_timer;
    uint16_t sweep_shadow;
    int      sweep_enabled;
    int      sweep_negate_used;

    uint8_t  sw_countdown;
    int32_t  sw_calc_cd;
    int32_t  sw_reload_timer;
    uint16_t sw_addend;
    uint16_t sw_shadow;
    int      sw_unshifted;
    int      sw_instant_done;
    int32_t  sw_restart_hold;
    int      just_reloaded;
    int      did_tick;
} square_t;

typedef struct {
    int      enabled;
    int      dac_on;
    uint16_t freq;
    int32_t  timer;
    uint8_t  pos;
    uint8_t  sample_buffer;
    uint16_t length;
    int      length_enabled;
    uint8_t  shift;
} wave_t;

typedef struct {
    int      enabled;
    int      dac_on;
    int32_t  timer;
    uint16_t lfsr;
    int      narrow;
    uint16_t length;
    int      length_enabled;
    uint8_t  volume;
    uint8_t  env_period;
    int      env_direction;
    uint8_t  env_timer;

    uint16_t counter;
    int32_t  counter_cd;

    int      bg_active;
    int      did_step_counter;
    int      bg_active_prev;
    int      started_dac_off;
    int      pending_lfsr;
    int      pending_step;
    int      countdown_reloaded;

    int32_t  dmg_delay;
} noise_t;

static square_t s_sq[2];
static wave_t   s_wave;
static noise_t  s_noise;

static int      s_power;
static int32_t  s_seq_timer;
static uint8_t  s_seq_step;

static float    s_hp_l, s_hp_r, s_hp_coeff = 0.999958f;
static uint32_t s_out_rate = 44100;

static const uint8_t kDuty[4][8] = {
    { 0,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,1,1,1 },
    { 0,1,1,1,1,1,1,0 },
};

static const uint8_t kNoiseDivisor[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

static uint16_t sq_freq(int i) {
    return (uint16_t)(((s_reg[0x14 + i * 5] & 0x07) << 8) | s_reg[0x13 + i * 5]);
}

int g_gbapu_trigger_delay = 4;
int g_gbapu_ablate_sweep = 0;
int g_gbapu_wave_trigger_delay = 6;
int g_gbapu_env_step = 7;
int g_gbapu_env_zero_is_8 = 0;

int g_gbapu_env_delay = 4096;
int g_gbapu_env_trigger_extra = 0;
int g_gbapu_env_wrap8 = 0;
int g_gbapu_sq_period_adj = 0;

int g_gbapu_just_reloaded = 1;

int g_gbapu_backstep = 0;

int g_gbapu_init_duty = 0;

int g_gbapu_noise_counter = 1;

int g_gbapu_noise_cinit = 0;

int g_gbapu_noise_trig_adj = 0;

int g_gbapu_noise_align = 0;

int g_gbapu_noise_cdoff = 0;

int g_gbapu_noise_entry_guard = 1;
int g_gbapu_trigger_trace = 0;
int g_gbapu_last_branch = 0;

int g_gbapu_noise_div_latch = 0;
static int s_noise_div_chunk = 2;
int g_gbapu_noise_div_chunk_on = 1;
static int s_noise_div_prev = -1;
int g_gbapu_noise_nudge = 15;

int g_gbapu_dmg_delay = 1;

int g_gbapu_sweep_delay = 0;

int g_gbapu_sweep2 = 1;

int g_gbapu_nr43_glitch = 1;
uint64_t g_gbapu_nr43_calls = 0, g_gbapu_nr43_fires = 0;
uint64_t g_gbapu_backstep_hits = 0;
uint64_t g_gbapu_backstep_cand = 0;

int g_gbapu_trigger_quantise = 0;
int g_gbapu_mute_mask = 0;

float g_gbapu_ch_gain[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

void GbApu_SetChannelGain(int ch, float gain) {
    if (ch < 0 || ch > 3) return;
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    g_gbapu_ch_gain[ch] = gain;
}

float g_gbapu_dac_discharge = 0.002f;
static float s_dac_level[4];
float g_gbapu_hp_override = 0.0f;

float g_gbapu_lp_cutoff = 0.0f;
static float s_lp_l, s_lp_r, s_lp_l2, s_lp_r2;
static uint64_t s_t_total;

static int32_t s_sec_timer;
static uint8_t s_env_countdown[3];

static void sq_reload_timer(int i) {
    s_sq[i].timer = (2048 - s_sq[i].freq) * 4;
}

int g_gbapu_lfdiv = 0;

static void sq_reload_timer_trigger(int i) {
    int32_t align = 0;
    if (g_gbapu_trigger_quantise)
        align = (int32_t)((g_gbapu_trigger_quantise - (s_t_total % g_gbapu_trigger_quantise))
                          % g_gbapu_trigger_quantise);
    int32_t td = g_gbapu_trigger_delay;
    if (g_gbapu_lfdiv) {
        int lf = (int)((s_t_total >> 1) & 1);
        if (g_gbapu_lfdiv == 2) lf ^= 1;
        td -= 2 * lf;
    }
    s_sq[i].timer = (2048 - s_sq[i].freq) * 4 + td + align;
}

static void wave_reload_timer(void) {
    s_wave.timer = (2048 - s_wave.freq) * 2;
}

static void noise_reload_timer(void) {
    uint8_t nr43  = s_reg[0x22];
    uint8_t shift = nr43 >> 4;
    int32_t p = kNoiseDivisor[nr43 & 7] << shift;
    s_noise.timer = p;
}

static int dac_enabled_from_env(uint8_t nrx2) { return (nrx2 & 0xF8) != 0; }

static uint16_t sweep_calculate(void) {
    uint16_t d = (uint16_t)(s_sq[0].sweep_shadow >> s_sq[0].sweep_shift);
    uint16_t n;
    if (s_sq[0].sweep_negate) {
        n = (uint16_t)(s_sq[0].sweep_shadow - d);
        s_sq[0].sweep_negate_used = 1;
    } else {
        n = (uint16_t)(s_sq[0].sweep_shadow + d);
    }
    if (n > 2047) s_sq[0].enabled = 0;
    return n;
}

static void trigger_square(int i) {
    square_t *c = &s_sq[i];
    int was_active = c->enabled && c->dac_on;
    c->enabled = 1;
    c->did_tick = 0;
    if (c->length == 64) c->length = 0;
    c->freq = sq_freq(i);
    sq_reload_timer_trigger(i);
    c->env_timer   = c->env_period;
    c->volume      = (uint8_t)(s_reg[0x12 + i * 5] >> 4);
    c->env_period  = (uint8_t)(s_reg[0x12 + i * 5] & 7);
    c->env_direction = (s_reg[0x12 + i * 5] & 0x08) ? +1 : -1;
    c->env_timer   = c->env_period;
    s_env_countdown[i] = c->env_period;
    c->dac_on      = dac_enabled_from_env(s_reg[0x12 + i * 5]);
    if (!c->dac_on) c->enabled = 0;

    if (i == 0) {
        c->sweep_shadow  = c->freq;
        c->sweep_period  = (uint8_t)((s_reg[0x10] >> 4) & 7);
        c->sweep_shift   = (uint8_t)(s_reg[0x10] & 7);
        c->sweep_negate  = (s_reg[0x10] & 0x08) != 0;
        c->sweep_negate_used = 0;
        c->sweep_timer   = c->sweep_period ? c->sweep_period : 8;
        c->sweep_enabled = (c->sweep_period || c->sweep_shift);

        if (!g_gbapu_sweep2 && c->sweep_shift) sweep_calculate();
        if (g_gbapu_sweep2) {

            uint8_t nr10 = s_reg[R_NR10];
            c->sw_instant_done = 0;
            c->sw_shadow = 0;
            if (nr10 & 7) {
                c->sw_calc_cd      = nr10 & 7;
                c->sw_reload_timer = 3 + (was_active ? 0 : 1);
                c->sw_unshifted    = 0;
                c->sw_addend = (uint16_t)(c->freq >> (nr10 & 7));
            } else {
                c->sw_addend = 0;
            }
            c->sw_restart_hold = 4;
            c->sw_countdown = (uint8_t)(((nr10 >> 4) & 7) ^ 7);
        }
    }
}

static void trigger_wave(void) {
    s_wave.enabled = 1;
    if (s_wave.length == 256) s_wave.length = 0;
    s_wave.freq = (uint16_t)(((s_reg[0x1E] & 0x07) << 8) | s_reg[0x1D]);
    s_wave.timer = (2048 - s_wave.freq) * 2 + g_gbapu_wave_trigger_delay;
    s_wave.pos = 0;
    s_wave.dac_on = (s_reg[0x1A] & 0x80) != 0;
    if (!s_wave.dac_on) s_wave.enabled = 0;
}

static void trigger_noise(void) {

    int was_active_noise = s_noise.enabled && s_noise.dac_on;
    s_noise.enabled = 1;
    if (s_noise.length == 64) s_noise.length = 0;
    noise_reload_timer();
    if (g_gbapu_noise_counter) {

        s_noise.bg_active = 1;
        int r      = s_reg[0x22] & 7;
        int align  = (int)(((s_t_total >> 1) + (uint64_t)g_gbapu_noise_align) & 3);
        int active = was_active_noise;
        int cnt_active = (s_reg[0x21] & 0xF8) != 0;
        int was_bg = s_noise.bg_active_prev;
        int instant_step = 0, div_1_glitch = 0;
        int cd;

        int32_t cd_t = s_noise.counter_cd + g_gbapu_noise_cdoff * 2;
        int r_eff = r;
        g_gbapu_last_branch = 0;
        if ((g_gbapu_noise_nudge & 1) && r > 1 && cd_t == 2) {
            g_gbapu_last_branch = 1;
            s_noise.counter = (uint16_t)((s_noise.counter + 1) & 0x3FFF);
        }
        else if (cd_t == 4 && (align & 3) == 0 && active) {
            if (r == 0) {
                g_gbapu_last_branch = 2;
                if (g_gbapu_noise_nudge & 4) r_eff = 8;
            }
            else if (r == 1 && (g_gbapu_noise_nudge & 2)) {
                g_gbapu_last_branch = 3;
                if (!s_noise.did_step_counter) div_1_glitch = 1;
                uint16_t mask = (uint16_t)(1u << (s_reg[0x22] >> 4));
                int ob = (s_noise.counter & mask) != 0;
                uint16_t bumped = (uint16_t)((s_noise.counter + 1) & 0x3FFF);
                int nb = (bumped & mask) != 0;
                if (g_gbapu_noise_nudge & 8) s_noise.counter = bumped;
                if (nb && !ob) instant_step = 1;
            }
        }

        cd = (r_eff == 0 ? 6 : r_eff * 4 + 6);

        if (align & 1) {
            if (!r_eff) cd++;
            else if (align & 2) { if (r_eff == 1 && !active) cd++; else cd -= 3; }
            else { cd--; if (r_eff == 1 && active) cd -= 4; }
        }
        else if (r_eff) {
            if (align & 2) cd -= 2;
            else if (r_eff > 1) cd -= 4;
            else if (r_eff == 1 && active && !(s_reg[0x22] & 0xF0)) cd -= 4;
        }

        if (r_eff > 1) {
            if (!cnt_active && !(align & 3)) cd += 4;
        }
        else if (was_bg && !active && !(align & 3)) {
            if (r_eff == 0) { if (s_noise.started_dac_off) cd += 28; }
            else cd -= 4;
        }
        if (div_1_glitch) cd -= 4;

        s_noise.counter_cd = cd * 2 + g_gbapu_noise_trig_adj;
        s_noise.started_dac_off = !cnt_active;
        s_noise.bg_active_prev = 1;
        s_noise.pending_lfsr = (!r_eff && active && (align & 3) == 3) ? 0x0055 : 0;
        s_noise.pending_step = instant_step;

        s_noise.did_step_counter = ((align & 3) == 2);
    }

    s_noise.lfsr = g_gbapu_noise_counter
                 ? (uint16_t)(~(uint16_t)s_noise.pending_lfsr & 0x7FFF)
                 : 0x7FFF;
    if (g_gbapu_noise_counter && s_noise.pending_step) {
        uint16_t x = (uint16_t)((s_noise.lfsr ^ (s_noise.lfsr >> 1)) & 1);
        s_noise.lfsr >>= 1;
        s_noise.lfsr = (uint16_t)(s_noise.lfsr | (x << 14));
        if (s_noise.narrow)
            s_noise.lfsr = (uint16_t)((s_noise.lfsr & ~0x40u) | (x << 6));
    }
    s_noise.volume  = (uint8_t)(s_reg[0x21] >> 4);
    s_noise.env_period = (uint8_t)(s_reg[0x21] & 7);
    s_noise.env_direction = (s_reg[0x21] & 0x08) ? +1 : -1;
    s_noise.env_timer = s_noise.env_period;
    s_env_countdown[2] = s_noise.env_period;
    s_noise.dac_on  = dac_enabled_from_env(s_reg[0x21]);
    if (!s_noise.dac_on) s_noise.enabled = 0;
}

static void clock_length_square(int i) {
    if (s_sq[i].length_enabled && s_sq[i].length < 64) {
        if (++s_sq[i].length == 64) s_sq[i].enabled = 0;
    }
}

static void clock_lengths(void) {
    clock_length_square(0);
    clock_length_square(1);
    if (s_wave.length_enabled && s_wave.length < 256) {
        if (++s_wave.length == 256) s_wave.enabled = 0;
    }
    if (s_noise.length_enabled && s_noise.length < 64) {
        if (++s_noise.length == 64) s_noise.enabled = 0;
    }
}

static void clock_envelope_square(int i) {
    s_env_countdown[i] = (uint8_t)((s_env_countdown[i] - 1) & 7);
}

static void secondary_envelope_square(int i) {
    square_t *c = &s_sq[i];
    if (!c->enabled || s_env_countdown[i] != 0) return;

    uint8_t reload = g_gbapu_env_wrap8 ? 8 : c->env_period;
    if (c->env_period == 0 && g_gbapu_env_zero_is_8) reload = 8;
    s_env_countdown[i] = reload;
    if (c->env_period == 0) return;
    int v = c->volume + c->env_direction;
    if (v >= 0 && v <= 15) c->volume = (uint8_t)v;
}

static void noise_step_lfsr(void) {
    uint16_t x = (uint16_t)((s_noise.lfsr ^ (s_noise.lfsr >> 1)) & 1);
    s_noise.lfsr >>= 1;
    s_noise.lfsr = (uint16_t)(s_noise.lfsr | (x << 14));
    if (s_noise.narrow)
        s_noise.lfsr = (uint16_t)((s_noise.lfsr & ~0x40u) | (x << 6));
}

static void nr43_write_glitch(uint8_t old, uint8_t val) {
    if ((old & 0xF0) == (val & 0xF0)) return;
    if (!g_gbapu_nr43_glitch) return;
    g_gbapu_nr43_calls++;

    uint16_t ec = s_noise.counter;
    if (s_noise.countdown_reloaded) ec |= (uint16_t)((ec - 1) & 0x3FFF);

    int old_bit    = (ec >> (old >> 4)) & 1;
    uint8_t gv     = (uint8_t)((old & 0x7F) | (val & 0x80));
    int glitch_bit = (ec >> (gv >> 4)) & 1;
    int new_bit    = (ec >> (val >> 4)) & 1;

    if (old_bit == new_bit && new_bit != glitch_bit) {
        g_gbapu_nr43_fires++;
        noise_step_lfsr();
    }
    else if ((val & 0xF0) <= 0x20 && !glitch_bit && !new_bit && !old_bit && (ec & 8)) {
        g_gbapu_nr43_fires++;
        noise_step_lfsr();
    }
}

static void secondary_envelope_noise(void) {
    if (!s_noise.enabled || s_env_countdown[2] != 0) return;
    s_env_countdown[2] = s_noise.env_period;
    if (s_noise.env_period == 0) return;
    int v = s_noise.volume + s_noise.env_direction;
    if (v >= 0 && v <= 15) s_noise.volume = (uint8_t)v;
}

static int32_t s_env_defer_t = -1;

static void apply_secondary_event(void) {
    secondary_envelope_square(0);
    secondary_envelope_square(1);
    secondary_envelope_noise();
}

static void secondary_event(void) {
    if (g_gbapu_env_delay <= 0) { apply_secondary_event(); return; }
    s_env_defer_t = g_gbapu_env_delay;
}

static void clock_envelopes(void) {
    clock_envelope_square(0);
    clock_envelope_square(1);
    s_env_countdown[2] = (uint8_t)((s_env_countdown[2] - 1) & 7);
}

static void sw_calculation_done(void) {
    square_t *c = &s_sq[0];
    uint8_t nr10 = s_reg[R_NR10];
    if (c->sw_restart_hold == 0) c->sw_shadow = c->freq;
    if (nr10 & 8) c->sw_addend ^= 0x7FF;
    if ((int)c->sw_shadow + (int)c->sw_addend > 0x7FF && !(nr10 & 8))
        c->enabled = 0;
}

static void sw_trigger_calculation(void) {
    square_t *c = &s_sq[0];
    uint8_t nr10 = s_reg[R_NR10];
    if (!(nr10 & 0x70) || c->sw_countdown != 7) return;

    if (nr10 & 0x07) {
        uint16_t v = (uint16_t)((c->sw_addend + c->sw_shadow
                                 + ((nr10 & 8) ? 1 : 0)) & 0x7FF);
        c->freq = v;
        s_reg[0x13] = (uint8_t)(v & 0xFF);
        s_reg[0x14] = (uint8_t)((s_reg[0x14] & 0xF8) | ((v >> 8) & 7));
    }
    if (c->sw_restart_hold == 0)
        c->sw_addend = (uint16_t)(c->freq >> (nr10 & 7));

    c->sw_calc_cd       = nr10 & 7;
    c->sw_reload_timer  = 1;
    c->sw_unshifted     = !(nr10 & 7);
    c->sw_countdown     = (uint8_t)(((nr10 >> 4) & 7) ^ 7);
    if (c->sw_calc_cd == 0) c->sw_instant_done = 1;
}

static void sw_step_unit(void) {
    square_t *c = &s_sq[0];
    uint8_t nr10 = s_reg[R_NR10];
    if (c->sw_reload_timer > 0) {
        if (--c->sw_reload_timer == 0) {
            if (!c->sw_calc_cd && c->sw_instant_done) sw_calculation_done();
            c->sw_instant_done = 0;
        }
        return;
    }

    if (c->sw_calc_cd && ((nr10 & 7) || c->sw_unshifted)) {
        if (--c->sw_calc_cd == 0) sw_calculation_done();
    }
}

static int32_t s_sweep_defer_t = -1;
static int32_t s_sw_acc;

static void clock_sweep_apply(void);

static void clock_sweep(void) {
    if (g_gbapu_sweep2) {
        s_sq[0].sw_countdown = (uint8_t)((s_sq[0].sw_countdown + 1) & 7);
        sw_trigger_calculation();
        return;
    }
    if (g_gbapu_sweep_delay > 0) { s_sweep_defer_t = g_gbapu_sweep_delay; return; }
    clock_sweep_apply();
}

static void clock_sweep_apply(void) {
    if (g_gbapu_ablate_sweep) return;
    square_t *c = &s_sq[0];
    if (c->sweep_timer && --c->sweep_timer) return;
    c->sweep_timer = c->sweep_period ? c->sweep_period : 8;
    if (!c->sweep_enabled || c->sweep_period == 0) return;

    uint16_t n = sweep_calculate();
    if (c->sweep_shift) {

        n &= 0x7FF;
        c->sweep_shadow = n;
        c->freq = n;
        s_reg[0x13] = (uint8_t)(n & 0xFF);
        s_reg[0x14] = (uint8_t)((s_reg[0x14] & 0xF8) | ((n >> 8) & 7));

        sweep_calculate();
    }
}

static void frame_sequencer_step(void) {
    s_seq_step++;
    if ((s_seq_step & 7) == (g_gbapu_env_step & 7)) clock_envelopes();
    if ((s_seq_step & 1) == 1) clock_lengths();
    if ((s_seq_step & 3) == 3) clock_sweep();
}

void GbApu_SetOutputRate(uint32_t hz) {
    if (!hz) return;
    s_out_rate = hz;

    double per_sample = 1.0;

    double decay = 0.9999947;
    double n = (double)GB_APU_CLOCK / (double)hz;

    unsigned k = (unsigned)n;
    double base = decay;
    while (k) {
        if (k & 1) per_sample *= base;
        base *= base;
        k >>= 1;
    }
    s_hp_coeff = g_gbapu_hp_override > 0.0f ? g_gbapu_hp_override : (float)per_sample;
}

void GbApu_SetSeqPhase(int32_t t_until_next_step) {
    if (t_until_next_step > 0 && t_until_next_step <= 8192) {
        s_seq_timer = t_until_next_step;
        s_sec_timer = t_until_next_step > 4096 ? t_until_next_step - 4096
                                                : t_until_next_step + 4096;
    }
}

void GbApu_SetSeqStep(uint8_t step) { s_seq_step = (uint8_t)(step & 7); }

uint16_t GbApu_DebugCh1Freq(void) { return s_sq[0].freq; }
uint16_t GbApu_DebugSqFreq(int i)  { return s_sq[i & 1].freq; }
uint8_t  GbApu_DebugSqVol(int i)   { return s_sq[i & 1].volume; }
uint8_t  GbApu_DebugSqPos(int i)   { return s_sq[i & 1].duty_pos; }
uint8_t  GbApu_DebugSqDuty(int i)  { return (uint8_t)((s_reg[0x11 + (i & 1) * 5] >> 6) & 3); }
int32_t  GbApu_DebugCh1Timer(void) { return s_sq[0].timer; }
uint64_t g_gbapu_ch1_reloads;
uint64_t GbApu_DebugCh1Reloads(void) { return g_gbapu_ch1_reloads; }
uint8_t  GbApu_DebugNoiseVolume(void) { return s_noise.volume; }
int      GbApu_DebugAlign(void)        { return (int)(((s_t_total >> 1) + (uint64_t)g_gbapu_noise_align) & 3); }
int32_t  GbApu_DebugNoiseCd(void)      { return s_noise.counter_cd; }
uint16_t GbApu_DebugNoiseCounter(void) { return s_noise.counter; }
uint16_t GbApu_DebugNoiseLfsr(void)    { return s_noise.lfsr; }
uint8_t  GbApu_DebugNoiseBit(void)    { return (uint8_t)((~s_noise.lfsr) & 1); }
uint8_t  GbApu_DebugNoiseOn(void)     { return (uint8_t)(s_noise.enabled && s_noise.dac_on); }
uint8_t  GbApu_DebugCh1DutyPos(void) { return s_sq[0].duty_pos; }
uint8_t  GbApu_DebugCh1Volume(void)  { return s_sq[0].volume; }

void GbApu_Reset(void) {
    memset(s_reg, 0, sizeof s_reg);
    memset(&s_sq, 0, sizeof s_sq);
    memset(&s_wave, 0, sizeof s_wave);
    memset(&s_noise, 0, sizeof s_noise);
    s_power = 0;
    s_seq_timer = 8192;
    s_sec_timer = 4096;
    s_seq_step = 0;
    s_hp_l = s_hp_r = 0.0f;
    s_noise.lfsr = 0x7FFF;
    s_sq[0].duty_pos = s_sq[1].duty_pos = (uint8_t)(g_gbapu_init_duty & 7);
    s_noise.counter = (uint16_t)(g_gbapu_noise_cinit & 0x3FFF);
    GbApu_SetOutputRate(s_out_rate);
}

uint8_t GbApu_ReadReg(uint8_t lo) {
    if (lo >= 0x40) return 0xFF;
    if (lo == R_NR52) {
        uint8_t v = (uint8_t)((s_power ? 0x80 : 0) |
                              (s_sq[0].enabled  ? 1 : 0) |
                              (s_sq[1].enabled  ? 2 : 0) |
                              (s_wave.enabled   ? 4 : 0) |
                              (s_noise.enabled  ? 8 : 0));
        return (uint8_t)(v | kReadOrMask[lo]);
    }
    return (uint8_t)(s_reg[lo] | kReadOrMask[lo]);
}

static void sq_nrx4_backstep(int i, uint8_t prev, uint8_t value) {
    if (!g_gbapu_backstep) return;
    if (value & 0x80) return;
    if (!(s_sq[i].enabled && s_sq[i].dac_on)) return;
    if ((prev & 7) != 7) return;
    if ((value & 7) == 7) return;
    g_gbapu_backstep_cand++;
    if (!s_sq[i].did_tick) return;

    int32_t cd = (s_sq[i].timer / 2) - 1;
    if (!(cd & 1)) return;
    if ((cd >> 1) != (2047 - (int32_t)s_sq[i].freq)) return;
    s_sq[i].duty_pos = (uint8_t)((s_sq[i].duty_pos - 1) & 7);
    g_gbapu_backstep_hits++;
}

void GbApu_WriteReg(uint8_t lo, uint8_t value) {
    if (lo >= 0x40) return;

    if (lo == R_NR52) {
        int on = (value & 0x80) != 0;
        if (!on && s_power) {

            for (int i = R_NR10; i < R_NR52; i++) s_reg[i] = 0;
            memset(&s_sq, 0, sizeof s_sq);
            memset(&s_wave, 0, sizeof s_wave);
            memset(&s_noise, 0, sizeof s_noise);
            s_noise.lfsr = 0x7FFF;
        } else if (on && !s_power) {
            s_seq_step = 0;
        }
        s_power = on;
        s_reg[lo] = (uint8_t)(value & 0x80);
        return;
    }

    if (!s_power && lo < R_WAVE) return;

    uint8_t prev_reg = s_reg[lo];
    s_reg[lo] = value;

    switch (lo) {

        case 0x10:
            s_sq[0].sweep_period = (uint8_t)((value >> 4) & 7);
            s_sq[0].sweep_shift  = (uint8_t)(value & 7);

            if (s_sq[0].sweep_negate && !(value & 0x08) && s_sq[0].sweep_negate_used)
                s_sq[0].enabled = 0;
            s_sq[0].sweep_negate = (value & 0x08) != 0;
            break;
        case 0x11: s_sq[0].length = (uint16_t)(value & 0x3F); break;
        case 0x12:
            s_sq[0].env_period    = (uint8_t)(value & 7);
            s_sq[0].env_direction = (value & 0x08) ? +1 : -1;
            s_sq[0].dac_on = dac_enabled_from_env(value);
            if (!s_sq[0].dac_on) s_sq[0].enabled = 0;
            break;
        case 0x13:
            s_sq[0].freq = sq_freq(0);
            {
                int32_t np0 = (2048 - s_sq[0].freq) * 4 + g_gbapu_sq_period_adj;
                if (g_gbapu_just_reloaded == 2 ||
                    (g_gbapu_just_reloaded == 1 && s_sq[0].just_reloaded))
                    s_sq[0].timer = np0;

                else if (g_gbapu_just_reloaded == 3 && s_sq[0].timer > np0)
                    s_sq[0].timer = np0;
            }
            break;
        case 0x14:
            sq_nrx4_backstep(0, prev_reg, value);
            s_sq[0].freq = sq_freq(0);
            s_sq[0].length_enabled = (value & 0x40) != 0;
            if (value & 0x80) trigger_square(0);
            break;

        case 0x16: s_sq[1].length = (uint16_t)(value & 0x3F); break;
        case 0x17:
            s_sq[1].env_period    = (uint8_t)(value & 7);
            s_sq[1].env_direction = (value & 0x08) ? +1 : -1;
            s_sq[1].dac_on = dac_enabled_from_env(value);
            if (!s_sq[1].dac_on) s_sq[1].enabled = 0;
            break;
        case 0x18:
            s_sq[1].freq = sq_freq(1);
            {
                int32_t np1 = (2048 - s_sq[1].freq) * 4 + g_gbapu_sq_period_adj;
                if (g_gbapu_just_reloaded == 2 ||
                    (g_gbapu_just_reloaded == 1 && s_sq[1].just_reloaded))
                    s_sq[1].timer = np1;

                else if (g_gbapu_just_reloaded == 3 && s_sq[1].timer > np1)
                    s_sq[1].timer = np1;
            }
            break;
        case 0x19:
            sq_nrx4_backstep(1, prev_reg, value);
            s_sq[1].freq = sq_freq(1);
            s_sq[1].length_enabled = (value & 0x40) != 0;
            if (value & 0x80) trigger_square(1);
            break;

        case 0x1A:
            s_wave.dac_on = (value & 0x80) != 0;
            if (!s_wave.dac_on) s_wave.enabled = 0;
            break;
        case 0x1B: s_wave.length = value; break;
        case 0x1C: s_wave.shift = (uint8_t)((value >> 5) & 3); break;
        case 0x1D: s_wave.freq = (uint16_t)(((s_reg[0x1E] & 7) << 8) | value); break;
        case 0x1E:
            s_wave.freq = (uint16_t)(((value & 7) << 8) | s_reg[0x1D]);
            s_wave.length_enabled = (value & 0x40) != 0;
            if (value & 0x80) trigger_wave();
            break;

        case 0x20: s_noise.length = (uint16_t)(value & 0x3F); break;
        case 0x21:
            s_noise.env_period    = (uint8_t)(value & 7);
            s_noise.env_direction = (value & 0x08) ? +1 : -1;
            s_noise.dac_on = dac_enabled_from_env(value);
            if (!s_noise.dac_on) s_noise.enabled = 0;
            break;
        case 0x22: {
            {

                int oc = prev_reg & 7, nc = value & 7;
                int nd = nc << 2; if (!nd) nd = 2;
                if (oc == 0 && nc != 0 && s_noise.bg_active)
                    s_noise.counter_cd += nd * 2;
            }
            s_noise.narrow = (value & 0x08) != 0;
            nr43_write_glitch(prev_reg, value);
            break;
        }
        case 0x23:
            s_noise.length_enabled = (value & 0x40) != 0;
            if (value & 0x80) {
                int al = (int)(((s_t_total >> 1) + (uint64_t)g_gbapu_noise_align) & 3);
                if (g_gbapu_dmg_delay && al != 0)
                    s_noise.dmg_delay = 12;
                else
                    trigger_noise();
            }
            break;
        default: break;
    }
}

void GbApu_StepT(uint32_t t) {

    s_noise_div_chunk = (s_reg[0x22] & 7) << 2;
    if (!s_noise_div_chunk) s_noise_div_chunk = 2;
    if (!s_power) {

        return;
    }

    while (t) {

        int32_t step = (int32_t)t;
        if (s_seq_timer < step) step = s_seq_timer;
        if (s_sq[0].enabled && s_sq[0].timer < step) step = s_sq[0].timer;
        if (s_sq[1].enabled && s_sq[1].timer < step) step = s_sq[1].timer;
        if (s_wave.enabled  && s_wave.timer  < step) step = s_wave.timer;
        if (s_noise.enabled && !g_gbapu_noise_counter && s_noise.timer < step)
            step = s_noise.timer;
        if (s_noise.bg_active && g_gbapu_noise_counter && s_noise.counter_cd < step)
            step = s_noise.counter_cd;
        if (s_sec_timer < step) step = s_sec_timer;
        if (s_env_defer_t >= 0 && s_env_defer_t < step) step = s_env_defer_t;
        if (s_noise.dmg_delay > 0 && s_noise.dmg_delay < step) step = s_noise.dmg_delay;
        if (s_sweep_defer_t >= 0 && s_sweep_defer_t < step) step = s_sweep_defer_t;
        if (step <= 0) step = 1;

        s_sec_timer -= step;
        if (s_sec_timer <= 0) { s_sec_timer += 8192; secondary_event(); }
        if (s_env_defer_t >= 0) {
            s_env_defer_t -= step;
            if (s_env_defer_t <= 0) { s_env_defer_t = -1; apply_secondary_event(); }
        }
        if (g_gbapu_sweep2) {
            if (s_sq[0].sw_restart_hold > 0) {
                s_sq[0].sw_restart_hold -= step;
                if (s_sq[0].sw_restart_hold < 0) s_sq[0].sw_restart_hold = 0;
            }
            s_sw_acc += step;
            while (s_sw_acc >= 4) { s_sw_acc -= 4; sw_step_unit(); }
        }
        if (s_noise.dmg_delay > 0) {
            s_noise.dmg_delay -= step;
            if (s_noise.dmg_delay <= 0) { s_noise.dmg_delay = 0; trigger_noise(); }
        }
        if (s_sweep_defer_t >= 0) {
            s_sweep_defer_t -= step;
            if (s_sweep_defer_t <= 0) { s_sweep_defer_t = -1; clock_sweep_apply(); }
        }
        s_seq_timer -= step;
        if (s_seq_timer <= 0) {
            s_seq_timer += 8192;
            frame_sequencer_step();
        }

        for (int i = 0; i < 2; i++) {

            if (!s_sq[i].enabled) continue;
            if (!s_sq[i].dac_on) continue;
            s_sq[i].timer -= step;
            while (s_sq[i].timer <= 0) {
                s_sq[i].timer += (2048 - s_sq[i].freq) * 4 + g_gbapu_sq_period_adj;
                s_sq[i].duty_pos = (uint8_t)((s_sq[i].duty_pos + 1) & 7);
                s_sq[i].did_tick = 1;
                if (i == 0) g_gbapu_ch1_reloads++;
            }

            s_sq[i].just_reloaded =
                (s_sq[i].timer == (2048 - s_sq[i].freq) * 4 + g_gbapu_sq_period_adj);
        }

        if (s_wave.enabled) {
            s_wave.timer -= step;
            while (s_wave.timer <= 0) {
                s_wave.timer += (2048 - s_wave.freq) * 2;
                s_wave.pos = (uint8_t)((s_wave.pos + 1) & 31);
                uint8_t b = s_reg[R_WAVE + (s_wave.pos >> 1)];
                s_wave.sample_buffer = (s_wave.pos & 1) ? (uint8_t)(b & 0x0F)
                                                        : (uint8_t)(b >> 4);
            }
        }

        if (s_noise.bg_active && g_gbapu_noise_counter) {

            if (g_gbapu_noise_entry_guard && s_noise.counter_cd == 0) {
                uint8_t nr43e = s_reg[0x22];
                int de = (nr43e & 7) << 2; if (!de) de = 2;
                s_noise.counter_cd = de * 2;
            }
            s_noise.counter_cd -= step;

            s_noise.countdown_reloaded = (s_noise.counter_cd == 0);
            while (s_noise.counter_cd <= 0) {
                uint8_t nr43 = s_reg[0x22];
                int div_apu = (nr43 & 7) << 2; if (!div_apu) div_apu = 2;
                if (g_gbapu_noise_div_chunk_on) div_apu = s_noise_div_chunk;

                if (g_gbapu_noise_div_latch && s_noise_div_prev >= 0 &&
                    s_noise_div_prev != div_apu) {
                    div_apu = s_noise_div_prev;
                }
                s_noise_div_prev = (nr43 & 7) << 2; if (!s_noise_div_prev) s_noise_div_prev = 2;
                s_noise.counter_cd += div_apu * 2;
                uint16_t mask = (uint16_t)(1u << (nr43 >> 4));
                int old_bit = (s_noise.counter & mask) != 0;
                s_noise.counter = (uint16_t)((s_noise.counter + 1) & 0x3FFF);
                s_noise.did_step_counter = 1;
                int new_bit = (s_noise.counter & mask) != 0;
                if (!(new_bit && !old_bit)) continue;
                if (!s_noise.enabled) continue;
                uint16_t x = (uint16_t)((s_noise.lfsr ^ (s_noise.lfsr >> 1)) & 1);
                s_noise.lfsr >>= 1;
                s_noise.lfsr = (uint16_t)(s_noise.lfsr | (x << 14));
                if (s_noise.narrow)
                    s_noise.lfsr = (uint16_t)((s_noise.lfsr & ~0x40u) | (x << 6));
            }
        }
        if (s_noise.enabled && !g_gbapu_noise_counter) {
            s_noise.timer -= step;
            while (s_noise.timer <= 0) {
                uint8_t nr43 = s_reg[0x22];
                s_noise.timer += kNoiseDivisor[nr43 & 7] << (nr43 >> 4);

                uint16_t x = (uint16_t)((s_noise.lfsr ^ (s_noise.lfsr >> 1)) & 1);
                s_noise.lfsr >>= 1;
                s_noise.lfsr = (uint16_t)(s_noise.lfsr | (x << 14));
                if (s_noise.narrow)
                    s_noise.lfsr = (uint16_t)((s_noise.lfsr & ~0x40u) | (x << 6));
            }
        }

        s_t_total += (uint64_t)step;
        t -= (uint32_t)step;
    }
}

void GbApu_ChannelSamples(uint8_t out[4], uint8_t active[4]) {
    for (int i = 0; i < 2; i++) {
        int on = s_sq[i].enabled && s_sq[i].dac_on;
        uint8_t duty = (uint8_t)((s_reg[0x11 + i * 5] >> 6) & 3);
        out[i]    = on ? (uint8_t)(kDuty[duty][s_sq[i].duty_pos] * s_sq[i].volume) : 0;
        active[i] = (uint8_t)on;
    }

    int won = s_wave.enabled && s_wave.dac_on;

    static const uint8_t kWaveShift[4] = { 4, 0, 1, 2 };
    out[2]    = won ? (uint8_t)(s_wave.sample_buffer >> kWaveShift[s_wave.shift]) : 0;
    active[2] = (uint8_t)won;

    int non = s_noise.enabled && s_noise.dac_on;
    out[3]    = non ? (uint8_t)((~s_noise.lfsr & 1) * s_noise.volume) : 0;
    active[3] = (uint8_t)non;
}

static uint32_t next_event_t(void) {
    int32_t m = s_seq_timer;
    if (s_sec_timer < m) m = s_sec_timer;
    if (s_sq[0].enabled && s_sq[0].timer < m) m = s_sq[0].timer;
    if (s_sq[1].enabled && s_sq[1].timer < m) m = s_sq[1].timer;
    if (s_wave.enabled  && s_wave.timer  < m) m = s_wave.timer;
    if (s_noise.enabled && s_noise.timer < m) m = s_noise.timer;
    return m > 0 ? (uint32_t)m : 1u;
}

static void mix_raw(float *left, float *right) {
    uint8_t sm[4], a[4];
    GbApu_ChannelSamples(sm, a);
    uint8_t nr51 = s_reg[0x25], nr50 = s_reg[0x24];
    float l = 0.0f, r = 0.0f;
    for (int i = 0; i < 4; i++) {
        if (g_gbapu_mute_mask & (1 << i)) continue;
        int dac_on = (i < 2) ? s_sq[i].dac_on : (i == 2 ? s_wave.dac_on : s_noise.dac_on);
        float v;
        if (dac_on) {
            v = (float)sm[i] / 7.5f - 1.0f;
            s_dac_level[i] = v;
        } else if (g_gbapu_dac_discharge > 0.0f) {
            s_dac_level[i] -= s_dac_level[i] * g_gbapu_dac_discharge;
            v = s_dac_level[i];
        } else {
            s_dac_level[i] = 0.0f;
            continue;
        }

        v *= g_gbapu_ch_gain[i];
        if (nr51 & (1 << i))       r += v;
        if (nr51 & (1 << (i + 4))) l += v;
    }
    *left  = l * (float)(((nr50 >> 4) & 7) + 1) / 8.0f / 4.0f;
    *right = r * (float)((nr50 & 7) + 1) / 8.0f / 4.0f;
}

void GbApu_RenderSamples(float *out, int n) {

    static uint32_t carry;
    const uint32_t step_fp = (uint32_t)(((uint64_t)GB_APU_CLOCK << 16) / s_out_rate);

    for (int i = 0; i < n; i++) {
        uint32_t want_fp = step_fp + carry;
        uint32_t whole   = want_fp >> 16;
        carry            = want_fp & 0xFFFF;

        double accl = 0.0, accr = 0.0;
        uint32_t done = 0;
        while (done < whole) {
            uint32_t slice = next_event_t();
            if (slice > whole - done) slice = whole - done;
            if (slice == 0) slice = 1;

            float l, r;
            mix_raw(&l, &r);
            accl += (double)l * slice;
            accr += (double)r * slice;

            GbApu_StepT(slice);
            done += slice;
        }

        float l = (float)(accl / (double)(whole ? whole : 1));
        float r = (float)(accr / (double)(whole ? whole : 1));

        if (g_gbapu_lp_cutoff > 0.0f) {

            float a = g_gbapu_lp_cutoff;
            if (a > 1.0f) a = 1.0f;

            s_lp_l  += a * (l - s_lp_l);
            s_lp_r  += a * (r - s_lp_r);
            s_lp_l2 += a * (s_lp_l - s_lp_l2);
            s_lp_r2 += a * (s_lp_r - s_lp_r2);
            l = s_lp_l2; r = s_lp_r2;
        }

        float ol = l - s_hp_l, orr = r - s_hp_r;
        s_hp_l = l - ol * s_hp_coeff;
        s_hp_r = r - orr * s_hp_coeff;
        out[i * 2 + 0] = ol;
        out[i * 2 + 1] = orr;
    }
}

void GbApu_Mix(float *left, float *right) {
    uint8_t s[4], a[4];
    GbApu_ChannelSamples(s, a);

    uint8_t nr51 = s_reg[0x25];
    uint8_t nr50 = s_reg[0x24];

    float l = 0.0f, r = 0.0f;
    for (int i = 0; i < 4; i++) {

        int dac_on = (i < 2) ? s_sq[i].dac_on : (i == 2 ? s_wave.dac_on : s_noise.dac_on);

        float target = dac_on ? ((float)s[i] / 7.5f - 1.0f) : 0.0f;
        if (dac_on || g_gbapu_dac_discharge <= 0.0f) {
            s_dac_level[i] = target;
        }
        else {
            s_dac_level[i] += (target - s_dac_level[i]) * g_gbapu_dac_discharge;
        }
        float v = s_dac_level[i];
        if (nr51 & (1 << i))       r += v;
        if (nr51 & (1 << (i + 4))) l += v;
    }

    l *= (float)(((nr50 >> 4) & 7) + 1) / 8.0f / 4.0f;
    r *= (float)((nr50 & 7) + 1) / 8.0f / 4.0f;

    float ol = l - s_hp_l, orr = r - s_hp_r;
    s_hp_l = l - ol * s_hp_coeff;
    s_hp_r = r - orr * s_hp_coeff;

    *left = ol;
    *right = orr;
}
