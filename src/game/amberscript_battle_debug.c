
#include "amberscript_battle_debug.h"
#include "amberscript_core.h"

#include "text.h"
#include "pokemon.h"
#include "music.h"
#include "battle/battle.h"
#include "battle/battle_init.h"
#include "battle/battle_ui.h"
#include "../data/moves_data.h"
#include "../platform/hardware.h"
#include "constants.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern uint8_t gCliButtons;
extern int     gCliFrames;

static int s_animlab_enabled = 0;
static int s_animlab_move_id = 1;
static int s_animlab_loops   = 0;
static int s_animlab_level   = 50;
static int s_autowin_enabled = 0;

int AmberScript_IsAutoWinEnabled(void) { return s_autowin_enabled; }

static int pks_first_alive_party_slot(void) {
    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++) {
        if (wPartyMons[i].base.hp > 0) return i;
    }
    return -1;
}

static void pks_animlab_set_player_move(uint8_t move_id) {
    if (move_id == 0 || move_id >= NUM_MOVE_DEFS) move_id = 1;

    int slot = (int)wPlayerMonNumber;
    if (slot < 0 || slot >= PARTY_LENGTH || slot >= wPartyCount) slot = 0;

    wBattleMon.moves[0] = move_id;
    wBattleMon.moves[1] = 0;
    wBattleMon.moves[2] = 0;
    wBattleMon.moves[3] = 0;
    wBattleMon.pp[0]    = gMoves[move_id].pp;
    wBattleMon.pp[1]    = 0;
    wBattleMon.pp[2]    = 0;
    wBattleMon.pp[3]    = 0;

    wPartyMons[slot].base.moves[0] = move_id;
    wPartyMons[slot].base.moves[1] = 0;
    wPartyMons[slot].base.moves[2] = 0;
    wPartyMons[slot].base.moves[3] = 0;
    wPartyMons[slot].base.pp[0]    = gMoves[move_id].pp;
    wPartyMons[slot].base.pp[1]    = 0;
    wPartyMons[slot].base.pp[2]    = 0;
    wPartyMons[slot].base.pp[3]    = 0;
}

static void pks_animlab_set_enemy_harmless(void) {
    wEnemyMon.moves[0] = MOVE_GROWL;
    wEnemyMon.moves[1] = 0;
    wEnemyMon.moves[2] = 0;
    wEnemyMon.moves[3] = 0;
    wEnemyMon.pp[0]    = gMoves[MOVE_GROWL].pp;
    wEnemyMon.pp[1]    = 0;
    wEnemyMon.pp[2]    = 0;
    wEnemyMon.pp[3]    = 0;
}

static void pks_animlab_start_battle(int level) {
    int alive = pks_first_alive_party_slot();
    if (alive < 0) {
        Pokemon_InitMon(&wPartyMons[0], STARTER1, (uint8_t)level);
        wPartyCount = 1;
        alive = 0;
    }

    wPartyMons[alive].base.hp     = wPartyMons[alive].max_hp;
    wPartyMons[alive].base.status = 0;

    wCurPartySpecies = SPECIES_RHYDON;
    wCurEnemyLevel   = (uint8_t)level;

    Music_Play(MUSIC_WILD_BATTLE);
    Battle_Start();

    pks_animlab_set_enemy_harmless();
    pks_animlab_set_player_move((uint8_t)s_animlab_move_id);

    BattleUI_Enter();
    extern void Game_SetScene(int);
    Game_SetScene(2);

    s_animlab_enabled = 1;
    s_animlab_level   = level;
    printf("[amberscript] animlab: started (level %d), auto-playing move animations\n", level);
}

void AmberScript_BattleDebug_Tick(void) {
    extern int Game_GetScene(void);
    if (!s_animlab_enabled || Game_GetScene() != 2 ) return;

    wBattleMon.hp = wBattleMon.max_hp;
    wEnemyMon.hp  = wEnemyMon.max_hp;
    wBattleMon.status = 0;
    wEnemyMon.status  = 0;
    wPlayerBattleStatus1 = wPlayerBattleStatus2 = wPlayerBattleStatus3 = 0;
    wEnemyBattleStatus1  = wEnemyBattleStatus2  = wEnemyBattleStatus3  = 0;
    pks_animlab_set_enemy_harmless();

    if (Text_IsOpen()) {
        gCliButtons = PKS_BTN_A;
        gCliFrames  = 1;
    } else if (!AmberScript_SeqPending()) {
        int bui = BattleUI_GetState();
        if (bui == 10 ) {
            uint8_t move_id = (uint8_t)((s_animlab_move_id > 0 && s_animlab_move_id < NUM_MOVE_DEFS)
                ? s_animlab_move_id : 1);
            pks_animlab_set_player_move(move_id);

            AmberScript_SeqClear();
            AmberScript_SeqBattleMenu(0);
            AmberScript_SeqMoveSelect(1);

            s_animlab_move_id++;
            if (s_animlab_move_id >= NUM_MOVE_DEFS) {
                s_animlab_move_id = 1;
                s_animlab_loops++;
            }
        } else if (bui == 11 ) {
            AmberScript_SeqClear();
            AmberScript_SeqPush(PKS_BTN_A, 1, 8);
        }
    }
}

static const char *pks_hittrace_reason_name(uint8_t r) {
    switch (r) {
        case BHTR_HIT: return "hit";
        case BHTR_MISS_DREAM_EATER: return "miss:dream_eater_target_awake";
        case BHTR_HIT_SWIFT: return "hit:swift_always";
        case BHTR_MISS_INVULNERABLE: return "miss:target_invulnerable";
        case BHTR_MISS_MIST: return "miss:mist_block";
        case BHTR_HIT_XACCURACY: return "hit:x_accuracy_bypass";
        case BHTR_MISS_ACCURACY_ROLL: return "miss:accuracy_roll";
        default: return "unknown";
    }
}

void AmberScript_BattleDebug_WriteStateExtra(FILE *fp) {
    battle_hittrace_t ht = Battle_GetLastHitTrace();
    fprintf(fp, "HITTRACE: %s\n", Battle_HitTraceIsEnabled() ? "ON" : "OFF");
    if (ht.seq > 0) {
        const char *mname = (ht.move_num < NUM_MOVE_DEFS && gMoveNames[ht.move_num])
            ? gMoveNames[ht.move_num] : "???";
        fprintf(fp,
            "  seq=%lu turn=%s move=%u(%s) effect=0x%02X base_acc=%u scaled_acc=%u roll=%u missed=%u reason=%s\n\n",
            (unsigned long)ht.seq,
            ht.player_turn ? "enemy" : "player",
            ht.move_num, mname, ht.move_effect,
            ht.base_acc, ht.scaled_acc, ht.roll, ht.missed,
            pks_hittrace_reason_name(ht.reason));
    } else {
        fprintf(fp, "  (no MoveHitTest samples yet)\n\n");
    }

    if (s_animlab_enabled) {
        uint8_t next = (uint8_t)((s_animlab_move_id > 0 && s_animlab_move_id < NUM_MOVE_DEFS)
            ? s_animlab_move_id : 1);
        const char *next_name = gMoveNames[next] ? gMoveNames[next] : "???";
        fprintf(fp, "ANIMLAB: ON  next=%d (%s)  loops=%d\n\n",
                (int)next, next_name, s_animlab_loops);
    }
}

int AmberScript_BattleDebug_TryHandle(const char *cmd, const char *verb, int n) {
    if (strcmp(verb, "animlab") == 0) {
        char mode[16] = "start";
        int level = 50;
        sscanf(cmd, "%*s %15s %d", mode, &level);

        if (strcmp(mode, "start") == 0 || strcmp(mode, "on") == 0) {
            if (level < 5 || level > 100) level = 50;
            s_animlab_move_id = 1;
            s_animlab_loops   = 0;
            pks_animlab_start_battle(level);
        } else if (strcmp(mode, "stop") == 0 || strcmp(mode, "off") == 0) {
            s_animlab_enabled = 0;
            printf("[amberscript] animlab: stopped (battle remains under manual control)\n");
        } else if (strcmp(mode, "status") == 0) {
            printf("[amberscript] animlab: %s (next move %d, loops %d, level %d)\n",
                   s_animlab_enabled ? "ON" : "OFF",
                   s_animlab_move_id, s_animlab_loops, s_animlab_level);
        } else {
            printf("[amberscript] animlab: use 'animlab start [level]', 'animlab stop', or 'animlab status'\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "hittrace") == 0) {
        char arg[16] = {0};
        sscanf(cmd, "%*s %15s", arg);
        if (strcmp(arg, "on") == 0) {
            Battle_HitTraceEnable(1);
            printf("[amberscript] hittrace: ON\n");
        } else if (strcmp(arg, "off") == 0) {
            Battle_HitTraceEnable(0);
            printf("[amberscript] hittrace: OFF\n");
        } else if (strcmp(arg, "reset") == 0) {
            Battle_HitTraceReset();
            printf("[amberscript] hittrace: reset\n");
        } else if (strcmp(arg, "status") == 0 || arg[0] == '\0') {
            battle_hittrace_t ht = Battle_GetLastHitTrace();
            printf("[amberscript] hittrace: %s seq=%lu move=0x%02X effect=0x%02X missed=%u reason=%s\n",
                   Battle_HitTraceIsEnabled() ? "ON" : "OFF",
                   (unsigned long)ht.seq, ht.move_num, ht.move_effect, ht.missed,
                   pks_hittrace_reason_name(ht.reason));
        } else {
            printf("[amberscript] hittrace: use 'hittrace on|off|reset|status'\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "autowin") == 0) {
        char arg[16] = {0};
        sscanf(cmd, "%*s %15s", arg);
        if (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0) {
            s_autowin_enabled = 1;
            printf("[amberscript] autowin: ON (first player move each battle auto-wins)\n");
        } else if (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0) {
            s_autowin_enabled = 0;
            printf("[amberscript] autowin: OFF\n");
        } else if (strcmp(arg, "status") == 0 || arg[0] == '\0') {
            printf("[amberscript] autowin: %s\n", s_autowin_enabled ? "ON" : "OFF");
        } else {
            printf("[amberscript] autowin: use 'autowin on|off|status'\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "fight") == 0) {
        AmberScript_SeqBattleMenu(0);
        if (n >= 1 && n <= 4)
            AmberScript_SeqMoveSelect(n);
    } else if (strcmp(verb, "run") == 0) {
        AmberScript_SeqBattleMenu(3);
    } else if (strcmp(verb, "pkmn") == 0 || strcmp(verb, "pokemon") == 0) {
        AmberScript_SeqBattleMenu(1);
    } else if (strcmp(verb, "bag") == 0 || strcmp(verb, "item") == 0) {
        AmberScript_SeqBattleMenu(2);
    } else if (strcmp(verb, "battle_seed") == 0) {
        char tok[32] = {0};
        uint8_t seed;
        if (!AmberScript_ParseArg(cmd, 1, tok, sizeof(tok))) {
            printf("[amberscript] battle_seed usage: battle_seed <0-255>\n");
            AmberScript_WriteState();
            return 1;
        }
        seed = (uint8_t)strtol(tok, NULL, 0);
        hRandomAdd = seed;
        hRandomSub = (uint8_t)~seed;
        printf("[amberscript] battle_seed: hRandomAdd=0x%02X hRandomSub=0x%02X\n", hRandomAdd, hRandomSub);
        AmberScript_WriteState();
        return 1;
    } else if (strcmp(verb, "rng_state") == 0) {
        printf("[amberscript] rng_state: add=0x%02X sub=0x%02X frame=%u\n",
               hRandomAdd, hRandomSub, (unsigned)hFrameCounter);
        AmberScript_WriteState();
        return 1;
    } else {
        return 0;
    }

    if (AmberScript_SeqPending()) {
        AmberScript_RequestDeferredWrite(20);
    } else {
        AmberScript_WriteState();
    }
    return 1;
}
