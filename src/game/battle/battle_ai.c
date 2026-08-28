
#include "battle_ai.h"

#include "../../platform/hardware.h"
#include "../constants.h"
#include "../types.h"
#include "../../data/type_chart.h"

#include "../../data/moves_data.h"

#define NUM_MOVES 4

static uint8_t ai_get_type_effectiveness(uint8_t move_type) {
    uint8_t t1 = wBattleMon.type1;
    uint8_t t2 = wBattleMon.type2;
    const type_entry_t *tbl = TypeChart_Table();
    for (int i = 0; tbl[i].atk != 0xFF; i++) {
        if (tbl[i].atk != move_type) continue;
        if (tbl[i].def == t1 || tbl[i].def == t2)
            return tbl[i].eff;
    }
    return 0x10;
}

static const uint8_t kStatusAilmentMoveEffects[] = {
    EFFECT_01,
    EFFECT_SLEEP,
    EFFECT_POISON,
    EFFECT_PARALYZE,
};

static int is_status_ailment_effect(uint8_t effect) {
    for (unsigned i = 0; i < sizeof(kStatusAilmentMoveEffects); i++)
        if (kStatusAilmentMoveEffects[i] == effect) return 1;
    return 0;
}

static void ai_move_choice_modification_1(const uint8_t moves[4],
                                          uint8_t scores[4]) {
    if (wBattleMon.status == 0) return;
    for (int i = 0; i < NUM_MOVES; i++) {
        uint8_t move = moves[i];
        if (move == 0) return;
        if (gMoves[move].power != 0) continue;
        if (is_status_ailment_effect(gMoves[move].effect))
            scores[i] += 5;
    }
}

static void ai_move_choice_modification_2(const uint8_t moves[4],
                                          uint8_t scores[4]) {
    if (wAILayer2Encouragement != 1) return;
    for (int i = 0; i < NUM_MOVES; i++) {
        uint8_t move = moves[i];
        if (move == 0) return;
        uint8_t effect = gMoves[move].effect;
        if (effect < EFFECT_ATTACK_UP1) continue;
        if (effect < EFFECT_BIDE)      { scores[i]--; continue; }
        if (effect < EFFECT_ATTACK_UP2) continue;
        if (effect < EFFECT_POISON)    { scores[i]--; continue; }

    }
}

static int has_better_move_than(const uint8_t moves[4],
                                uint8_t ineffective_type) {
    for (int j = 0; j < NUM_MOVES; j++) {
        uint8_t m = moves[j];
        if (m == 0) break;
        uint8_t effect = gMoves[m].effect;
        if (effect == EFFECT_SUPER_FANG)      return 1;
        if (effect == EFFECT_SPECIAL_DAMAGE)  return 1;
        if (effect == EFFECT_FLY)             return 1;
        if (gMoves[m].type == ineffective_type) continue;
        if (gMoves[m].power != 0)             return 1;
    }
    return 0;
}

static void ai_move_choice_modification_3(const uint8_t moves[4],
                                          uint8_t scores[4]) {
    for (int i = 0; i < NUM_MOVES; i++) {
        uint8_t move = moves[i];
        if (move == 0) return;
        uint8_t move_type = gMoves[move].type;
        uint8_t eff = ai_get_type_effectiveness(move_type);
        if (eff == 0x10) continue;
        if (eff > 0x10) { scores[i]--; continue; }

        if (has_better_move_than(moves, move_type))
            scores[i]++;
    }
}

static const uint8_t kMoveChoiceMods[NUM_TRAINERS + 1][4] = {
              { 0 },
     { 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 3, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 2, 3, 0 },
     { 1, 2, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 3, 0 },
     { 1, 0 },
     { 1, 2, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 0 },
     { 1, 0 },
     { 1, 3, 0 },
     { 1, 2, 0 },
     { 1, 3, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 3, 0 },
     { 1, 2, 0 },
     { 1, 2, 0 },
     { 1, 3, 0 },
     { 1, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 2, 0 },
     { 1, 3, 0 },
     { 1, 3, 0 },
     { 1, 2, 3, 0 },
     { 1, 0 },
     { 1, 0 },
     { 1, 3, 0 },
};

const uint8_t *AI_EnemyTrainerChooseMoves(uint8_t out_moves[4]) {
    const uint8_t *moves = wEnemyMon.moves;
    uint8_t scores[4];

    for (int i = 0; i < NUM_MOVES; i++) scores[i] = 0x0A;

    {
        uint8_t dis = (uint8_t)((wEnemyDisabledMove >> 4) & 0x0F);
        if (dis != 0)
            scores[dis - 1] = 0x50;
    }

    if (wTrainerClass == 0 || wTrainerClass > NUM_TRAINERS)
        return moves;
    const uint8_t *mods = kMoveChoiceMods[wTrainerClass];
    if (mods[0] == 0)
        return moves;

    for (int k = 0; k < NUM_MOVES && mods[k] != 0; k++) {
        switch (mods[k]) {
            case 1: ai_move_choice_modification_1(moves, scores); break;
            case 2: ai_move_choice_modification_2(moves, scores); break;
            case 3: ai_move_choice_modification_3(moves, scores); break;
            case 4:  break;
            default: break;
        }
    }

    {
        int idx = 0;
        int found = 0;
        int guard = 0;
        while (!found && guard++ < 4096) {
            if (idx >= NUM_MOVES || moves[idx] == 0) { idx = 0; continue; }
            if (--scores[idx] == 0) { found = 1; break; }
            idx++;
        }

        for (int j = idx; j >= 0; j--)
            scores[j]++;

        for (int i = 0; i < NUM_MOVES; i++) {
            if (moves[i] == 0)      { out_moves[i] = 0; continue; }
            if (scores[i] == 1)     out_moves[i] = moves[i];
            else                    out_moves[i] = 0;
        }
    }
    return out_moves;
}

extern void Battle_StatModifierUpEffect(void);
extern int  Battle_EnemySendOut_State(void);

#define AI_ITEM_FULL_RESTORE 0x10
#define AI_ITEM_HYPER_POTION 0x12
#define AI_ITEM_SUPER_POTION 0x13
#define AI_ITEM_POTION       0x14
#define AI_ITEM_X_ATTACK     0x41
#define AI_ITEM_X_DEFEND     0x42
#define AI_ITEM_X_SPEED      0x43
#define AI_ITEM_X_SPECIAL    0x44
#define AI_ITEM_FULL_HEAL    0x34
#define AI_ITEM_GUARD_SPEC   0x37

ai_action_t gLastAIAction = { AI_ACT_NONE, 0 };

static void record_action(ai_action_kind_t kind, uint8_t item_id) {
    gLastAIAction.kind    = kind;
    gLastAIAction.item_id = item_id;
}

static int decrement_ai_count(void) {
    wAICount--;
    return 1;
}

static int hp_below_fraction(uint8_t denom) {
    return wEnemyMon.hp < (uint16_t)(wEnemyMon.max_hp / denom);
}

static int ai_recover_hp(uint8_t item_id, uint16_t amount) {
    uint32_t hp = (uint32_t)wEnemyMon.hp + amount;
    if (hp > wEnemyMon.max_hp) hp = wEnemyMon.max_hp;
    wEnemyMon.hp = (uint16_t)hp;
    record_action(AI_ACT_ITEM_HEAL, item_id);
    return decrement_ai_count();
}
#define AI_POTION_HP        20
#define AI_SUPER_POTION_HP  50
#define AI_HYPER_POTION_HP 200

static void ai_cure_status(void) {
    if (wEnemyMonPartyPos < PARTY_LENGTH)
        wEnemyMons[wEnemyMonPartyPos].base.status = 0;
    wEnemyMon.status = 0;
    wEnemyBattleStatus3 &= (uint8_t)~(1u << BSTAT3_BADLY_POISONED);
}

static int ai_use_full_restore(void) {
    ai_cure_status();
    wEnemyMon.hp = wEnemyMon.max_hp;
    record_action(AI_ACT_ITEM_HEAL, AI_ITEM_FULL_RESTORE);
    return decrement_ai_count();
}

static int ai_use_full_heal(void) {
    ai_cure_status();
    record_action(AI_ACT_FULL_HEAL, AI_ITEM_FULL_HEAL);
    return decrement_ai_count();
}

static int ai_use_x_stat(uint8_t effect, uint8_t item_id) {
    uint8_t saved_effect = wEnemyMoveEffect;
    wEnemyMoveEffect = effect;
    hWhoseTurn = 1;
    Battle_StatModifierUpEffect();
    wEnemyMoveEffect = saved_effect;
    record_action(AI_ACT_X_STAT, item_id);
    return decrement_ai_count();
}

static int ai_use_guard_spec(void) {
    wEnemyBattleStatus2 |= (uint8_t)(1u << BSTAT2_PROTECTED_BY_MIST);
    record_action(AI_ACT_GUARD_SPEC, AI_ITEM_GUARD_SPEC);
    return decrement_ai_count();
}

static int ai_switch_if_enough_mons(void) {
    int alive = 0;
    for (uint8_t i = 0; i < wEnemyPartyCount; i++)
        if (wEnemyMons[i].base.hp != 0) alive++;
    if (alive < 2) return 0;

    if (wEnemyMonPartyPos < PARTY_LENGTH) {
        party_mon_t *slot = &wEnemyMons[wEnemyMonPartyPos];
        slot->base.hp        = wEnemyMon.hp;
        slot->base.box_level = wEnemyMon.party_pos;
        slot->base.status    = wEnemyMon.status;
    }
    Battle_EnemySendOut_State();
    record_action(AI_ACT_SWITCH, 0);
    return 1;
}

#define P25 64
#define P13 32
#define P50 128
#define P8  20

static int JugglerAI(uint8_t r)     { return r < P25 ? ai_switch_if_enough_mons()                          : 0; }
static int BlackbeltAI(uint8_t r)   { return r < P13 ? ai_use_x_stat(EFFECT_ATTACK_UP1,  AI_ITEM_X_ATTACK) : 0; }
static int GiovanniAI(uint8_t r)    { return r < P25 ? ai_use_guard_spec()                                 : 0; }
static int CooltrainerMAI(uint8_t r){ return r < P25 ? ai_use_x_stat(EFFECT_ATTACK_UP1,  AI_ITEM_X_ATTACK) : 0; }
static int BrunoAI(uint8_t r)       { return r < P25 ? ai_use_x_stat(EFFECT_DEFENSE_UP1, AI_ITEM_X_DEFEND) : 0; }
static int MistyAI(uint8_t r)       { return r < P25 ? ai_use_x_stat(EFFECT_DEFENSE_UP1, AI_ITEM_X_DEFEND) : 0; }
static int LtSurgeAI(uint8_t r)     { return r < P25 ? ai_use_x_stat(EFFECT_SPEED_UP1,   AI_ITEM_X_SPEED)  : 0; }
static int KogaAI(uint8_t r)        { return r < P25 ? ai_use_x_stat(EFFECT_ATTACK_UP1,  AI_ITEM_X_ATTACK) : 0; }
static int BlaineAI(uint8_t r)      { return r < P25 ? ai_recover_hp(AI_ITEM_SUPER_POTION, AI_SUPER_POTION_HP) : 0; }

static int CooltrainerFAI(uint8_t r) {
    (void)r;
    if (hp_below_fraction(10)) return ai_recover_hp(AI_ITEM_HYPER_POTION, AI_HYPER_POTION_HP);
    if (!hp_below_fraction(5)) return 0;
    return ai_switch_if_enough_mons();
}

static int BrockAI(uint8_t r) {
    (void)r;
    return wEnemyMon.status != 0 ? ai_use_full_heal() : 0;
}

static int ErikaAI(uint8_t r) {
    if (r >= P50) return 0;
    if (!hp_below_fraction(10)) return 0;
    return ai_recover_hp(AI_ITEM_SUPER_POTION, AI_SUPER_POTION_HP);
}
static int SabrinaAI(uint8_t r) {
    if (r >= P25) return 0;
    if (!hp_below_fraction(10)) return 0;
    return ai_recover_hp(AI_ITEM_HYPER_POTION, AI_HYPER_POTION_HP);
}
static int Rival2AI(uint8_t r) {
    if (r >= P13) return 0;
    if (!hp_below_fraction(5)) return 0;
    return ai_recover_hp(AI_ITEM_POTION, AI_POTION_HP);
}
static int Rival3AI(uint8_t r) {
    if (r >= P13) return 0;
    if (!hp_below_fraction(5)) return 0;
    return ai_use_full_restore();
}
static int LoreleiAI(uint8_t r) {
    if (r >= P50) return 0;
    if (!hp_below_fraction(5)) return 0;
    return ai_recover_hp(AI_ITEM_SUPER_POTION, AI_SUPER_POTION_HP);
}
static int AgathaAI(uint8_t r) {
    if (r < P8) return ai_switch_if_enough_mons();
    if (r >= P50) return 0;
    if (!hp_below_fraction(4)) return 0;
    return ai_recover_hp(AI_ITEM_SUPER_POTION, AI_SUPER_POTION_HP);
}
static int LanceAI(uint8_t r) {
    if (r >= P50) return 0;
    if (!hp_below_fraction(5)) return 0;
    return ai_recover_hp(AI_ITEM_HYPER_POTION, AI_HYPER_POTION_HP);
}
static int GenericAI(uint8_t r) { (void)r; return 0; }

typedef int (*ai_routine_fn)(uint8_t rand);
typedef struct { uint8_t count; ai_routine_fn fn; } ai_pointer_t;

static const ai_pointer_t kTrainerAIPointers[NUM_TRAINERS + 1] = {
              { 0, 0 },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, JugglerAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 3, JugglerAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 2, BlackbeltAI },
        { 3, GenericAI },
        { 3, GenericAI },
        { 1, GenericAI },
        { 3, GenericAI },
        { 1, GiovanniAI },
        { 3, GenericAI },
        { 2, CooltrainerMAI },
        { 1, CooltrainerFAI },
        { 2, BrunoAI },
        { 5, BrockAI },
        { 1, MistyAI },
        { 1, LtSurgeAI },
        { 1, ErikaAI },
        { 2, KogaAI },
        { 2, BlaineAI },
        { 1, SabrinaAI },
        { 3, GenericAI },
        { 1, Rival2AI },
        { 1, Rival3AI },
        { 2, LoreleiAI },
        { 3, GenericAI },
        { 2, AgathaAI },
        { 1, LanceAI },
};

int AI_TrainerAI(void) {
    record_action(AI_ACT_NONE, 0);
    if (wIsInBattle != 2) return 0;
    if (wTrainerClass == 0 || wTrainerClass > NUM_TRAINERS) return 0;

    if (wAICount == 0) return 0;
    const ai_pointer_t *p = &kTrainerAIPointers[wTrainerClass];
    if (wAICount == 0xFF)
        wAICount = p->count;

    uint8_t rand = BattleRandom();
    return p->fn ? p->fn(rand) : 0;
}
