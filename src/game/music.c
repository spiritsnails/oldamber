
#include "music.h"
#include "audio_engine.h"
#include "music_ids.h"
#include <string.h>
#include "../platform/audio.h"

#include "../data/music_types.h"
#include "assetpack_bind.h"
#include <stddef.h>

typedef struct {
    const ch_data_t *data;
    int              pos;
    int              delay;

    uint16_t vib_base_freq;
    uint8_t  vib_delay_left;
    uint8_t  vib_rate;
    int      vib_rate_left;
    int      vib_phase;

    int      slide_active;
    uint8_t  slide_decreasing;
    uint8_t  slide_cur_lo;
    uint8_t  slide_cur_hi;
    uint8_t  slide_target_lo;
    uint8_t  slide_target_hi;
    uint8_t  slide_step_int;
    uint8_t  slide_step_frac;
    uint8_t  slide_cur_frac;

    const drum_inst_t *drum_inst;
    int      drum_step;
    int      drum_timer;
} ch_seq_t;

static ch_seq_t gSeq[4];
static uint8_t  gCurrentMusic   = MUSIC_NONE;
static int      g_skip_update    = 0;
static uint8_t  g_suspend_count[4] = {0, 0, 0, 0};
static uint8_t  g_fade_state = 0;
static uint8_t  g_fade_target_music = MUSIC_NONE;

static uint8_t  g_fade_vol = 7;
static uint8_t  g_fade_wait = 10;

static const song_t kSongs[] = {
               { { NULL,            NULL,            NULL            } },
        { { &kPalletTown_Ch1,&kPalletTown_Ch2,&kPalletTown_Ch3} },
         { { &kPokecenter_Ch1,&kPokecenter_Ch2,&kPokecenter_Ch3} },
                { { &kGym_Ch1,       &kGym_Ch2,       &kGym_Ch3       } },
            { { &kCities1_Ch1,   &kCities1_Ch2,   &kCities1_Ch3   } },
            { { &kCities2_Ch1,   &kCities2_Ch2,   &kCities2_Ch3   } },
            { { &kCeladon_Ch1,   &kCeladon_Ch2,   &kCeladon_Ch3   } },
           { { &kCinnabar_Ch1,  &kCinnabar_Ch2,  &kCinnabar_Ch3  } },
          { { &kVermilion_Ch1, &kVermilion_Ch2, &kVermilion_Ch3 } },
           { { &kLavender_Ch1,  &kLavender_Ch2,  &kLavender_Ch3  } },
            { { &kSSAnne_Ch1,    &kSSAnne_Ch2,    &kSSAnne_Ch3    } },
            { { &kRoutes1_Ch1,   &kRoutes1_Ch2,   &kRoutes1_Ch3   } },
            { { &kRoutes2_Ch1,   &kRoutes2_Ch2,   &kRoutes2_Ch3   } },
            { { &kRoutes3_Ch1,   &kRoutes3_Ch2,   &kRoutes3_Ch3   } },
            { { &kRoutes4_Ch1,   &kRoutes4_Ch2,   &kRoutes4_Ch3,   &kRoutes4_Ch4 } },
     { { &kIndigoPlateau_Ch1, &kIndigoPlateau_Ch2, &kIndigoPlateau_Ch3, &kIndigoPlateau_Ch4 } },
           { { &kOaksLab_Ch1,   &kOaksLab_Ch2,   &kOaksLab_Ch3   } },
           { { &kDungeon1_Ch1,  &kDungeon1_Ch2,  &kDungeon1_Ch3  } },
           { { &kDungeon2_Ch1,  &kDungeon2_Ch2,  &kDungeon2_Ch3  } },
           { { &kDungeon3_Ch1,  &kDungeon3_Ch2,  &kDungeon3_Ch3  } },
      { { &kPokemonTower_Ch1,&kPokemonTower_Ch2,&kPokemonTower_Ch3} },
           { { &kSilphCo_Ch1,   &kSilphCo_Ch2,   &kSilphCo_Ch3   } },
        { { &kSafariZone_Ch1,&kSafariZone_Ch2,&kSafariZone_Ch3} },
              { { &kTitleScreen_Ch1,&kTitleScreen_Ch2,&kTitleScreen_Ch3,&kTitleScreen_Ch4 } },
         { { &kJigglypuffSong_Ch1, &kJigglypuffSong_Ch2, NULL               } },
             { { &kWildBattle_Ch1,        &kWildBattle_Ch2,        &kWildBattle_Ch3        } },
       { { &kDefeatedWildMon_Ch1,   &kDefeatedWildMon_Ch2,   &kDefeatedWildMon_Ch3   } },
        { { &kDefeatedTrainer_Ch1,   &kDefeatedTrainer_Ch2,   &kDefeatedTrainer_Ch3   } },
     { { &kDefeatedGymLeader_Ch1, &kDefeatedGymLeader_Ch2, &kDefeatedGymLeader_Ch3 } },
            { { &kPkmnHealed_Ch1,        &kPkmnHealed_Ch2,        &kPkmnHealed_Ch3        } },
      { { &kGymLeaderBattle_Ch1,   &kGymLeaderBattle_Ch2,   &kGymLeaderBattle_Ch3   } },
         { { &kTrainerBattle_Ch1,     &kTrainerBattle_Ch2,     &kTrainerBattle_Ch3     } },
             { { &kMeetRival_Ch1,         &kMeetRival_Ch2,         &kMeetRival_Ch3         } },
      { { &kMeetMaleTrainer_Ch1,   &kMeetMaleTrainer_Ch2,   &kMeetMaleTrainer_Ch3   } },
    { { &kMeetFemaleTrainer_Ch1, &kMeetFemaleTrainer_Ch2, &kMeetFemaleTrainer_Ch3 } },
             { { &kMuseumGuy_Ch1,         &kMuseumGuy_Ch2,         &kMuseumGuy_Ch3         } },
      { { &kMeetEvilTrainer_Ch1,   &kMeetEvilTrainer_Ch2,   &kMeetEvilTrainer_Ch3   } },
                { { &kSurfing_Ch1,           &kSurfing_Ch2,           &kSurfing_Ch3           } },
         { { &kMeetProfOak_Ch1,       &kMeetProfOak_Ch2,       &kMeetProfOak_Ch3       } },
          { { &kIntroBattle_Ch1,       &kIntroBattle_Ch2,       &kIntroBattle_Ch3,      &kIntroBattle_Ch4 } },
           { { &kGameCorner_Ch1,        &kGameCorner_Ch2,        &kGameCorner_Ch3        } },
           { { &kBikeRiding_Ch1,        &kBikeRiding_Ch2,        &kBikeRiding_Ch3        } },
      { { &kCinnabarMansion_Ch1,   &kCinnabarMansion_Ch2,   &kCinnabarMansion_Ch3,   &kCinnabarMansion_Ch4 } },
          { { &kFinalBattle_Ch1,       &kFinalBattle_Ch2,       &kFinalBattle_Ch3        } },
          { { &kHallOfFame_Ch1,        &kHallOfFame_Ch2,        &kHallOfFame_Ch3         } },
               { { &kCredits_Ch1,           &kCredits_Ch2,           &kCredits_Ch3            } },
             { { NULL,                    NULL,                    &kPokeflute_Ch3          } },
};
#define NUM_SONGS  ((int)(sizeof(kSongs)/sizeof(kSongs[0])))

static void reset_vib(ch_seq_t *seq, const note_evt_t *n) {
    seq->vib_base_freq  = n->freq;
    seq->vib_delay_left = n->vib_delay;
    seq->vib_rate       = n->vib_rate ? n->vib_rate : 1;
    seq->vib_rate_left  = seq->vib_rate;
    seq->vib_phase      = 0;
}

static void reset_slide(ch_seq_t *seq, const note_evt_t *n) {
    seq->slide_active = 0;
    seq->slide_decreasing = 0;
    seq->slide_cur_lo = (uint8_t)(n->freq & 0xFF);
    seq->slide_cur_hi = (uint8_t)((n->freq >> 8) & 0x07);
    seq->slide_target_lo = (uint8_t)(n->slide_target & 0xFF);
    seq->slide_target_hi = (uint8_t)((n->slide_target >> 8) & 0x07);
    seq->slide_step_int = 0;
    seq->slide_step_frac = 0;
    seq->slide_cur_frac = 0;
    if (n->freq == 0 || n->slide_target == 0 || n->slide_frames == 0) return;
    if (n->freq == n->slide_target) return;
    seq->slide_active = 1;

    {
        uint8_t divisor = n->slide_frames;
        if (divisor == 0) divisor = 1;

        uint8_t d = seq->slide_cur_hi;
        uint8_t e = seq->slide_cur_lo;
        uint8_t th = seq->slide_target_hi;
        uint8_t tl = seq->slide_target_lo;
        uint8_t diff_hi = 0;
        uint8_t diff_lo = 0;

        {
            uint16_t cur = ((uint16_t)d << 8) | e;
            uint16_t tgt = ((uint16_t)th << 8) | tl;
            if (cur >= tgt) {
                seq->slide_decreasing = 1;
                diff_hi = (uint8_t)((cur - tgt) >> 8);
                diff_lo = (uint8_t)(cur - tgt);
            } else {

                seq->slide_decreasing = 0;
                {
                    uint8_t lo = (uint8_t)(tl - e);
                    uint8_t borrow = (tl < e) ? 1 : 0;
                    uint8_t d_bug = (uint8_t)(d - borrow);
                    uint8_t hi = (uint8_t)(th - d_bug);
                    diff_hi = hi;
                    diff_lo = lo;
                }
            }
        }

        {
            uint8_t b = 0;
            uint8_t dh = diff_hi;
            uint8_t el = diff_lo;
            for (;;) {
                b++;
                if (el >= divisor) {
                    el = (uint8_t)(el - divisor);
                    continue;
                }
                if (dh == 0) break;
                dh--;
                continue;
            }
            seq->slide_step_int = b;
            seq->slide_step_frac = el;
            seq->slide_cur_frac = el;
        }
    }
}

static void set_music_mix_immediate(uint8_t level) {
    Audio_SetMixVolumeImmediate(level);
}

static void tick_vibrato(int c, ch_seq_t *seq) {
    const note_evt_t *n = &seq->data->notes[seq->pos];
    if (n->vib_depth == 0 || seq->vib_base_freq == 0) return;
    if (seq->vib_delay_left > 0) { seq->vib_delay_left--; return; }
    if (--seq->vib_rate_left > 0) return;
    seq->vib_phase    ^= 1;
    seq->vib_rate_left = seq->vib_rate;
    int16_t  delta  = seq->vib_phase ? -(int16_t)n->vib_depth : (int16_t)n->vib_depth;
    uint16_t af     = (uint16_t)((int16_t)seq->vib_base_freq + delta) & 0x7FFu;
    if (g_suspend_count[c] > 0) return;
    Audio_WriteReg(c, 3, (uint8_t)(af & 0xFF));
    Audio_WriteReg(c, 4, (uint8_t)((af >> 8) & 0x07));
}

static void tick_pitch_slide(int c, ch_seq_t *seq) {
    if (!seq->slide_active) return;
    {
        uint8_t d = seq->slide_cur_hi;
        uint8_t e = seq->slide_cur_lo;
        uint8_t th = seq->slide_target_hi;
        uint8_t tl = seq->slide_target_lo;
        int reached = 0;

        if (!seq->slide_decreasing) {

            {
                uint16_t v = (((uint16_t)d << 8) | e) + seq->slide_step_int;
                d = (uint8_t)(v >> 8);
                e = (uint8_t)v;
            }
            {
                uint16_t s = (uint16_t)seq->slide_step_frac + (uint16_t)seq->slide_cur_frac;
                seq->slide_cur_frac = (uint8_t)s;
                if (s > 0xFF) {
                    uint16_t v = (((uint16_t)d << 8) | e) + 1;
                    d = (uint8_t)(v >> 8);
                    e = (uint8_t)v;
                }
            }
            if (d > th || (d == th && e > tl))
                reached = 1;
        } else {

            {
                uint16_t cur = ((uint16_t)d << 8) | e;
                cur = (uint16_t)(cur - seq->slide_step_int);
                d = (uint8_t)(cur >> 8);
                e = (uint8_t)cur;
            }
            {

                uint16_t dbl = (uint16_t)seq->slide_step_frac << 1;
                seq->slide_step_frac = (uint8_t)dbl;
                if (dbl > 0xFF) {
                    uint16_t cur = ((uint16_t)d << 8) | e;
                    cur = (uint16_t)(cur - 1);
                    d = (uint8_t)(cur >> 8);
                    e = (uint8_t)cur;
                }
            }
            if (d < th || (d == th && e < tl))
                reached = 1;
        }

        if (reached) {
            seq->slide_active = 0;
            d = th;
            e = tl;
        }

        seq->slide_cur_hi = d;
        seq->slide_cur_lo = e;
    }

    uint16_t f = (((uint16_t)seq->slide_cur_hi << 8) | seq->slide_cur_lo) & 0x07FFu;
    seq->vib_base_freq = f;
    if (g_suspend_count[c] > 0) return;
    Audio_WriteReg(c, 3, (uint8_t)(f & 0xFF));
    Audio_WriteReg(c, 4, (uint8_t)((f >> 8) & 0x07));
}

static void drum_fire_step(int c, ch_seq_t *seq) {
    if (!seq->drum_inst) return;
    if (seq->drum_step < 0 || seq->drum_step >= seq->drum_inst->count) return;
    const drum_step_t *st = &seq->drum_inst->steps[seq->drum_step];
    if (g_suspend_count[c] > 0) {
        seq->drum_timer = st->frames;
        return;
    }
    Audio_WriteReg(3, 2, st->env_byte);
    Audio_WriteReg(3, 3, st->nr43);
    Audio_WriteReg(3, 4, 0x80);
    seq->drum_timer = st->frames;
}

static void drum_start(ch_seq_t *seq, int inst_id) {
    seq->drum_inst = NULL;
    seq->drum_step = -1;
    seq->drum_timer = 0;
    if (inst_id <= 0 || inst_id >= 20) return;
    if (kDrumInst[inst_id].count == 0) return;
    seq->drum_inst = &kDrumInst[inst_id];
    seq->drum_step = 0;
}

static void tick_drum(int c, ch_seq_t *seq) {
    if (!seq->drum_inst) return;
    if (seq->drum_step < 0 || seq->drum_step >= seq->drum_inst->count) {
        seq->drum_inst = NULL;
        seq->drum_step = -1;
        seq->drum_timer = 0;
        return;
    }
    if (seq->drum_timer <= 0) {
        drum_fire_step(c, seq);
        return;
    }
    if (--seq->drum_timer <= 0) {
        seq->drum_step++;
        if (seq->drum_step >= seq->drum_inst->count) {
            seq->drum_inst = NULL;
            seq->drum_step = -1;
            seq->drum_timer = 0;
        }
    }
}

static void reset_drum(int c, ch_seq_t *seq, const note_evt_t *n) {
    drum_start(seq, n->duty);
    if (!seq->drum_inst) return;

    const drum_step_t *st0 = &seq->drum_inst->steps[0];
    if (g_suspend_count[c] == 0) {
        Audio_WriteReg(3, 2, st0->env_byte);
        Audio_WriteReg(3, 3, st0->nr43);
        Audio_WriteReg(3, 4, 0x80);
    }
    seq->drum_timer = st0->frames;
    seq->drum_step = 1;
}

static void fire_note(int ch, const note_evt_t *n) {
    if (ch == 3) {

        return;
    }

    if (n->freq > 0) {

        if (ch == 2) Audio_SetWaveInstrument(n->duty);

        Audio_WriteReg(ch, 1, (uint8_t)((n->duty << 6) | 0x3F));

        Audio_WriteReg(ch, 2, n->env_byte);

        Audio_WriteReg(ch, 3, (uint8_t)(n->freq & 0xFF));

        Audio_WriteReg(ch, 4, (uint8_t)(((n->freq >> 8) & 0x07) | 0x80));
    } else {

        Audio_WriteReg(ch, 2, 0x00);
        Audio_WriteReg(ch, 4, 0x00);
    }
}

#include "johto_music.h"

static void yield_other_engine(void) {
    if (JohtoMusic_IsPlaying()) JohtoMusic_Stop();
}

void Music_Play(uint8_t music_id) {
    yield_other_engine();
    if (music_id == gCurrentMusic) return;
    g_fade_state = 0;
    g_fade_target_music = MUSIC_NONE;
    g_fade_vol = 7;
    g_fade_wait = 10;
    Music_Stop();
    set_music_mix_immediate(15);
    if (music_id == MUSIC_NONE || music_id >= NUM_SONGS) return;

    gCurrentMusic = music_id;

    if (music_id < (int)(sizeof kMusicRom / sizeof kMusicRom[0])) {
        music_rom_t m = kMusicRom[music_id];
        if (m.engine != 0xFF) {
            AudioEngine_PlayMusic(m.rom_id, m.engine);
            g_skip_update = 1;
            return;
        }
    }

    const song_t *s = &kSongs[music_id];

    for (int c = 0; c < 4; c++) {
        const ch_data_t *d = s->ch[c];
        if (!d || d->count == 0) {
            gSeq[c].data = NULL;
            continue;
        }
        gSeq[c].data  = d;
        gSeq[c].pos   = 0;
        gSeq[c].delay = 0;

        fire_note(c, &d->notes[gSeq[c].pos]);
        reset_vib(&gSeq[c], &d->notes[gSeq[c].pos]);
        reset_slide(&gSeq[c], &d->notes[gSeq[c].pos]);
        if (c == 3) reset_drum(c, &gSeq[c], &d->notes[gSeq[c].pos]);
        gSeq[c].delay = d->notes[gSeq[c].pos].frames;
    }

    g_skip_update = 1;
}

#define MEETRIVAL_CH1_ALT_START            0x71A2u
#define MEETRIVAL_CH2_ALT_START            0x721Du
#define MEETRIVAL_CH3_ALT_START            0x72B5u
#define MEETRIVAL_CH1_ALT_TEMPO            0x7119u
#define MEETRIVAL_CH1_ALT_START_AND_TEMPO  0x719Bu
#define CITIES1_CH1_ALT_TEMPO              0x6A6Fu

void Music_PlayCities1AlternateTempo(void) {

    Music_Play(MUSIC_CITIES1);
    if (gCurrentMusic != MUSIC_CITIES1) return;
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN1, CITIES1_CH1_ALT_TEMPO);
}

void Music_PlayRivalAlternateStart(void) {

    Music_Play(MUSIC_MEET_RIVAL);
    if (gCurrentMusic != MUSIC_MEET_RIVAL) return;
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN1, MEETRIVAL_CH1_ALT_START);
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN2, MEETRIVAL_CH2_ALT_START);
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN3, MEETRIVAL_CH3_ALT_START);
}

void Music_PlayRivalAlternateTempo(void) {

    Music_Play(MUSIC_MEET_RIVAL);
    if (gCurrentMusic != MUSIC_MEET_RIVAL) return;
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN1, MEETRIVAL_CH1_ALT_TEMPO);
}

void Music_PlayRivalAlternateStartAndTempo(void) {

    Music_PlayRivalAlternateStart();
    if (gCurrentMusic != MUSIC_MEET_RIVAL) return;
    AudioEngine_OverwriteChannelPointer(AUDIO_CHAN1, MEETRIVAL_CH1_ALT_START_AND_TEMPO);
}

void Music_PlayDefaultFadeOutCurrent(uint8_t music_id) {
    if (music_id == MUSIC_NONE || music_id >= NUM_SONGS) return;
    yield_other_engine();
    if (music_id == gCurrentMusic) {

        g_fade_state = 0;
        g_fade_target_music = MUSIC_NONE;
        g_fade_vol = 7;
        g_fade_wait = 10;
        set_music_mix_immediate(15);
        return;
    }
    if (g_fade_state != 0 && music_id == g_fade_target_music) return;
    g_fade_target_music = music_id;
    if (gCurrentMusic == MUSIC_NONE) {
        Music_Play(music_id);
        return;
    }
    g_fade_state = 1;
    g_fade_vol = 7;
    g_fade_wait = 10;
    set_music_mix_immediate(15);
}

void Music_PlayFromLoop(uint8_t music_id) {
    yield_other_engine();

    Music_Stop();
    if (music_id == MUSIC_NONE || music_id >= NUM_SONGS) return;

    gCurrentMusic = music_id;
    const song_t *s = &kSongs[music_id];

    for (int c = 0; c < 4; c++) {
        const ch_data_t *d = s->ch[c];
        if (!d || d->count == 0) {
            gSeq[c].data = NULL;
            continue;
        }
        gSeq[c].data  = d;
        gSeq[c].pos   = d->loop_start;
        gSeq[c].delay = 0;
        fire_note(c, &d->notes[gSeq[c].pos]);
        reset_vib(&gSeq[c], &d->notes[gSeq[c].pos]);
        reset_slide(&gSeq[c], &d->notes[gSeq[c].pos]);
        if (c == 3) reset_drum(c, &gSeq[c], &d->notes[gSeq[c].pos]);
        gSeq[c].delay = d->notes[gSeq[c].pos].frames;
    }
    g_skip_update = 1;
}

uint8_t Music_CurrentId(void) { return gCurrentMusic; }

void Music_Stop(void) {
    gCurrentMusic    = MUSIC_NONE;
    g_skip_update    = 0;
    g_fade_state     = 0;
    g_fade_target_music = MUSIC_NONE;
    g_fade_vol = 7;
    g_fade_wait = 10;
    set_music_mix_immediate(15);
    g_suspend_count[0] = 0;
    g_suspend_count[1] = 0;
    g_suspend_count[2] = 0;
    g_suspend_count[3] = 0;
    for (int c = 0; c < 4; c++) {
        gSeq[c].data = NULL;
        gSeq[c].drum_inst = NULL;
        gSeq[c].drum_step = -1;
        gSeq[c].drum_timer = 0;
    }

    if (AudioEngine_IsReady()) {
        AudioEngine_PlaySound(0xFF);
        return;
    }
    for (int c = 0; c < 4; c++) {
        Audio_WriteReg(c, 2, 0x00);
        Audio_WriteReg(c, 4, 0x00);
    }
}

void Music_Update(void) {

    if (g_skip_update) { g_skip_update = 0; return; }

    for (int c = 0; c < 4; c++) {
        ch_seq_t *seq = &gSeq[c];
        if (!seq->data) continue;

        tick_pitch_slide(c, seq);
        if (c == 3) tick_drum(c, seq);

        if (c != 3 && !seq->slide_active)
            tick_vibrato(c, seq);

        if (--seq->delay > 0) continue;

        seq->pos++;
        if (seq->pos >= seq->data->count) {
            int ls = seq->data->loop_start;
            if (ls >= 0)
                seq->pos = ls;
            else {
                seq->data = NULL;
                Audio_WriteReg(c, 2, 0x00);
                continue;
            }
        }

        const note_evt_t *n = &seq->data->notes[seq->pos];
        seq->delay = n->frames;
        reset_vib(seq, n);
        reset_slide(seq, n);
        if (c == 3) reset_drum(c, seq, n);

        if (g_suspend_count[c] == 0 && c != 3)
            fire_note(c, n);
    }

    if (g_fade_state == 1) {
        if (g_fade_wait > 0) {
            g_fade_wait--;
            return;
        }
        g_fade_wait = 10;
        if (g_fade_vol > 0) {
            g_fade_vol--;
            set_music_mix_immediate((uint8_t)((g_fade_vol * 15) / 7));
            return;
        }
        {
            uint8_t target = g_fade_target_music;
            g_fade_state = 0;
            g_fade_target_music = MUSIC_NONE;
            Music_Play(target);
        }
    }
}

void Music_SuspendChannel(int c) {
    if (c < 0 || c >= 4) return;
    if (g_suspend_count[c] < 0xFF) g_suspend_count[c]++;

    Audio_WriteReg(c, 2, 0x00);
    Audio_WriteReg(c, 4, 0x00);
}

void Music_ResumeChannel(int c) {
    if (c < 0 || c >= 4) return;
    if (g_suspend_count[c] == 0) return;
    g_suspend_count[c]--;
    if (g_suspend_count[c] > 0) return;
    if (gSeq[c].data) {

        if (c == 3)
            reset_drum(c, &gSeq[c], &gSeq[c].data->notes[gSeq[c].pos]);
        else
            fire_note(c, &gSeq[c].data->notes[gSeq[c].pos]);
    } else

        Audio_WriteReg(c, 2, 0x00);
}

int Music_IsChannelSuspended(int c) {
    if (c < 0 || c >= 4) return 0;
    return g_suspend_count[c] > 0;
}

int Music_IsPlaying(void) {

    if (AudioEngine_IsMusicPlaying()) return 1;
    for (int c = 0; c < 4; c++)
        if (gSeq[c].data) return 1;
    return 0;
}

uint8_t Music_GetMapID(uint8_t map_id) {

    if (map_id >= kMapMusicID_count)
        return MUSIC_NONE;
    return kMapMusicID[map_id];
}

uint8_t KantoMusic_ForTrackName(const char *track) {
    static const struct { const char *name; uint8_t id; } tbl[] = {
        { "pallet_town", MUSIC_PALLET_TOWN },

        { "celadon", MUSIC_CELADON }, { "cinnabar", MUSIC_CINNABAR },
        { "vermilion", MUSIC_VERMILION }, { "lavender", MUSIC_LAVENDER },
        { "cities1", MUSIC_CITIES1 }, { "cities2", MUSIC_CITIES2 },
        { "routes1", MUSIC_ROUTES1 }, { "routes2", MUSIC_ROUTES2 },
        { "routes3", MUSIC_ROUTES3 }, { "routes4", MUSIC_ROUTES4 },
        { "pokecenter", MUSIC_POKECENTER },
        { "gym", MUSIC_GYM },
        { "safari_zone", MUSIC_SAFARI_ZONE },
        { "oaks_lab", MUSIC_OAKS_LAB },
        { "dungeon1", MUSIC_DUNGEON1 }, { "dungeon2", MUSIC_DUNGEON2 },
        { "dungeon3", MUSIC_DUNGEON3 },
        { "pokemontower", MUSIC_POKEMON_TOWER },
        { "indigo_plateau", MUSIC_INDIGO_PLATEAU },
        { "ss_anne", MUSIC_SS_ANNE },
        { "game_corner", MUSIC_GAME_CORNER },
        { "cinnabar_mansion", MUSIC_CINNABAR_MANSION },
        { "silph_co", MUSIC_SILPH_CO },
    };
    if (!track) return MUSIC_NONE;
    for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
        if (strcmp(track, tbl[i].name) == 0) return tbl[i].id;
    return MUSIC_NONE;
}
