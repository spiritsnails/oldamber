
#include "amberscript_story.h"
#include "amberscript_core.h"

#include "overworld.h"
#include "npc.h"
#include "trainer_sight.h"
#include "badge.h"
#include "inventory.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include "../data/map_data.h"
#include "../data/event_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);

static int pks_is_numeric_token(const char *s) {
    if (!s || !*s) return 0;
    if (s[0] == '+' || s[0] == '-') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!*s) return 0;
        while (*s) {
            if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')))
                return 0;
            s++;
        }
        return 1;
    }
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

static void pks_norm(char *dst, size_t dst_sz, const char *src) {
    size_t n = 0;
    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    if (!src) return;
    for (size_t i = 0; src[i] && n + 1 < dst_sz; i++) {
        char c = (char)tolower((unsigned char)src[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) dst[n++] = c;
    }
    dst[n] = '\0';
}

static int pks_resolve_map_token(const char *tok, int *out_map_id) {
    char want[64];
    if (!tok || !*tok || !out_map_id) return 0;
    if (strcmp(tok, "here") == 0 || strcmp(tok, "current") == 0) {
        *out_map_id = (int)wCurMap;
        return 1;
    }
    if (pks_is_numeric_token(tok)) {
        *out_map_id = (int)strtol(tok, NULL, 0);
        return (*out_map_id >= 0 && *out_map_id < NUM_MAPS);
    }
    pks_norm(want, sizeof(want), tok);
    for (int i = 0; i < NUM_MAPS; i++) {
        char have[64];
        pks_norm(have, sizeof(have), gMapTable[i].name);
        if (strcmp(want, have) == 0) {
            *out_map_id = i;
            return 1;
        }
    }
    return 0;
}

static void pks_gym_clear(const char *name) {
    if (strcmp(name, "brock") == 0) {
        ClearEvent(EVENT_BEAT_BROCK);
        ClearEvent(EVENT_GOT_TM34);
        ClearEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_BOULDER);
    } else if (strcmp(name, "misty") == 0) {
        ClearEvent(EVENT_BEAT_MISTY);
        ClearEvent(EVENT_GOT_TM11);
        ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_CASCADE);
    } else if (strcmp(name, "surge") == 0) {
        ClearEvent(EVENT_BEAT_LT_SURGE);
        ClearEvent(EVENT_GOT_TM24);
        ClearEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_THUNDER);
    } else if (strcmp(name, "erika") == 0) {
        ClearEvent(EVENT_BEAT_ERIKA);
        ClearEvent(EVENT_GOT_TM21);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_RAINBOW);
    } else if (strcmp(name, "koga") == 0) {
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_GOT_TM06);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_SOUL);
    } else if (strcmp(name, "blaine") == 0) {
        ClearEvent(EVENT_BEAT_BLAINE);
        ClearEvent(EVENT_GOT_TM38);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_5);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_6);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_VOLCANO);
    }
}

typedef struct pks_eventdiff_snapshot_t {
    int valid;
    uint8_t badges;
    uint8_t map;
    uint8_t x;
    uint8_t y;
    uint8_t party_count;
    uint8_t key_flags[15];
} pks_eventdiff_snapshot_t;
static pks_eventdiff_snapshot_t s_eventdiff = {0};

static const uint16_t s_eventdiff_keys[15] = {
    EVENT_GOT_POKEDEX,
    EVENT_BEAT_BROCK,
    EVENT_BEAT_MISTY,
    EVENT_BEAT_LT_SURGE,
    EVENT_BEAT_ERIKA,
    EVENT_BEAT_KOGA,
    EVENT_BEAT_BLAINE,
    EVENT_BEAT_SABRINA,
    EVENT_GOT_HM01,
    EVENT_GOT_TM34,
    EVENT_GOT_TM11,
    EVENT_GOT_TM24,
    EVENT_GOT_TM21,
    EVENT_GOT_TM06,
    EVENT_GOT_TM38
};

void AmberScript_EventDiff_Snapshot(void) {
    s_eventdiff.valid = 1;
    s_eventdiff.badges = wObtainedBadges;
    s_eventdiff.map = wCurMap;
    s_eventdiff.x = wXCoord;
    s_eventdiff.y = wYCoord;
    s_eventdiff.party_count = wPartyCount;
    for (int i = 0; i < 15; i++)
        s_eventdiff.key_flags[i] = (uint8_t)CheckEvent(s_eventdiff_keys[i]);
}

void AmberScript_EventDiff_PrintDiff(void) {
    if (!s_eventdiff.valid) {
        printf("[amberscript] eventdiff: no snapshot; run 'eventdiff snapshot'\n");
        return;
    }
    printf("  map %u->%u, pos (%u,%u)->(%u,%u), badges 0x%02X->0x%02X, party %u->%u\n",
           s_eventdiff.map, wCurMap, s_eventdiff.x, s_eventdiff.y, wXCoord, wYCoord,
           s_eventdiff.badges, wObtainedBadges, s_eventdiff.party_count, wPartyCount);
    for (int i = 0; i < 15; i++) {
        uint8_t now = (uint8_t)CheckEvent(s_eventdiff_keys[i]);
        if (now != s_eventdiff.key_flags[i]) {
            printf("  flag %u: %u -> %u\n",
                   (unsigned)s_eventdiff_keys[i], s_eventdiff.key_flags[i], now);
        }
    }
}

int AmberScript_Story_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;

    if (strcmp(verb, "story_guard") == 0) {
        char name[32] = {0};
        int missing = 0;
#define GUARD_EVENT(ev, label) do { if (!CheckEvent((ev))) { printf("[story_guard] missing event: %s (%u)\n", (label), (unsigned)(ev)); missing++; } } while (0)
#define GUARD_BADGE(bit, label) do { if (!(wObtainedBadges & (1u << (bit)))) { printf("[story_guard] missing badge: %s\n", (label)); missing++; } } while (0)
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] story_guard usage: story_guard <brock|misty|surge|erika|koga|blaine|bike_gate|list>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(name, "list") == 0 || strcmp(name, "help") == 0) {
            printf("[amberscript] story_guard targets: brock, misty, surge, erika, koga, blaine, bike_gate\n");
            AmberScript_WriteState();
            return 1;
        } else if (strcmp(name, "brock") == 0) {
            GUARD_EVENT(EVENT_GOT_POKEDEX, "EVENT_GOT_POKEDEX");
        } else if (strcmp(name, "misty") == 0) {
            GUARD_EVENT(EVENT_GOT_POKEDEX, "EVENT_GOT_POKEDEX");
            GUARD_EVENT(EVENT_BEAT_BROCK, "EVENT_BEAT_BROCK");
            GUARD_BADGE(BADGE_BOULDER, "BOULDER");
        } else if (strcmp(name, "surge") == 0) {
            GUARD_EVENT(EVENT_BEAT_MISTY, "EVENT_BEAT_MISTY");
            GUARD_BADGE(BADGE_CASCADE, "CASCADE");
            GUARD_EVENT(EVENT_GOT_HM01, "EVENT_GOT_HM01");
        } else if (strcmp(name, "erika") == 0) {
            GUARD_EVENT(EVENT_BEAT_LT_SURGE, "EVENT_BEAT_LT_SURGE");
            GUARD_BADGE(BADGE_THUNDER, "THUNDER");
        } else if (strcmp(name, "koga") == 0) {
            GUARD_EVENT(EVENT_BEAT_ERIKA, "EVENT_BEAT_ERIKA");
            GUARD_BADGE(BADGE_RAINBOW, "RAINBOW");
        } else if (strcmp(name, "blaine") == 0) {
            GUARD_EVENT(EVENT_BEAT_KOGA, "EVENT_BEAT_KOGA");
            GUARD_BADGE(BADGE_SOUL, "SOUL");
        } else if (strcmp(name, "bike_gate") == 0) {
            if (Inventory_GetQty(ITEM_BICYCLE) == 0) {
                printf("[story_guard] missing item: BICYCLE\n");
                missing++;
            }
        } else {
            printf("[amberscript] story_guard: unknown target '%s'\n", name);
            AmberScript_WriteState();
            return 1;
        }
        if (missing == 0) printf("[story_guard] %s: OK\n", name);
        else printf("[story_guard] %s: %d prerequisite(s) missing\n", name, missing);
#undef GUARD_EVENT
#undef GUARD_BADGE
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "eventdiff") == 0) {
        char sub[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "show");
        if (strcmp(sub, "snapshot") == 0 || strcmp(sub, "snap") == 0) {
            AmberScript_EventDiff_Snapshot();
            printf("[amberscript] eventdiff: snapshot captured (map=%u pos=%u,%u badges=0x%02X)\n",
                   s_eventdiff.map, s_eventdiff.x, s_eventdiff.y, s_eventdiff.badges);
        } else if (strcmp(sub, "show") == 0 || strcmp(sub, "diff") == 0) {
            if (!s_eventdiff.valid) {
                printf("[amberscript] eventdiff: no snapshot; run 'eventdiff snapshot'\n");
            } else {
                printf("[amberscript] eventdiff:\n");
                AmberScript_EventDiff_PrintDiff();
            }
        } else {
            printf("[amberscript] eventdiff usage: eventdiff snapshot | eventdiff show\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "trainer_reset") == 0) {
        char tok[64] = {0};
        char mode[8] = {0};
        if (!AmberScript_ParseArg(cmd, 1, tok, sizeof(tok))) strcpy(tok, "here");
        (void)AmberScript_ParseArg(cmd, 2, mode, sizeof(mode));
        if (pks_is_numeric_token(tok) && (strcmp(mode, "set") == 0 || strcmp(mode, "clear") == 0 || strcmp(mode, "1") == 0 || strcmp(mode, "0") == 0)) {
            int flag = (int)strtol(tok, NULL, 0);
            if (strcmp(mode, "set") == 0 || strcmp(mode, "1") == 0) {
                SetEvent((uint16_t)flag);
                printf("[amberscript] trainer_reset: set event %d (trainer beaten)\n", flag);
            } else {
                ClearEvent((uint16_t)flag);
                printf("[amberscript] trainer_reset: cleared event %d (trainer unbeaten)\n", flag);
            }
        } else {
            int map_id = -1;
            int cleared = 0;
            if (!pks_resolve_map_token(tok, &map_id)) {
                printf("[amberscript] trainer_reset: unknown map '%s' (use map id, map name, or 'here')\n", tok);
                AmberScript_WriteState();
                return 1;
            }
            if (map_id < 0 || map_id >= NUM_MAPS) {
                printf("[amberscript] trainer_reset: map out of range (%d)\n", map_id);
                AmberScript_WriteState();
                return 1;
            }
            {
                const map_events_t *ev = &gMapEvents[map_id];
                for (int i = 0; i < ev->num_trainers; i++) {
                    ClearEvent(ev->trainers[i].flag_bit);
                    cleared++;
                }
            }
            if (wCurMap == (uint8_t)map_id) {
                NPC_Load();
                Trainer_LoadMap();
            }
            printf("[amberscript] trainer_reset: cleared %d trainer flag(s) on map %d (%s)\n",
                   cleared, map_id, gMapTable[map_id].name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "gym_reset") == 0) {
        char gym[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, gym, sizeof(gym))) {
            printf("[amberscript] gym_reset usage: gym_reset <brock|misty|surge|erika|koga|blaine|all>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(gym, "all") == 0) {
            pks_gym_clear("brock");
            pks_gym_clear("misty");
            pks_gym_clear("surge");
            pks_gym_clear("erika");
            pks_gym_clear("koga");
            pks_gym_clear("blaine");
            printf("[amberscript] gym_reset: cleared Brock..Blaine leader/trainer/TM progress\n");
        } else {
            pks_gym_clear(gym);
            {
                uint8_t gym_map; int gym_x, gym_y;
                if (strcmp(gym, "brock") == 0) { gym_map = 0x36; gym_x = 4; gym_y = 2; }
                else if (strcmp(gym, "misty") == 0) { gym_map = 0x41; gym_x = 4; gym_y = 8; }
                else if (strcmp(gym, "surge") == 0) { gym_map = 0x5C; gym_x = 5; gym_y = 3; }
                else if (strcmp(gym, "erika") == 0) { gym_map = 0x87; gym_x = 4; gym_y = 12; }
                else if (strcmp(gym, "koga") == 0) { gym_map = 0x9D; gym_x = 4; gym_y = 11; }
                else if (strcmp(gym, "blaine") == 0) { gym_map = 0xA6; gym_x = 3; gym_y = 4; }
                else {
                    printf("[amberscript] gym_reset: unknown gym '%s'\n", gym);
                    AmberScript_WriteState();
                    return 1;
                }
                Game_WarpToRealMap(gym_map, gym_x, gym_y);
            }
            printf("[amberscript] gym_reset: cleared %s progress and teleported to leader\n", gym);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "e4_reset") == 0 ||
        strcmp(verb, "elite4_reset") == 0 ||
        strcmp(verb, "elitefour_reset") == 0) {
        ClearEvent(EVENT_BEAT_LORELEIS_ROOM_TRAINER_0);
        ClearEvent(EVENT_AUTOWALKED_INTO_LORELEIS_ROOM);
        ClearEvent(EVENT_BEAT_BRUNOS_ROOM_TRAINER_0);
        ClearEvent(EVENT_AUTOWALKED_INTO_BRUNOS_ROOM);
        ClearEvent(EVENT_BEAT_AGATHAS_ROOM_TRAINER_0);
        ClearEvent(EVENT_AUTOWALKED_INTO_AGATHAS_ROOM);
        ClearEvent(EVENT_BEAT_LANCES_ROOM_TRAINER_0);
        ClearEvent(EVENT_BEAT_LANCE);
        ClearEvent(EVENT_LANCES_ROOM_LOCK_DOOR);

        int e4r = Map_CurrentRealId();
        if (e4r == 0xae || e4r == 0xf5 || e4r == 0xf6 ||
            e4r == 0xf7 || e4r == 0x71 || e4r == 0x78) {
            Map_Load(wCurMap);
            NPC_Load();
            Trainer_LoadMap();
        }

        printf("[amberscript] e4_reset: cleared Elite Four room beat/autowalk/lock flags\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tmflow") == 0) {
        char gym[16] = {0};
        char state[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, gym, sizeof(gym)) || !AmberScript_ParseArg(cmd, 2, state, sizeof(state))) {
            printf("[amberscript] tmflow usage: tmflow <brock|misty|surge|erika|koga|blaine> <pre|post|done|reset>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(state, "reset") == 0 || strcmp(state, "pre") == 0) {
            pks_gym_clear(gym);
            printf("[amberscript] tmflow: %s set to pre-battle/reset state\n", gym);
        } else {
            if (strcmp(gym, "brock") == 0) {
                SetEvent(EVENT_BEAT_BROCK); wObtainedBadges |= (1u << BADGE_BOULDER);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM34); else ClearEvent(EVENT_GOT_TM34);
            } else if (strcmp(gym, "misty") == 0) {
                SetEvent(EVENT_BEAT_MISTY); wObtainedBadges |= (1u << BADGE_CASCADE);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM11); else ClearEvent(EVENT_GOT_TM11);
            } else if (strcmp(gym, "surge") == 0) {
                SetEvent(EVENT_BEAT_LT_SURGE); wObtainedBadges |= (1u << BADGE_THUNDER);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM24); else ClearEvent(EVENT_GOT_TM24);
            } else if (strcmp(gym, "erika") == 0) {
                SetEvent(EVENT_BEAT_ERIKA); wObtainedBadges |= (1u << BADGE_RAINBOW);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM21); else ClearEvent(EVENT_GOT_TM21);
            } else if (strcmp(gym, "koga") == 0) {
                SetEvent(EVENT_BEAT_KOGA); wObtainedBadges |= (1u << BADGE_SOUL);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM06); else ClearEvent(EVENT_GOT_TM06);
            } else if (strcmp(gym, "blaine") == 0) {
                SetEvent(EVENT_BEAT_BLAINE); wObtainedBadges |= (1u << BADGE_VOLCANO);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM38); else ClearEvent(EVENT_GOT_TM38);
            }
            printf("[amberscript] tmflow: %s set to %s-TM state\n", gym, state);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "gym_badges_clear") == 0) {
        int keep = 0;
        sscanf(cmd, "%*s %d", &keep);
        if (keep < 0) keep = 0;
        if (keep > 8) keep = 8;
        wObtainedBadges &= (uint8_t)((keep == 0) ? 0 : ((1u << keep) - 1u));
        if (keep <= 0) pks_gym_clear("brock");
        if (keep <= 1) pks_gym_clear("misty");
        if (keep <= 2) pks_gym_clear("surge");
        if (keep <= 3) pks_gym_clear("erika");
        if (keep <= 4) pks_gym_clear("koga");
        if (keep <= 5) pks_gym_clear("blaine");
        printf("[amberscript] gym_badges_clear: kept first %d badge(s), cleared later gym progress\n", keep);
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
