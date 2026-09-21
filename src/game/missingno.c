#include "missingno.h"
#include "glitches.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/assetpack.h"
#include "../data/base_stats.h"
#include <string.h>

static uint8_t s_encounter_memory[1 + 10 * 2];
static uint8_t s_observed_map = 0xff;
static int s_have_encounter_memory;
static int s_old_man_name_active;

enum {
    MAPID_CINNABAR_ISLAND = 0x08,
    MAPID_ROUTE_20 = 0x1f
};

static const uint8_t kSlotChance[10] = {
    0x33, 0x65, 0x8c, 0xa5, 0xbe, 0xd7, 0xe4, 0xf1, 0xfc, 0xff
};

static const uint8_t kMissingNoSpecies[] = {
    0x1f, 0x20, 0x32, 0x34, 0x38, 0x3d, 0x3e, 0x3f,
    0x43, 0x44, 0x45, 0x4f, 0x50, 0x51, 0x56, 0x57,
    0x5e, 0x5f, 0x73, 0x79, 0x7a, 0x7f, 0x86, 0x87,
    0x89, 0x8c, 0x92, 0x9c, 0x9f, 0xa0, 0xa1, 0xa2,
    0xac, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xb8
};

int MissingNo_IsSpecies(uint8_t species) {
    if (species == SPECIES_M_00_INTERNAL) return 1;
    for (unsigned i = 0; i < sizeof(kMissingNoSpecies); i++)
        if (kMissingNoSpecies[i] == species) return 1;
    return 0;
}

int MissingNo_IsM00(uint8_t species) {
    return species == SPECIES_M_00_INTERNAL;
}

int MissingNo_IsEnabledSpecies(uint8_t species) {
    return Glitches_MissingNoEnabled() && MissingNo_IsSpecies(species);
}

const char *MissingNo_GetName(uint8_t species) {
    if (!MissingNo_IsEnabledSpecies(species)) return NULL;
    return MissingNo_IsM00(species) ? "'M" : "MISSINGNO.";
}

int MissingNo_GetBaseStats(uint8_t species, base_stats_t *out_bs) {
    if (!out_bs || !MissingNo_IsEnabledSpecies(species)) return 0;
    memset(out_bs, 0, sizeof(*out_bs));
    out_bs->hp = 33;
    out_bs->atk = MissingNo_IsM00(species) ? 137 : 136;
    out_bs->def = 0;
    out_bs->spd = MissingNo_IsM00(species) ? 6 : 29;
    out_bs->spc = MissingNo_IsM00(species) ? 29 : 6;
    out_bs->type1 = TYPE_BIRD;
    out_bs->type2 = TYPE_NORMAL;
    out_bs->catch_rate = MissingNo_IsM00(species) ? 3 : 29;
    out_bs->base_exp = 0x8f;
    out_bs->sprite_dim = 0x88;
    out_bs->start_moves[0] = 0x37;
    out_bs->start_moves[1] = 0x37;
    out_bs->start_moves[2] = MOVE_SKY_ATTACK;
    out_bs->growth_rate = GROWTH_MEDIUM_FAST;
    return 1;
}

int MissingNo_GetTypes(uint8_t species, uint8_t *out_type1, uint8_t *out_type2) {
    if (!out_type1 || !out_type2 || !MissingNo_IsEnabledSpecies(species)) return 0;
    *out_type1 = TYPE_BIRD;
    *out_type2 = TYPE_NORMAL;
    return 1;
}

const uint8_t *MissingNo_GetFrontTile(uint8_t species, int tile) {
    if (!MissingNo_IsEnabledSpecies(species) || tile < 0 || tile >= 49) return NULL;
    if (species == 0xb6) return gFossilKabutopsSprite[tile];
    if (species == 0xb7) return gFossilAerodactylSprite[tile];
    if (species == 0xb8) return kGhostFrontSprite[tile];
    return gMissingNoFrontSprite[tile];
}

const uint8_t *MissingNo_GetBackTile(uint8_t species, int tile) {
    return MissingNo_GetFrontTile(species, tile);
}

void MissingNo_ApplyItemDuplication(void) {
    uint8_t *quantity;
    if (!Glitches_MissingNoEnabled() || wNumBagItems < 6) return;
    quantity = &wBagItems[5 * 2 + 1];
    *quantity |= 0x80;
}

void MissingNo_ApplyHallOfFameCorruption(uint8_t species) {
    AssetPack_Entry payload;
    size_t n;
    if (!MissingNo_IsEnabledSpecies(species)) return;

    if (species == 0xb6 || species == 0xb7 || species == 0xb8) return;
    if (!AssetPack_Find("gMissingNoHallOfFameCorruption", &payload)) return;
    n = payload.size;
    if (n > sizeof(wHallOfFameTeams)) n = sizeof(wHallOfFameTeams);
    memcpy(wHallOfFameTeams, payload.data, n);
}

void MissingNo_CaptureOldManName(void) {
    if (!Glitches_MissingNoEnabled()) return;
    memcpy(s_encounter_memory, wPlayerName, NAME_LENGTH);
    s_observed_map = 0xff;
    s_have_encounter_memory = 1;
    s_old_man_name_active = 1;
}

void MissingNo_ObserveWildTable(uint8_t map_id, const wild_mons_t *table) {

    if (s_old_man_name_active && map_id == MAPID_ROUTE_20) return;
    if (!table || !table->rate || map_id == s_observed_map) return;
    s_encounter_memory[0] = table->rate;
    for (int i = 0; i < 10; i++) {
        s_encounter_memory[1 + i * 2] = table->slots[i].level;
        s_encounter_memory[2 + i * 2] = table->slots[i].species;
    }
    s_observed_map = map_id;
    s_have_encounter_memory = 1;
    s_old_man_name_active = 0;
}

int MissingNo_TryCinnabarEncounter(uint8_t real_map, int x, int y, int surfing,
                                   uint8_t rate_roll, uint8_t slot_roll,
                                   uint8_t *out_species, uint8_t *out_level) {
    int slot = 9;
    uint8_t species;

    int on_cinnabar_east_shore =
        ((real_map == MAPID_CINNABAR_ISLAND && x == 19) ||
         (real_map == MAPID_ROUTE_20 && x == 0)) &&
        y >= 4 && y <= 13;
    if (!Glitches_MissingNoEnabled() || !s_have_encounter_memory ||
        !s_old_man_name_active || !on_cinnabar_east_shore || !surfing ||
        rate_roll >= s_encounter_memory[0]) return 0;
    for (int i = 0; i < 10; i++) {
        if (slot_roll <= kSlotChance[i]) {
            slot = i;
            break;
        }
    }
    species = s_encounter_memory[2 + slot * 2];
    if (species == 0) species = SPECIES_M_00_INTERNAL;
    if (!MissingNo_IsSpecies(species) && gSpeciesToDex[species] == 0) return 0;
    if (out_level) *out_level = s_encounter_memory[1 + slot * 2];
    if (out_species) *out_species = species;
    return 1;
}
