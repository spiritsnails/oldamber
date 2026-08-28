#include "elite_four_scripts.h"
#include "rom_text.h"
#include "amberscript_tilemod.h"
#include <string.h>
#include <stdio.h>
#include "overworld.h"
#include "text.h"
#include "player.h"
#include "trainer_sight.h"
#include "music.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include "../platform/debug_log.h"

#define MAP_LANCES_ROOM 0x71
#define MAP_INDIGO_PLATEAU_LOBBY 0xAE
#define MAP_LORELEIS_ROOM 0xf5
#define MAP_BRUNOS_ROOM 0xf6
#define MAP_AGATHAS_ROOM 0xf7

#define LORELEI_CLASS 44
#define BRUNO_CLASS 33
#define AGATHA_CLASS 46
#define LANCE_CLASS 47

static uint8_t e4_cur_map(void) {
    int id = Map_CurrentRealId();
    return (id < 0) ? 0xFFu : (uint8_t)id;
}

typedef enum {
    E4_IDLE = 0,
    E4_PRE_WAIT,
    E4_PRE_LAST_WAIT,
    E4_PRE_JINGLE_WAIT,
    E4_BATTLE_PENDING,
    E4_POST_WAIT,
    E4_WARN_WAIT,
    E4_AUTOWALK_WAIT,
    E4_RECOVER_WAIT,
} E4State;

typedef struct {
    uint8_t map_id;
    uint16_t beat_event;
    uint16_t autowalk_event;
    uint8_t trainer_class;
    uint8_t trainer_no;

    const char *swap_prefix;
    uint8_t block_x;
    uint8_t block_y;

    const char *dont_run_label;
} e4_room_t;

static const e4_room_t kRoomLorelei = {
    MAP_LORELEIS_ROOM, EVENT_BEAT_LORELEIS_ROOM_TRAINER_0, EVENT_AUTOWALKED_INTO_LORELEIS_ROOM,
    LORELEI_CLASS, 1, "loreleisroom_door", 2, 0,
    "_LoreleisRoomLoreleiDontRunAwayText"
};
static const e4_room_t kRoomBruno = {
    MAP_BRUNOS_ROOM, EVENT_BEAT_BRUNOS_ROOM_TRAINER_0, EVENT_AUTOWALKED_INTO_BRUNOS_ROOM,
    BRUNO_CLASS, 1, "brunosroom_door", 2, 0,
    "_BrunosRoomBrunoDontRunAwayText"
};
static const e4_room_t kRoomAgatha = {
    MAP_AGATHAS_ROOM, EVENT_BEAT_AGATHAS_ROOM_TRAINER_0, EVENT_AUTOWALKED_INTO_AGATHAS_ROOM,
    AGATHA_CLASS, 1, "agathasroom_door", 2, 0,
    "_AgathasRoomAgathaDontRunAwayText"
};

#define LANCE_DOOR_L "lancesroom_door_l"
#define LANCE_DOOR_R "lancesroom_door_r"
#define LANCE_DOOR_LX 2
#define LANCE_DOOR_RX 3
#define LANCE_DOOR_Y  6

static const uint16_t kE4ResetEvents[] = {
    EVENT_BEAT_LORELEIS_ROOM_TRAINER_0,
    EVENT_AUTOWALKED_INTO_LORELEIS_ROOM,
    EVENT_BEAT_BRUNOS_ROOM_TRAINER_0,
    EVENT_AUTOWALKED_INTO_BRUNOS_ROOM,
    EVENT_BEAT_AGATHAS_ROOM_TRAINER_0,
    EVENT_AUTOWALKED_INTO_AGATHAS_ROOM,
    EVENT_BEAT_LANCES_ROOM_TRAINER_0,
    EVENT_BEAT_LANCE,
    EVENT_LANCES_ROOM_LOCK_DOOR,
};
#define E4_RESET_COUNT ((int)(sizeof kE4ResetEvents / sizeof kE4ResetEvents[0]))

static E4State g_state = E4_IDLE;
static int g_pending_battle = 0;
static int g_battle_active = 0;
static uint16_t g_active_beat_event = 0;
static uint8_t g_active_class = 0;
static uint8_t g_active_no = 0;
static int g_autowalk_started = 0;
static int g_pending_warn_push = 0;
static int g_recover_timer = 0;
static int g_pre_jingle_timer = 0;
static uint16_t g_pending_unlock_event = 0;
static const char *g_pending_after_text = 0;
static int g_post_text_started = 0;
static uint8_t g_pending_meet_class = 0;
static const char *g_pending_pre_last_text = 0;
static uint16_t g_hold_closed_event = 0;
static int g_lance_close_after_walk = 0;
static int g_lance_close_phase = 0;
static int g_lance_close_delay = 0;

static const char *after_text_for_event(uint16_t beat_event) {
    switch (beat_event) {
    case EVENT_BEAT_LORELEIS_ROOM_TRAINER_0:
        return RomText("LoreleisRoomLoreleiAfterBattleText");
    case EVENT_BEAT_BRUNOS_ROOM_TRAINER_0:
        return RomText("BrunoAfterBattleText");
    case EVENT_BEAT_AGATHAS_ROOM_TRAINER_0:
        return RomText("AgathaAfterBattleText");
    case EVENT_BEAT_LANCES_ROOM_TRAINER_0:
        return RomText("_LancesRoomLanceAfterBattleText");
    default:
        return 0;
    }
}

static void play_e4_encounter_music(uint8_t trainer_class) {

    static const uint8_t kEvil[] = {
        13, 17, 20, 21, 27, 28, 29, 30, 0xFF
    };
    static const uint8_t kFemale[] = {
        3, 6, 7, 24, 0xFF
    };
    for (int i = 0; kEvil[i] != 0xFF; i++) {
        if (trainer_class == kEvil[i]) {
            Music_Play(MUSIC_MEET_EVIL_TRAINER);
            return;
        }
    }
    for (int i = 0; kFemale[i] != 0xFF; i++) {
        if (trainer_class == kFemale[i]) {
            Music_Play(MUSIC_MEET_FEMALE_TRAINER);
            return;
        }
    }
    Music_Play(MUSIC_MEET_MALE_TRAINER);
}

static void play_pending_e4_encounter_music(void) {
    play_e4_encounter_music(g_pending_meet_class);
}

static int entrance_coord_index(void) {
    int x = (int)wXCoord, y = (int)wYCoord;
    if (x == 4 && y == 10) return 0;
    if (x == 5 && y == 10) return 1;
    if (x == 4 && y == 11) return 2;
    if (x == 5 && y == 11) return 3;
    return -1;
}

static int lance_coord_index(void) {
    int x = (int)wXCoord, y = (int)wYCoord;
    if (x == 5 && y == 1) return 0;
    if (x == 6 && y == 2) return 1;
    if (x == 5 && y == 11) return 2;
    if (x == 6 && y == 11) return 3;
    if (x == 24 && y == 16) return 4;
    return -1;
}

static const e4_room_t *get_room(uint8_t map_id) {
    if (map_id == MAP_LORELEIS_ROOM) return &kRoomLorelei;
    if (map_id == MAP_BRUNOS_ROOM) return &kRoomBruno;
    if (map_id == MAP_AGATHAS_ROOM) return &kRoomAgatha;
    return 0;
}

static void split_last_page(const char *symbol, char *out_pre, size_t pre_size,
                            char *out_last, size_t last_size) {
    const char *full = RomText(symbol);
    const char *last_break = NULL;
    for (const char *p = full; *p; p++)
        if (*p == '\f') last_break = p;
    if (!last_break) {
        out_pre[0] = '\0';
        snprintf(out_last, last_size, "%s", full);
        return;
    }
    size_t pre_len = (size_t)(last_break - full);
    if (pre_len >= pre_size) pre_len = pre_size - 1;
    memcpy(out_pre, full, pre_len);
    out_pre[pre_len] = '\0';
    snprintf(out_last, last_size, "%s", last_break + 1);
}

static int room_door_closed(const e4_room_t *r) {
    if (!r) return 0;
    if (g_hold_closed_event == r->beat_event) return 1;
    return !CheckEvent(r->beat_event);
}

static void apply_room_door(const e4_room_t *r) {
    if (!r || e4_cur_map() != r->map_id) return;
    AmberScript_PlaceSwapBlock(r->swap_prefix,
                               room_door_closed(r) ? "closed" : "open",
                               (int)r->block_x, (int)r->block_y);
}

static void start_leader(const e4_room_t *r, const char *pre, const char *pre_last, const char *end) {
    if (!r) return;
    if (CheckEvent(r->beat_event)) {

        const char *after = after_text_for_event(r->beat_event);
        Text_ShowASCII(after ? after : end);
        if (r->map_id == MAP_LANCES_ROOM) SetEvent(EVENT_BEAT_LANCE);
        return;
    }
    if (g_state != E4_IDLE) return;
    Text_ShowASCII(pre);
    g_active_beat_event = r->beat_event;
    g_active_class = r->trainer_class;
    g_active_no = r->trainer_no;
    g_pending_meet_class = r->trainer_class;
    g_pending_pre_last_text = pre_last;
    gTrainerAfterText = end;
    g_state = E4_PRE_WAIT;
}

void EliteFourScripts_ResetRun(void) {
    for (int i = 0; i < E4_RESET_COUNT; i++) ClearEvent(kE4ResetEvents[i]);
}

void EliteFourScripts_ResetRunFull(void) {
    EliteFourScripts_ResetRun();
    ClearEvent(EVENT_BEAT_CHAMPION_RIVAL);
}

static void indigo_lobby_on_load(void) {
    ClearEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH);

    for (int i = 0; i < E4_RESET_COUNT; i++) {
        if (CheckEvent(kE4ResetEvents[i])) {
            EliteFourScripts_ResetRun();
            return;
        }
    }
}

void EliteFourScripts_OnMapLoad(void) {

    const uint8_t m = e4_cur_map();
    if (m != MAP_LORELEIS_ROOM && m != MAP_BRUNOS_ROOM &&
        m != MAP_AGATHAS_ROOM  && m != MAP_LANCES_ROOM) {
        g_state = E4_IDLE;
        g_pending_battle = 0;
        g_battle_active = 0;
        g_autowalk_started = 0;
        g_pending_warn_push = 0;
        g_post_text_started = 0;
        g_recover_timer = 0;
        g_pre_jingle_timer = 0;
        g_pending_pre_last_text = NULL;
        g_pending_after_text = NULL;
    }

    if (m == MAP_INDIGO_PLATEAU_LOBBY) indigo_lobby_on_load();

    const e4_room_t *r = get_room(e4_cur_map());
    if (r) apply_room_door(r);

    if (e4_cur_map() == MAP_LANCES_ROOM) {
        if (!CheckEvent(EVENT_BEAT_LANCE) &&
            (int)wXCoord == 24 && (int)wYCoord == 16 &&
            CheckEvent(EVENT_LANCES_ROOM_LOCK_DOOR)) {

            ClearEvent(EVENT_LANCES_ROOM_LOCK_DOOR);
        }
        const char *lock = CheckEvent(EVENT_LANCES_ROOM_LOCK_DOOR)
                           ? "closed" : "open";
        AmberScript_PlaceSwapBlock(LANCE_DOOR_L, lock, LANCE_DOOR_LX, LANCE_DOOR_Y);
        AmberScript_PlaceSwapBlock(LANCE_DOOR_R, lock, LANCE_DOOR_RX, LANCE_DOOR_Y);
    }

    if (r || e4_cur_map() == MAP_LANCES_ROOM) {
        g_state = E4_IDLE;
        g_pending_warn_push = 0;
        g_pre_jingle_timer = 0;
        g_pending_pre_last_text = 0;
        g_pending_unlock_event = 0;
        g_pending_after_text = 0;
        g_post_text_started = 0;
        g_hold_closed_event = 0;
    }
    if (e4_cur_map() == MAP_LANCES_ROOM) {
        g_lance_close_after_walk = 0;
        g_lance_close_phase = 0;
        g_lance_close_delay = 0;
    }
}

void EliteFourScripts_Tick(void) {
    const e4_room_t *r = get_room(e4_cur_map());

    {
        static int last_map = -2, last_state = -1, last_x = -1, last_y = -1;
        int m = (int)e4_cur_map();
        if (m != last_map || (int)g_state != last_state ||
            (int)wXCoord != last_x || (int)wYCoord != last_y) {
            last_map = m; last_state = (int)g_state;
            last_x = (int)wXCoord; last_y = (int)wYCoord;
            DBG_PRINTF("[E4DBG] tick curmap=%u real=%d room=%s state=%d xy=(%d,%d) "
                   "lidx=%d beat_lance=%d lock=%d moving=%d\n",
                   (unsigned)wCurMap, m, r ? "yes" : "no", (int)g_state,
                   (int)wXCoord, (int)wYCoord, lance_coord_index(),
                   CheckEvent(EVENT_BEAT_LANCE),
                   CheckEvent(EVENT_LANCES_ROOM_LOCK_DOOR),
                   Player_IsMoving());
            fflush(stdout);
        }
    }
    if (e4_cur_map() == MAP_LANCES_ROOM) {
        if (g_lance_close_phase == 1) {
            if (g_lance_close_delay > 0) {
                g_lance_close_delay--;
            } else {

                AmberScript_PlaceSwapBlock(LANCE_DOOR_R, "closed",
                                           LANCE_DOOR_RX, LANCE_DOOR_Y);
                g_lance_close_phase = 0;
            }
        }

        if (g_lance_close_phase == 0) {
            const char *lock = CheckEvent(EVENT_LANCES_ROOM_LOCK_DOOR)
                               ? "closed" : "open";
            AmberScript_PlaceSwapBlock(LANCE_DOOR_L, lock,
                                       LANCE_DOOR_LX, LANCE_DOOR_Y);
            AmberScript_PlaceSwapBlock(LANCE_DOOR_R, lock,
                                       LANCE_DOOR_RX, LANCE_DOOR_Y);
        }
        int lidx = lance_coord_index();
        if (!CheckEvent(EVENT_BEAT_LANCE) &&
            g_state == E4_IDLE &&
            !Player_IsMoving() &&
            lidx >= 0 && lidx < 2) {

            hJoyHeld = 0;
            EliteFourScripts_LanceInteract();
            return;
        }
        if (!CheckEvent(EVENT_BEAT_LANCE) && lidx == 4 && g_state == E4_IDLE) {

            static const int8_t seq_lance_walk[] = {
                1,1,1,1,1,1,1,1,1,1,1,1,
                2,2,2,2,2,2,2,2,2,2,2,2,
                0,0,0,0,0,0,0,
                2,2,2,2,2,2,
                -1
            };

            if (CheckEvent(EVENT_LANCES_ROOM_LOCK_DOOR)) {
                ClearEvent(EVENT_LANCES_ROOM_LOCK_DOOR);
                AmberScript_PlaceSwapBlock(LANCE_DOOR_L, "open",
                                           LANCE_DOOR_LX, LANCE_DOOR_Y);
                AmberScript_PlaceSwapBlock(LANCE_DOOR_R, "open",
                                           LANCE_DOOR_RX, LANCE_DOOR_Y);
            }
            hJoyPressed = 0;
            hJoyHeld = 0;
            Player_StartSimulatedMovement(seq_lance_walk, 36);
            g_lance_close_after_walk = 1;
            g_state = E4_AUTOWALK_WAIT;
            g_autowalk_started = 1;
            return;
        }
        if (g_state == E4_AUTOWALK_WAIT) {
            if (g_autowalk_started && !Player_IsSimulatingMovement()) {
                g_autowalk_started = 0;
                Player_IgnoreInputFrames(3);
                g_recover_timer = 3;
                g_state = E4_RECOVER_WAIT;
            }
            return;
        }
        if (g_state == E4_RECOVER_WAIT) {
            if (g_recover_timer > 0) g_recover_timer--;
            if (g_recover_timer <= 0) {
                if (g_lance_close_after_walk && !CheckEvent(EVENT_BEAT_LANCE)) {
                    SetEvent(EVENT_LANCES_ROOM_LOCK_DOOR);

                    AmberScript_PlaceSwapBlock(LANCE_DOOR_L, "closed",
                                               LANCE_DOOR_LX, LANCE_DOOR_Y);
                    g_lance_close_phase = 1;
                    g_lance_close_delay = 1;
                    g_lance_close_after_walk = 0;
                }
                g_state = E4_IDLE;
            }
            return;
        }
        if (g_state == E4_PRE_WAIT) {
            if (!Text_IsOpen()) {
                play_pending_e4_encounter_music();
                if (g_pending_pre_last_text) Text_ShowASCII(g_pending_pre_last_text);
                g_state = E4_PRE_LAST_WAIT;
            }
            return;
        }
        if (g_state == E4_PRE_LAST_WAIT) {
            if (!Text_IsOpen()) {
                g_pre_jingle_timer = 24;
                g_state = E4_PRE_JINGLE_WAIT;
            }
            return;
        }
        if (g_state == E4_PRE_JINGLE_WAIT) {
            if (g_pre_jingle_timer > 0) g_pre_jingle_timer--;
            if (g_pre_jingle_timer <= 0) {
                g_pending_battle = 1;
                g_state = E4_BATTLE_PENDING;
            }
            return;
        }
        if (g_state == E4_POST_WAIT) {
            if (!g_post_text_started) {
                if (!Text_IsOpen() && g_pending_after_text) {
                    Text_ShowASCII(g_pending_after_text);
                    g_post_text_started = 1;
                } else if (!Text_IsOpen()) {
                    g_pending_unlock_event = 0;
                    g_pending_after_text = 0;
                    g_state = E4_IDLE;
                }
            } else if (!Text_IsOpen()) {
                g_pending_unlock_event = 0;
                g_pending_after_text = 0;
                g_state = E4_IDLE;
            }
            return;
        }
        return;
    }
    if (!r) return;

    apply_room_door(r);

    if (g_state == E4_IDLE && !CheckEvent(r->beat_event)) {
        int idx = entrance_coord_index();
        if (idx >= 0) {

            if (idx >= 2 && !CheckEvent(r->autowalk_event)) {
                hJoyPressed = 0;
                hJoyHeld = 0;
                SetEvent(r->autowalk_event);
                static const int8_t seq_up6[] = { 1, 1, 1, 1, 1, 1, -1 };
                Player_StartSimulatedMovement(seq_up6, 5);
                g_state = E4_AUTOWALK_WAIT;
                g_autowalk_started = 1;
            } else if (idx <= 1 && !Player_IsMoving() && (hJoyHeld & PAD_DOWN)) {
                hJoyHeld = 0;
                Text_ShowASCII(RomText(r->dont_run_label));
                g_pending_warn_push = 1;
                g_state = E4_WARN_WAIT;
            }
        }
    }

    if (g_state == E4_PRE_WAIT) {
        if (!Text_IsOpen()) {
            play_pending_e4_encounter_music();
            if (g_pending_pre_last_text) Text_ShowASCII(g_pending_pre_last_text);
            g_state = E4_PRE_LAST_WAIT;
        }
        return;
    }
    if (g_state == E4_PRE_LAST_WAIT) {
        if (!Text_IsOpen()) {
            g_pre_jingle_timer = 24;
            g_state = E4_PRE_JINGLE_WAIT;
        }
        return;
    }
    if (g_state == E4_PRE_JINGLE_WAIT) {
        if (g_pre_jingle_timer > 0) g_pre_jingle_timer--;
        if (g_pre_jingle_timer <= 0) {
            g_pending_battle = 1;
            g_state = E4_BATTLE_PENDING;
        }
        return;
    }
    if (g_state == E4_POST_WAIT) {
        if (!g_post_text_started) {
            if (!Text_IsOpen() && g_pending_after_text) {
                Text_ShowASCII(g_pending_after_text);
                g_post_text_started = 1;
            } else if (!Text_IsOpen()) {
                g_hold_closed_event = 0;
                g_pending_unlock_event = 0;
                g_pending_after_text = 0;
                g_state = E4_IDLE;
            }
            return;
        }
        if (!Text_IsOpen()) {
            g_hold_closed_event = 0;
            g_pending_unlock_event = 0;
            g_pending_after_text = 0;
            apply_room_door(r);
            g_state = E4_IDLE;
        }
        return;
    }
    if (g_state == E4_WARN_WAIT) {
        if (!Text_IsOpen() && g_pending_warn_push) {
            static const int8_t seq_up1[] = { 1, -1 };
            Player_StartSimulatedMovement(seq_up1, 0);
            g_pending_warn_push = 0;
            g_state = E4_AUTOWALK_WAIT;
            g_autowalk_started = 1;
        }
        return;
    }
    if (g_state == E4_AUTOWALK_WAIT) {
        if (g_autowalk_started && !Player_IsSimulatingMovement()) {
            g_autowalk_started = 0;
            if (e4_cur_map() == MAP_LANCES_ROOM &&
                g_lance_close_after_walk &&
                !CheckEvent(EVENT_BEAT_LANCE)) {
                SetEvent(EVENT_LANCES_ROOM_LOCK_DOOR);
                AmberScript_PlaceSwapBlock(LANCE_DOOR_L, "closed",
                                           LANCE_DOOR_LX, LANCE_DOOR_Y);
                AmberScript_PlaceSwapBlock(LANCE_DOOR_R, "closed",
                                           LANCE_DOOR_RX, LANCE_DOOR_Y);
                g_lance_close_after_walk = 0;
            }
            Player_IgnoreInputFrames(3);
            g_recover_timer = 3;
            g_state = E4_RECOVER_WAIT;
        }
        return;
    }
    if (g_state == E4_RECOVER_WAIT) {
        if (g_recover_timer > 0) g_recover_timer--;
        if (g_recover_timer <= 0) g_state = E4_IDLE;
        return;
    }
}

int EliteFourScripts_IsActive(void) {

    const int in_lances_room = (e4_cur_map() == MAP_LANCES_ROOM);
    const e4_room_t *r = get_room(e4_cur_map());
    return (r || in_lances_room) && g_state != E4_IDLE;
}

int EliteFourScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out) {
    if (!g_pending_battle) return 0;
    g_pending_battle = 0;
    *class_out = g_active_class;
    *no_out = g_active_no;
    g_battle_active = 1;
    return 1;
}

int EliteFourScripts_ConsumeBattle(void) {
    if (!g_battle_active) return 0;
    g_battle_active = 0;
    return 1;
}

int EliteFourScripts_BlocksMovementTo(int nx, int ny) {
    const e4_room_t *r = get_room(e4_cur_map());
    if (!r) return 0;

    if (!room_door_closed(r)) return 0;
    if (nx != 4 && nx != 5) return 0;
    return ny == 0;
}

void EliteFourScripts_OnVictory(void) {
    if (g_active_beat_event) SetEvent(g_active_beat_event);
    if (g_active_beat_event == EVENT_BEAT_AGATHAS_ROOM_TRAINER_0) {

        ClearEvent(EVENT_LANCES_ROOM_LOCK_DOOR);
    }
    g_pending_unlock_event = g_active_beat_event;
    g_hold_closed_event = g_active_beat_event;
    g_pending_after_text = after_text_for_event(g_active_beat_event);
    g_post_text_started = 0;
    if (e4_cur_map() == MAP_LANCES_ROOM) SetEvent(EVENT_BEAT_LANCE);
    g_state = E4_POST_WAIT;
}

void EliteFourScripts_OnRoomTrainerVictory(void) {
    const e4_room_t *r = get_room(e4_cur_map());

    if (e4_cur_map() == MAP_LANCES_ROOM) {
        SetEvent(EVENT_BEAT_LANCE);
        g_hold_closed_event = 0;
        g_pending_unlock_event = 0;
        g_pending_after_text = after_text_for_event(EVENT_BEAT_LANCES_ROOM_TRAINER_0);
        g_post_text_started = 0;
        g_state = E4_POST_WAIT;
        return;
    }

    if (!r) return;
    if (r->beat_event == EVENT_BEAT_AGATHAS_ROOM_TRAINER_0)
        ClearEvent(EVENT_LANCES_ROOM_LOCK_DOOR);
    g_hold_closed_event = 0;
    g_pending_unlock_event = 0;
    g_pending_after_text = after_text_for_event(r->beat_event);
    g_post_text_started = 0;
    g_state = E4_POST_WAIT;
}

void EliteFourScripts_OnDefeat(void) {
    g_pre_jingle_timer = 0;
    g_hold_closed_event = 0;
    g_pending_unlock_event = 0;
    g_pending_after_text = 0;
    g_post_text_started = 0;
    g_pending_pre_last_text = 0;
    g_state = E4_IDLE;
}

void EliteFourScripts_LoreleiInteract(void) {
    static char kPre[320], kPreLast[64];
    split_last_page("_LoreleisRoomLoreleiBeforeBattleText", kPre, sizeof(kPre),
                    kPreLast, sizeof(kPreLast));

    #define kEnd (RomTextPrefixed("LORELEI: ", "LoreleisRoomLoreleiEndBattleText"))
    start_leader(&kRoomLorelei, kPre, kPreLast, kEnd);
    #undef kEnd
}

void EliteFourScripts_BrunoInteract(void) {
    static char kPre[320], kPreLast[64];
    split_last_page("_BrunoBeforeBattleText", kPre, sizeof(kPre),
                    kPreLast, sizeof(kPreLast));

    #define kEnd (RomTextPrefixed("BRUNO: ", "BrunoEndBattleText"))
    start_leader(&kRoomBruno, kPre, kPreLast, kEnd);
    #undef kEnd
}

void EliteFourScripts_AgathaInteract(void) {
    static char kPre[320], kPreLast[64];
    split_last_page("_AgathaBeforeBattleText", kPre, sizeof(kPre),
                    kPreLast, sizeof(kPreLast));

    #define kEnd (RomTextPrefixed("AGATHA: ", "AgathaEndBattleText"))
    start_leader(&kRoomAgatha, kPre, kPreLast, kEnd);
    #undef kEnd
}

void EliteFourScripts_LanceInteract(void) {
    static char kPre[320], kPreLast[64];
    split_last_page("_LancesRoomLanceBeforeBattleText", kPre, sizeof(kPre),
                    kPreLast, sizeof(kPreLast));

    #define kEnd (RomTextPrefixed("LANCE: ", "LancesRoomLanceEndBattleText"))

    start_leader(&(e4_room_t){
                     .map_id = MAP_LANCES_ROOM,
                     .beat_event = EVENT_BEAT_LANCES_ROOM_TRAINER_0,
                     .trainer_class = LANCE_CLASS,
                     .trainer_no = 1,
                 }, kPre, kPreLast, kEnd);
    #undef kEnd
}
