
#include "type_chart.h"
#include "assetpack_bind.h"
#include "../game/type_mod.h"

uint8_t TypeEffectiveness(uint8_t attacker, uint8_t defender) {
    uint8_t eff = 10;
    if (TypeMod_GetEffectOverride(attacker, defender, &eff)) return eff;
    for (int i = 0; kTypeChart[i].atk != 0xFF; i++)
        if (kTypeChart[i].atk == attacker && kTypeChart[i].def == defender)
            return kTypeChart[i].eff;
    return 10;
}

const type_entry_t *TypeChart_Table(void) {

    return kTypeChart;
}
