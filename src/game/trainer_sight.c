
#include "trainer_sight.h"
#include "npc.h"
#include "text.h"
#include "overworld.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/event_data.h"
#include "../data/trainer_sprites.h"
#include "../data/map_data.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "music.h"
#include "johto_music.h"
#include "johto_trainers.h"
#include "constants.h"
#include <stddef.h>
#include <stdio.h>

#define EMOTE_TILE_BASE  68

static int s_emote_active   = 0;
static int s_emote_on_player = 0;
static int s_emote_npc_idx   = -1;

static int s_emote_which     = EMOTE_SHOCK;

static void emote_load_tiles(void) {
    const uint8_t *src = (s_emote_which == EMOTE_SMILE) ? kHappyEmoteTiles
                                                        : kShockEmoteTiles;
    for (int i = 0; i < 4; i++)
        Display_LoadSpriteTile(EMOTE_TILE_BASE + i, src + i * 16);
}

typedef enum {
    TS_IDLE    = 0,
    TS_SPOTTED,
    TS_WALKING,
    TS_TALKING,
    TS_READY,
} TrainerSightState;

static TrainerSightState ts_state   = TS_IDLE;
static int               ts_npc_idx = -1;
static int               ts_map_trainer_idx = -1;
static int               ts_timer   = 0;

static int ts_ambush_pending_jingle = 0;

static int ts_ready_delay = 0;

uint8_t gEngagedTrainerClass = 0;
uint8_t gEngagedTrainerNo    = 0;

uint16_t gEngagedJohtoParty  = 0;
const char *gTrainerAfterText = NULL;

static int ts_walk_map    = -1;
static int ts_walk_npc    = -1;
static int ts_walk_x      = 0;
static int ts_walk_y      = 0;
static int ts_walk_facing = 0;

static int is_evil_trainer(uint8_t tc) {
    static const uint8_t kEvil[] = {
        OPP_JUGGLER_X - OPP_ID_OFFSET,
        OPP_GAMER     - OPP_ID_OFFSET,
        OPP_ROCKER    - OPP_ID_OFFSET,
        OPP_JUGGLER   - OPP_ID_OFFSET,
        OPP_CHIEF     - OPP_ID_OFFSET,
        OPP_SCIENTIST - OPP_ID_OFFSET,
        OPP_GIOVANNI  - OPP_ID_OFFSET,
        OPP_ROCKET    - OPP_ID_OFFSET,
        0xFF
    };
    for (int i = 0; kEvil[i] != 0xFF; i++)
        if (tc == kEvil[i]) return 1;
    return 0;
}

static void play_trainer_encounter_music(uint8_t trainer_class,
                                         uint16_t johto_party) {

    if (johto_party) {
        int idx = (int)johto_party - 1;
        if (idx >= 0 && idx < JOHTO_TRAINER_COUNT) {
            uint8_t cls = gJohtoTrainers[idx].class_id;
            if (cls < CRYSTAL_NUM_TRAINER_CLASSES) {
                uint8_t jtrack = gCrystalTrainerEncounterMusic[cls];
                printf("[trainer] encounter music: johto class_id=%d -> track=%d (%s)\n",
                       cls, jtrack, gCrystalMusic[jtrack].name);

                if (Music_IsPlaying()) Music_Stop();
                JohtoMusic_Play((johto_music_id_t)jtrack);
                return;
            }
        }
    }
    uint8_t track = is_evil_trainer(trainer_class) ? MUSIC_MEET_EVIL_TRAINER : MUSIC_MEET_MALE_TRAINER;
    printf("[trainer] play_trainer_encounter_music: class=%d track=%d\n", trainer_class, track);
    if (JohtoMusic_IsPlaying()) JohtoMusic_Stop();
    Music_Play(track);
}

void Trainer_PlayEncounterMusic(uint8_t trainer_class) {

    play_trainer_encounter_music(trainer_class, 0);
}

extern uint8_t    gGymTrainerBattlePending;
extern uint32_t   gGymTrainerVictoryEvent;
extern const char *gGymTrainerEndText;

static char s_defeat_text_buf[PKS_MAX_TEXT + 40];
void Trainer_SetDefeatText(int trainer_class, const char *raw) {
    const char *name;
    if (trainer_class == 0x19 || trainer_class == 0x2A || trainer_class == 0x2B)
        name = "{RIVAL}";
    else
        name = (trainer_class >= 1 && trainer_class <= NUM_TRAINERS)
                   ? gTrainerClassNames[trainer_class - 1] : NULL;
    if (name && name[0] && raw && raw[0])
        snprintf(s_defeat_text_buf, sizeof(s_defeat_text_buf), "%s: %s", name, raw);
    else
        snprintf(s_defeat_text_buf, sizeof(s_defeat_text_buf), "%s", raw ? raw : "");
    gTrainerAfterText = s_defeat_text_buf;
}
static void ts_set_defeat_text(int trainer_class, const char *raw) {
    Trainer_SetDefeatText(trainer_class, raw);
}

static int trainer_can_see_player(int tx, int ty, int facing, int sight_dist) {
    int px = (int)wXCoord;
    int py = (int)wYCoord;
    int dx = px - tx;
    int dy = py - ty;
    int dist;

    if (facing == 0 || facing == 1) {

        if (dx != 0) return 0;
        if (dy == 0) return 0;
        dist = dy < 0 ? -dy : dy;
        if (dist > sight_dist) return 0;

        if (facing == 0 && dy <= 0) return 0;

        if (facing == 1 && dy > 0) return 0;
    } else {

        if (dy != 0) return 0;
        if (dx == 0) return 0;
        dist = dx < 0 ? -dx : dx;
        if (dist > sight_dist) return 0;

        if (facing == 2 && dx > 0) return 0;

        if (facing == 3 && dx <= 0) return 0;
    }

    {
        const int on_vmap = AmberScript_IsEnabled() &&
                            wCurMap >= PKS_VIRTUAL_MAP_FIRST &&
                            wCurMap <= PKS_VIRTUAL_MAP_LAST;
        if (on_vmap) {
            const int sx = (dx > 0) - (dx < 0);
            const int sy = (dy > 0) - (dy < 0);
            int cx = tx + sx;
            int cy = ty + sy;
            int dirs;

            while (cx != px || cy != py) {

                if (AmberScript_GetLedgeOverrideAt(cx * 2, cy * 2 + 1, &dirs))
                    return 0;
                cx += sx;
                cy += sy;
            }
        }
    }

    return 1;
}

void Trainer_LoadMap(void) {
    ts_state           = TS_IDLE;
    ts_npc_idx         = -1;
    ts_map_trainer_idx = -1;
    ts_timer           = 0;
    ts_ambush_pending_jingle = 0;
    ts_ready_delay     = 0;
    gEngagedTrainerClass = 0;
    gEngagedTrainerNo    = 0;
    gEngagedJohtoParty   = 0;
    gTrainerAfterText    = NULL;
    Emote_Hide();

    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->trainers) return;

    for (int i = 0; i < ev->num_trainers; i++) {
        const map_trainer_t *t = &ev->trainers[i];
        NPC_SetFacing(t->npc_idx, t->facing);
    }

    if (ts_walk_map == (int)wCurMap && ts_walk_npc >= 0) {
        NPC_SetTilePos(ts_walk_npc, ts_walk_x, ts_walk_y);
        NPC_SetFacing(ts_walk_npc, ts_walk_facing);
    } else if (ts_walk_map != (int)wCurMap) {
        ts_walk_map = -1;
        ts_walk_npc = -1;
    }
}

void Trainer_CheckSight(void) {
    if (ts_state != TS_IDLE) return;

    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->trainers) return;

    for (int i = 0; i < ev->num_trainers; i++) {
        const map_trainer_t *t = &ev->trainers[i];

        if (CheckEvent(t->flag_bit)) continue;

        int tx, ty;
        NPC_GetTilePos(t->npc_idx, &tx, &ty);

        if (!trainer_can_see_player(tx, ty, NPC_GetFacing(t->npc_idx), t->sight_dist)) continue;

        ts_state           = TS_SPOTTED;
        ts_npc_idx         = t->npc_idx;
        ts_map_trainer_idx = i;
        ts_timer           = 60;
        gEngagedTrainerClass = t->trainer_class;
        gEngagedTrainerNo    = t->trainer_no;
        gEngagedJohtoParty   = t->johto_party;

        ts_set_defeat_text(t->trainer_class, t->defeat_text ? t->defeat_text : t->after_text);

        printf("[trainer] Trainer %d (class %d, no %d, flag_bit=%u) spotted player at (%d,%d)\n",
               i, t->trainer_class, t->trainer_no, (unsigned)t->flag_bit,
               (int)wXCoord, (int)wYCoord);
        return;
    }
}

int Trainer_SightTick(void) {
    if (ts_state == TS_IDLE) return 0;

    if (wCurMap >= NUM_MAPS) { ts_state = TS_IDLE; return 0; }
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->trainers || ts_map_trainer_idx < 0) { ts_state = TS_IDLE; return 0; }
    const map_trainer_t *t = &ev->trainers[ts_map_trainer_idx];

    switch (ts_state) {

    case TS_SPOTTED:

        if (ts_timer == 60) {
            Emote_ShowOnNPC(ts_npc_idx);
            play_trainer_encounter_music(t->trainer_class, t->johto_party);
        }
        if (--ts_timer > 0) return 0;
        Emote_Hide();
        ts_state = TS_WALKING;
        return 0;

    case TS_WALKING: {

        if (NPC_IsWalking(ts_npc_idx)) return 0;

        int tx, ty;
        NPC_GetTilePos(ts_npc_idx, &tx, &ty);
        int px = (int)wXCoord;
        int py = (int)wYCoord;
        int dx = px - tx;
        int dy = py - ty;
        int facing = t->facing;

        int adj = (facing == 0 || facing == 1) ? (dy < 0 ? -dy : dy) : (dx < 0 ? -dx : dx);
        if (adj <= 1) {

            ts_walk_map    = (int)wCurMap;
            ts_walk_npc    = ts_npc_idx;
            ts_walk_x      = tx;
            ts_walk_y      = ty;
            ts_walk_facing = facing;
            NPC_SetFacing(ts_npc_idx, facing);
            ts_state = TS_TALKING;
            return 0;
        }

        {
            int nx = tx + ((facing == 3) - (facing == 2));
            int ny = ty + ((facing == 0) - (facing == 1));
            int dirs;
            int on_vmap = AmberScript_IsEnabled() &&
                          wCurMap >= PKS_VIRTUAL_MAP_FIRST &&
                          wCurMap <= PKS_VIRTUAL_MAP_LAST;

            if (on_vmap && AmberScript_GetLedgeOverrideAt(nx * 2, ny * 2 + 1, &dirs)) {
                ts_walk_map    = (int)wCurMap;
                ts_walk_npc    = ts_npc_idx;
                ts_walk_x      = tx;
                ts_walk_y      = ty;
                ts_walk_facing = facing;
                NPC_SetFacing(ts_npc_idx, facing);
                ts_state = TS_TALKING;
                return 0;
            }
        }

        NPC_DoScriptedStep(ts_npc_idx, facing);
        return 0;
    }

    case TS_TALKING:

        if (Text_IsOpen()) return 0;
        if (ts_timer == 0) {
            if (t->before_text)
                Text_ShowASCII(t->before_text);
            ts_timer = 1;
            return 0;
        }

        if (t->end_text) {
            gGymTrainerBattlePending = 1;
            gGymTrainerVictoryEvent  = (uint32_t)t->flag_bit;
            gGymTrainerEndText       = t->end_text;
        }

        if (ts_ambush_pending_jingle) {
            ts_ambush_pending_jingle = 0;
            play_trainer_encounter_music(t->trainer_class, t->johto_party);
            ts_ready_delay = 24;
        } else {
            ts_ready_delay = 0;
        }
        ts_state = TS_READY;
        return 0;

    case TS_READY:
        if (ts_ready_delay > 0) { ts_ready_delay--; return 0; }
        ts_state = TS_IDLE;
        return 1;

    default:
        ts_state = TS_IDLE;
        return 0;
    }
}

int Trainer_IsEngaging(void) {
    return ts_state != TS_IDLE;
}

void Trainer_EngageImmediate(int npc_idx) {
    if (ts_state != TS_IDLE) return;

    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->trainers) return;

    for (int i = 0; i < ev->num_trainers; i++) {
        const map_trainer_t *t = &ev->trainers[i];
        if (t->npc_idx != npc_idx) continue;
        if (CheckEvent(t->flag_bit)) return;
        printf("[trainer] EngageImmediate: forced ambush engagement, trainer %d class=%d no=%d\n",
               i, t->trainer_class, t->trainer_no);

        ts_ambush_pending_jingle = 1;
        ts_state           = TS_TALKING;
        ts_npc_idx         = npc_idx;
        ts_map_trainer_idx = i;
        ts_timer           = 0;
        gEngagedTrainerClass = t->trainer_class;
        gEngagedTrainerNo    = t->trainer_no;
        gEngagedJohtoParty   = t->johto_party;

        ts_set_defeat_text(t->trainer_class, t->defeat_text ? t->defeat_text : t->after_text);
        return;
    }
}

static char s_pending_after_battle[64];

const char *Trainer_PeekPendingAfterBattleScene(void) {
    return s_pending_after_battle[0] ? s_pending_after_battle : NULL;
}

void Trainer_ClearPendingAfterBattleScene(void) { s_pending_after_battle[0] = '\0'; }

void Trainer_MarkCurrentDefeated(void) {
    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->trainers || ts_map_trainer_idx < 0 ||
        ts_map_trainer_idx >= ev->num_trainers) {
        printf("[trainer] MarkCurrentDefeated: NO-OP (ts_map_trainer_idx=%d, num_trainers=%d) -- defeat flag NOT set!\n",
               ts_map_trainer_idx, ev->trainers ? ev->num_trainers : -1);
        return;
    }
    uint16_t fb = ev->trainers[ts_map_trainer_idx].flag_bit;
    SetEvent(fb);

    if (ev->trainers[ts_map_trainer_idx].after_battle_scene) {
        snprintf(s_pending_after_battle, sizeof(s_pending_after_battle), "%s",
                 ev->trainers[ts_map_trainer_idx].after_battle_scene);
        printf("[trainer] after_battle queued: '%s'\n", s_pending_after_battle);
    }
    printf("[trainer] MarkCurrentDefeated: trainer %d flag_bit=%u -> SetEvent, CheckEvent now = %d\n",
           ts_map_trainer_idx, (unsigned)fb, CheckEvent(fb));
    ts_map_trainer_idx = -1;
}

void Emote_ShowOnNPC(int npc_idx) {
    s_emote_active   = 1;
    s_emote_on_player = 0;
    s_emote_npc_idx  = npc_idx;
    s_emote_which    = EMOTE_SHOCK;
    emote_load_tiles();
}

void Emote_ShowOnPlayer(void) { Emote_ShowOnPlayerKind(EMOTE_SHOCK); }

void Emote_ShowOnPlayerKind(int which) {
    s_emote_active   = 1;
    s_emote_on_player = 1;
    s_emote_npc_idx  = -1;
    s_emote_which    = which;
    emote_load_tiles();
}

void Emote_Hide(void) { s_emote_active = 0; }

int Emote_BuildOAM(oam_entry_t out[4]) {
    if (!s_emote_active) return 0;
    int actor_x, actor_y;
    if (s_emote_on_player) {
        if (wShadowOAM[0].y == 0) return 0;
        actor_x = (int)wShadowOAM[0].x - OAM_X_OFS;
        actor_y = (int)wShadowOAM[0].y - OAM_Y_OFS;
    } else if (s_emote_npc_idx >= 0) {
        if (!NPC_GetScreenTopLeft(s_emote_npc_idx, &actor_x, &actor_y)) return 0;
    } else {
        return 0;
    }
    int bubble_x = actor_x + OAM_X_OFS;
    int bubble_y = actor_y;
    out[0].y = (uint8_t)(bubble_y);     out[0].x = (bubble_x);     out[0].tile = EMOTE_TILE_BASE + 0; out[0].flags = 0;
    out[1].y = (uint8_t)(bubble_y);     out[1].x = (bubble_x + 8); out[1].tile = EMOTE_TILE_BASE + 1; out[1].flags = 0;
    out[2].y = (uint8_t)(bubble_y + 8); out[2].x = (bubble_x);     out[2].tile = EMOTE_TILE_BASE + 2; out[2].flags = 0;
    out[3].y = (uint8_t)(bubble_y + 8); out[3].x = (bubble_x + 8); out[3].tile = EMOTE_TILE_BASE + 3; out[3].flags = 0;
    return 1;
}
