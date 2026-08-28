
#include "johto_battle.h"
#include "gen2_species.h"
#include "gen2_evos_moves.h"
#include "johto_trainers.h"
#include <stdio.h>
#include <string.h>

extern uint8_t Game_ArmCustomTrainerParty(const uint8_t species[6],
                                          const uint8_t level[6],
                                          const uint8_t moves[6][4],
                                          uint8_t count);

uint8_t JohtoBattle_DexToInternal(uint8_t dex) {
    return Gen2Species_AnyDexToInternal(dex);
}

uint8_t JohtoBattle_ArmPending(uint16_t johto_party) {
    uint8_t species[6], level[6], moves[6][4];
    uint8_t count = 0;
    const johto_trainer_t *t;
    int skipped = 0;

    if (johto_party == 0) return 0;
    if (johto_party > JOHTO_TRAINER_COUNT) {
        printf("[johto] party index %u is past the end of gJohtoTrainers (%d) -- "
               "is generated/johto_trainers.c current?\n",
               (unsigned)johto_party, JOHTO_TRAINER_COUNT);
        return 0;
    }
    t = &gJohtoTrainers[johto_party - 1];

    memset(species, 0, sizeof(species));
    memset(level, 0, sizeof(level));
    memset(moves, 0, sizeof(moves));

    for (int i = 0; i < t->count && i < 6; i++) {
        const johto_mon_t *m = &t->party[i];
        uint8_t internal;
        int named = 0;
        if (m->species == 0 || m->level == 0) continue;
        internal = Gen2Species_AnyDexToInternal(m->species);
        if (internal == 0) {
            printf("[johto] %s %s: dex %u (lv %u) has no internal id -- "
                   "slot skipped\n", t->class_name, t->name,
                   (unsigned)m->species, (unsigned)m->level);
            skipped++;
            continue;
        }
        species[count] = internal;
        level[count] = m->level;
        for (int k = 0; k < 4; k++) {
            moves[count][k] = m->moves[k];
            if (m->moves[k]) named = 1;
        }
        if (!named)
            Gen2EvosMoves_MovesAtLevel(m->species, m->level, moves[count]);
        count++;
    }

    if (count == 0) {
        printf("[johto] %s %s: no fieldable mon (%d skipped) -- falling back to "
               "the Gen 1 roster\n", t->class_name, t->name, skipped);
        return 0;
    }
    if (skipped)
        printf("[johto] %s %s: fielding %d of %d mon (%d skipped)\n",
               t->class_name, t->name, count, t->count, skipped);

    return Game_ArmCustomTrainerParty(species, level,
                                      (const uint8_t (*)[4])moves, count);
}
