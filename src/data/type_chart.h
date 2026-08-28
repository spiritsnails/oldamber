#pragma once

#include <stdint.h>

uint8_t TypeEffectiveness(uint8_t attacker, uint8_t defender);

typedef struct { uint8_t atk, def, eff; } type_entry_t;
const type_entry_t *TypeChart_Table(void);
