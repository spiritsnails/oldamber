
#include "amberscript_party_items.h"
#include "amberscript_core.h"

#include "pokemon.h"
#include "inventory.h"
#include "species_mod.h"
#include "../platform/hardware.h"
#include "../data/base_stats.h"
#include "../data/moves_data.h"
#include "constants.h"
#include "../data/item_names_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int pks_resolve_move_id(const char *move_str) {
    if (!move_str || !*move_str) return 0;
    char *end;
    long parsed = strtol(move_str, &end, 0);
    if (end != move_str && *end == '\0') return (int)parsed;
    char needle[32] = {0};
    int ni = 0;
    for (int i = 0; move_str[i] && ni < 31; i++) {
        char c = (char)tolower((unsigned char)move_str[i]);
        if (c != ' ' && c != '_') needle[ni++] = c;
    }
    for (int id = 1; id <= 0xA5; id++) {
        const char *nm = gMoveNames[id];
        char norm[32] = {0};
        int nnorm = 0;
        for (int i = 0; nm[i] && nnorm < 31; i++) {
            char c = (char)tolower((unsigned char)nm[i]);
            if (c != ' ' && c != '_') norm[nnorm++] = c;
        }
        if (strcmp(needle, norm) == 0) return id;
    }
    return 0;
}

static int pks_resolve_species_id(const char *species_str) {
    uint8_t sid = 0;
    if (!species_str || !*species_str) return 0;
    if (SpeciesMod_ResolveSpeciesToken(species_str, &sid)) return (int)sid;
    {
        char *end;
        long parsed = strtol(species_str, &end, 0);
        if (end != species_str && *end == '\0') {
            if (parsed >= 1 && parsed <= 151) return gDexToSpecies[parsed];
            return (int)parsed;
        }
    }
    {
        char needle[40] = {0};
        int ni = 0;
        for (int i = 0; species_str[i] && ni < 39; i++) {
            char c = (char)tolower((unsigned char)species_str[i]);
            if (c != ' ' && c != '_') needle[ni++] = c;
        }
        for (int dex = 1; dex <= 151; dex++) {
            const char *nm = Pokemon_GetName((uint8_t)dex);
            char norm[40] = {0};
            int nn = 0;
            for (int i = 0; nm[i] && nn < 39; i++) {
                char c = (char)tolower((unsigned char)nm[i]);
                if (c != ' ' && c != '_') norm[nn++] = c;
            }
            if (strcmp(needle, norm) == 0) return gDexToSpecies[dex];
        }
    }
    return 0;
}

int AmberScript_PartyItems_TryHandle(const char *cmd, const char *verb, int n) {
    if (strcmp(verb, "giveitem") == 0) {
        static const struct { const char *name; uint8_t id; } kItems[] = {
            { "master_ball",   0x01 }, { "ultra_ball",    0x02 },
            { "great_ball",    0x03 }, { "poke_ball",     0x04 },
            { "town_map",      0x05 }, { "bicycle",       0x06 },
            { "pokedex",       0x09 }, { "moon_stone",    0x0A },
            { "antidote",      0x0B }, { "burn_heal",     0x0C },
            { "ice_heal",      0x0D }, { "awakening",     0x0E },
            { "parlyz_heal",   0x0F }, { "full_restore",  0x10 },
            { "max_potion",    0x11 }, { "hyper_potion",  0x12 },
            { "super_potion",  0x13 }, { "potion",        0x14 },
            { "escape_rope",   0x1D }, { "repel",         0x1E },
            { "fire_stone",    0x20 }, { "thunder_stone", 0x21 },
            { "water_stone",   0x22 }, { "leaf_stone",    0x2F },
            { "hp_up",         0x23 }, { "protein",       0x24 },
            { "iron",          0x25 }, { "carbos",        0x26 },
            { "calcium",       0x27 }, { "rare_candy",    0x28 },
            { "poke_doll",     0x33 }, { "full_heal",     0x34 },
            { "revive",        0x35 }, { "max_revive",    0x36 },
            { "super_repel",   0x38 }, { "max_repel",     0x39 },
            { "oaks_parcel",   0x46 }, { "parcel",        0x46 },
            { "poke_flute",    0x49 }, { "pokeflute",     0x49 },
            { "pp_up",         0x4F }, { "ether",         0x50 },
            { "max_ether",     0x51 }, { "elixer",        0x52 },
            { "max_elixer",    0x53 },
            { "hm01",          0xC4 }, { "tm01",          0xC9 },
            { NULL, 0 }
        };
        char id_str[32] = {0};
        int qty = 1;
        sscanf(cmd, "%*s %31s %d", id_str, &qty);
        if (qty < 1) qty = 1;

        int id = -1;
        char lc[32] = {0};
        for (int k = 0; id_str[k] && k < 31; k++)
            lc[k] = (id_str[k] >= 'A' && id_str[k] <= 'Z') ? id_str[k]+32 : id_str[k];
        for (int k = 0; kItems[k].name; k++) {
            if (strcmp(lc, kItems[k].name) == 0) { id = kItems[k].id; break; }
        }

        if (id < 0) {
            char uc[32] = {0};
            for (int k = 0; id_str[k] && k < 31; k++) {
                char c = id_str[k];
                if (c == ' ' || c == '-') c = '_';
                uc[k] = (char)toupper((unsigned char)c);
            }
            for (unsigned k = 0; k < NUM_ITEM_NAMES; k++)
                if (strcmp(uc, kItemNames[k].name) == 0) { id = kItemNames[k].id; break; }
        }
        if (id < 0) id = (int)strtol(id_str, NULL, 0);

        if (id <= 0 || id > 255) {
            printf("[amberscript] giveitem: unknown item '%s'\n", id_str);
        } else if (Inventory_Add((uint8_t)id, (uint8_t)qty) == 0) {
            printf("[amberscript] giveitem 0x%02X x%d -> added\n", id, qty);
        } else {
            printf("[amberscript] giveitem: bag full\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "givetm") == 0) {
        if (n < 1 || n > 50) {
            printf("[amberscript] givetm: n must be 1-50\n");
        } else {
            uint8_t id = (uint8_t)(TM01 + n - 1);
            if (Inventory_Add(id, 1) == 0)
                printf("[amberscript] givetm %d -> TM%02d (0x%02X) added\n", n, n, id);
            else
                printf("[amberscript] givetm: bag full\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "givehm") == 0) {
        if (n < 1 || n > 5) {
            printf("[amberscript] givehm: n must be 1-5\n");
        } else {
            uint8_t id = (uint8_t)(HM01 + n - 1);
            if (Inventory_Add(id, 1) == 0)
                printf("[amberscript] givehm %d -> HM%02d (0x%02X) added\n", n, n, id);
            else
                printf("[amberscript] givehm: bag full\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "givemon") == 0) {
        int dex = 1, level = 20;
        sscanf(cmd, "%*s %d %d", &dex, &level);
        if (dex < 1 || dex > 151) {
            printf("[amberscript] givemon: dex must be 1-151\n");
        } else if (level < 1 || level > 100) {
            printf("[amberscript] givemon: level must be 1-100\n");
        } else {
            uint8_t species = gDexToSpecies[dex];
            int slot = (wPartyCount < 6) ? wPartyCount : 5;
            Pokemon_InitMon(&wPartyMons[slot], species, (uint8_t)level);
            memcpy(wPartyMonOT[slot], wPlayerName, NAME_LENGTH);
            Pokemon_EncodeNameString(Pokemon_GetNameBySpecies(species), wPartyMonNicks[slot]);
            wPartySpecies[slot] = species;
            if (slot + 1 < PARTY_LENGTH) wPartySpecies[slot + 1] = 0xFF;
            if (wPartyCount < 6) wPartyCount++;
            printf("[amberscript] givemon: slot %d -> %s Lv%d (species 0x%02X)\n",
                   slot + 1, Pokemon_GetName(dex), level, species);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "givemonx") == 0) {
        char s_tok[40] = {0};
        int level = 20;
        int sid;
        if (sscanf(cmd, "%*s %39s %d", s_tok, &level) < 1) {
            printf("[amberscript] givemonx usage: givemonx <species> [level]\n");
            return 1;
        }
        sid = pks_resolve_species_id(s_tok);
        if (sid <= 0 || sid > 255) {
            printf("[amberscript] givemonx: bad species '%s'\n", s_tok);
            return 1;
        }
        if (level < 1 || level > 100) {
            printf("[amberscript] givemonx: level must be 1-100\n");
            return 1;
        }
        {
            int slot = (wPartyCount < 6) ? wPartyCount : 5;
            Pokemon_InitMon(&wPartyMons[slot], (uint8_t)sid, (uint8_t)level);
            memcpy(wPartyMonOT[slot], wPlayerName, NAME_LENGTH);
            Pokemon_EncodeNameString(Pokemon_GetNameBySpecies((uint8_t)sid), wPartyMonNicks[slot]);
            wPartySpecies[slot] = (uint8_t)sid;
            if (slot + 1 < PARTY_LENGTH) wPartySpecies[slot + 1] = 0xFF;
            if (wPartyCount < 6) wPartyCount++;
            printf("[amberscript] givemonx: slot %d -> %s Lv%d (species 0x%02X)\n",
                   slot + 1, Pokemon_GetNameBySpecies((uint8_t)sid), level, sid);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "magneton50") == 0) {
        uint8_t species = gDexToSpecies[82];
        int slot = (wPartyCount < 6) ? wPartyCount : 5;
        Pokemon_InitMon(&wPartyMons[slot], species, 50);
        if (wPartyCount < 6) wPartyCount++;

        wPartyMons[slot].base.moves[0] = 85;
        wPartyMons[slot].base.moves[1] = 86;
        wPartyMons[slot].base.moves[2] = 87;
        wPartyMons[slot].base.moves[3] = 48;
        wPartyMons[slot].base.pp[0] = gMoves[85].pp;
        wPartyMons[slot].base.pp[1] = gMoves[86].pp;
        wPartyMons[slot].base.pp[2] = gMoves[87].pp;
        wPartyMons[slot].base.pp[3] = gMoves[48].pp;
        wPartyMons[slot].base.hp = wPartyMons[slot].max_hp;
        wPartyMons[slot].base.status = 0;

        printf("[amberscript] magneton50: slot %d -> MAGNETON Lv50 (THUNDERBOLT / THUNDER WAVE / THUNDER / SUPERSONIC)\n",
               slot + 1);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "bulba15") == 0 || strcmp(verb, "givemon_bulba15") == 0) {
        uint8_t species = gDexToSpecies[1];
        Pokemon_InitMon(&wPartyMons[0], species, 15);
        if (wPartyCount < 1) wPartyCount = 1;

        wPartyMons[0].base.moves[0] = 79;
        wPartyMons[0].base.moves[1] = 75;
        wPartyMons[0].base.moves[2] = 73;
        wPartyMons[0].base.moves[3] = 22;
        wPartyMons[0].base.pp[0] = gMoves[79].pp;
        wPartyMons[0].base.pp[1] = gMoves[75].pp;
        wPartyMons[0].base.pp[2] = gMoves[73].pp;
        wPartyMons[0].base.pp[3] = gMoves[22].pp;
        wPartyMons[0].base.hp = wPartyMons[0].max_hp;
        wPartyMons[0].base.status = 0;

        printf("[amberscript] bulba15: slot 1 -> BULBASAUR Lv15 (SLEEP POWDER / RAZOR LEAF / LEECH SEED / VINE WHIP)\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "squirtle15") == 0 || strcmp(verb, "givemon_squirtle15") == 0) {
        uint8_t species = gDexToSpecies[7];
        Pokemon_InitMon(&wPartyMons[0], species, 15);
        if (wPartyCount < 1) wPartyCount = 1;

        wPartyMons[0].base.moves[0] = 145;
        wPartyMons[0].base.moves[1] = 55;
        wPartyMons[0].base.moves[2] = 33;
        wPartyMons[0].base.moves[3] = 39;
        wPartyMons[0].base.pp[0] = gMoves[145].pp;
        wPartyMons[0].base.pp[1] = gMoves[55].pp;
        wPartyMons[0].base.pp[2] = gMoves[33].pp;
        wPartyMons[0].base.pp[3] = gMoves[39].pp;
        wPartyMons[0].base.hp = wPartyMons[0].max_hp;
        wPartyMons[0].base.status = 0;

        printf("[amberscript] squirtle15: slot 1 -> SQUIRTLE Lv15 (BUBBLE / WATER GUN / TACKLE / TAIL WHIP)\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "teachmove") == 0) {
        int slot = 1;
        char move_str[16] = {0};
        sscanf(cmd, "%*s %d %15s", &slot, move_str);
        int move_id = (int)strtol(move_str, NULL, 0);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[amberscript] teachmove: slot must be 1-%d\n", wPartyCount);
        } else if (move_id < 1 || move_id > 0xA5) {
            printf("[amberscript] teachmove: invalid move id 0x%02X\n", move_id);
        } else {
            uint8_t *moves = wPartyMons[slot].base.moves;
            int target = 0;
            for (int i = 0; i < 4; i++) {
                if (moves[i] == 0) { target = i; break; }
                if (i == 3) target = 0;
            }
            moves[target] = (uint8_t)move_id;
            printf("[amberscript] teachmove: slot %d move[%d] = 0x%02X\n",
                   slot + 1, target, move_id);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "teach") == 0) {
        int slot = 1;
        char move_str[32] = {0};
        sscanf(cmd, "%*s %d %31s", &slot, move_str);
        slot--;

        int move_id = pks_resolve_move_id(move_str);

        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[amberscript] teach: slot must be 1-%d\n", wPartyCount);
        } else if (move_id < 1 || move_id > 0xA5) {
            printf("[amberscript] teach: unknown move '%s'\n", move_str);
        } else {
            uint8_t *moves = wPartyMons[slot].base.moves;
            uint8_t *pp    = wPartyMons[slot].base.pp;
            int target = 3;
            for (int i = 0; i < 4; i++) {
                if (moves[i] == 0) { target = i; break; }
            }
            moves[target] = (uint8_t)move_id;
            pp[target]    = gMoves[move_id].pp;
            printf("[amberscript] teach: slot %d move[%d] = 0x%02X (pp=%d)\n",
                   slot + 1, target, move_id, pp[target]);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "movetestteam1") == 0) {
        static const char *const kMoveNames24[24] = {
            "ABSORB", "ACID", "ACID ARMOR", "AGILITY",
            "AMNESIA", "AURORA BEAM", "BARRAGE", "BARRIER",
            "BIDE", "BIND", "BITE", "BLIZZARD",
            "BODY SLAM", "BONE CLUB", "BONEMERANG", "BUBBLE",
            "BUBBLEBEAM", "CLAMP", "COMET PUNCH", "CONFUSE RAY",
            "CONFUSION", "CONSTRICT", "CONVERSION", "COUNTER"
        };
        uint8_t move_ids[24];
        int ok = 1;
        for (int i = 0; i < 24; i++) {
            int id = pks_resolve_move_id(kMoveNames24[i]);
            if (id < 1 || id > 0xA5) {
                printf("[amberscript] movetestteam1: failed to resolve move '%s'\n", kMoveNames24[i]);
                ok = 0;
                break;
            }
            move_ids[i] = (uint8_t)id;
        }
        if (!ok) {
            AmberScript_WriteState();
            return 1;
        }

        wPartyCount = PARTY_LENGTH;
        for (int p = 0; p < PARTY_LENGTH; p++) {
            Pokemon_InitMon(&wPartyMons[p], STARTER1, 50);
            wPartyMons[p].base.status = 0;
            wPartyMons[p].base.hp = wPartyMons[p].max_hp;
            for (int m = 0; m < 4; m++) {
                uint8_t mid = move_ids[p * 4 + m];
                wPartyMons[p].base.moves[m] = mid;
                wPartyMons[p].base.pp[m] = gMoves[mid].pp;
            }
        }

        if (wPlayerMonNumber == 0) {
            for (int m = 0; m < 4; m++) {
                wBattleMon.moves[m] = wPartyMons[0].base.moves[m];
                wBattleMon.pp[m] = wPartyMons[0].base.pp[m];
            }
        }

        printf("[amberscript] movetestteam1: set 6-mon party with 24 move-animation test moves (slots 1-24)\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "movetestteam2") == 0) {
        static const char *const kMoveNames24[24] = {
            "CRABHAMMER", "CUT", "DEFENSE CURL", "DIG",
            "DISABLE", "DIZZY PUNCH", "DOUBLE KICK", "DOUBLE SLAP",
            "DOUBLE TEAM", "DOUBLE-EDGE", "DRAGON RAGE", "DREAM EATER",
            "DRILL PECK", "EARTHQUAKE", "EGG BOMB", "EMBER",
            "EXPLOSION", "FIRE BLAST", "FIRE PUNCH", "FIRE SPIN",
            "FISSURE", "FLAMETHROWER", "FLASH", "FLY"
        };
        static const uint8_t kSpecies[PARTY_LENGTH] = {
            SPECIES_BULBASAUR,
            SPECIES_CHARMANDER,
            SPECIES_SQUIRTLE,
            SPECIES_PIKACHU,
            SPECIES_EEVEE,
            SPECIES_DRATINI
        };
        uint8_t move_ids[24];
        int ok = 1;
        for (int i = 0; i < 24; i++) {
            int id = pks_resolve_move_id(kMoveNames24[i]);
            if (id < 1 || id > 0xA5) {
                printf("[amberscript] movetestteam2: failed to resolve move '%s'\n", kMoveNames24[i]);
                ok = 0;
                break;
            }
            move_ids[i] = (uint8_t)id;
        }
        if (!ok) {
            AmberScript_WriteState();
            return 1;
        }

        wPartyCount = PARTY_LENGTH;
        for (int p = 0; p < PARTY_LENGTH; p++) {
            Pokemon_InitMon(&wPartyMons[p], kSpecies[p], 50);
            wPartyMons[p].base.status = 0;
            wPartyMons[p].base.hp = wPartyMons[p].max_hp;
            for (int m = 0; m < 4; m++) {
                uint8_t mid = move_ids[p * 4 + m];
                wPartyMons[p].base.moves[m] = mid;
                wPartyMons[p].base.pp[m] = gMoves[mid].pp;
            }
        }

        if (wPlayerMonNumber == 0) {
            for (int m = 0; m < 4; m++) {
                wBattleMon.moves[m] = wPartyMons[0].base.moves[m];
                wBattleMon.pp[m] = wPartyMons[0].base.pp[m];
            }
        }

        printf("[amberscript] movetestteam2: set 6 unique mons with move-animation test moves (slots 25-48)\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "sethealth") == 0) {
        int slot = 0, hp = 0;
        sscanf(cmd, "%*s %d %d", &slot, &hp);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[amberscript] sethealth: slot must be 1-%d\n", wPartyCount);
        } else {
            if (hp < 0) hp = 0;
            if (hp > (int)wPartyMons[slot].max_hp) hp = (int)wPartyMons[slot].max_hp;
            wPartyMons[slot].base.hp = (uint16_t)hp;
            if (slot == (int)wPlayerMonNumber) wBattleMon.hp = (uint16_t)hp;
            printf("[amberscript] sethealth: slot %d HP -> %d/%d\n",
                   slot + 1, hp, (int)wPartyMons[slot].max_hp);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "healparty") == 0) {
        for (int i = 0; i < (int)wPartyCount; i++) {
            wPartyMons[i].base.hp     = wPartyMons[i].max_hp;
            wPartyMons[i].base.status = 0;
        }
        printf("[amberscript] healparty: all %d mons fully healed\n", wPartyCount);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "pc_send") == 0) {
        int slot = 0;
        sscanf(cmd, "%*s %d", &slot);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[amberscript] pc_send: slot must be 1-%d\n", wPartyCount);
        } else if (wPartyCount <= 1) {
            printf("[amberscript] pc_send: can't deposit your last party mon\n");
        } else if (!Pokemon_DepositPartyMonToBox(slot)) {
            printf("[amberscript] pc_send: current box is full\n");
        } else {
            printf("[amberscript] pc_send: deposited party slot %d to BOX %d\n",
                   slot + 1, (wCurrentBoxNum % NUM_BOXES) + 1);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "setmoney") == 0) {
        int amount = 0;
        sscanf(cmd, "%*s %d", &amount);
        if (amount < 0)      amount = 0;
        if (amount > 999999) amount = 999999;
        int a = amount;
        wPlayerMoney[2] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4)); a /= 100;
        wPlayerMoney[1] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4)); a /= 100;
        wPlayerMoney[0] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4));
        printf("[amberscript] setmoney: $%d\n", amount);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "listbag") == 0) {
        printf("[amberscript] bag (%d items):\n", wNumBagItems);
        for (int i = 0; i < (int)wNumBagItems; i++) {
            uint8_t id  = wBagItems[i * 2];
            uint8_t qty = wBagItems[i * 2 + 1];
            char name[20];
            Inventory_DecodeASCII(id, name, sizeof(name));
            printf("  [%d] 0x%02X %-18s x%d\n", i + 1, id, name, qty);
        }
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
