#include "scenario.h"
#include "intro.h"
#include "pokemon.h"
#include "pokedex.h"
#include "overworld.h"
#include "pokecenter.h"
#include "town_map.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../data/moves_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int Game_EnterScenarioOverworld(const char *, int, int, int);

static void set_error(char *dst, size_t size, int line, const char *message) {
    if (line) snprintf(dst, size, "line %d: %s", line, message);
    else snprintf(dst, size, "%s", message);
}

static void set_bcd(uint8_t *dst, int bytes, uint32_t value) {
    for (int i = bytes - 1; i >= 0; i--) {
        unsigned lo = value % 10; value /= 10;
        unsigned hi = value % 10; value /= 10;
        dst[i] = (uint8_t)((hi << 4) | lo);
    }
}

static int apply_name_hex(const char *hex, uint8_t dst[NAME_LENGTH]) {
    if (!hex || !strcmp(hex, "-")) return 1;
    if (strlen(hex) != NAME_LENGTH * 2) return 0;
    for (int i = 0; i < NAME_LENGTH; i++) {
        char pair[3] = {hex[i * 2], hex[i * 2 + 1], 0};
        char *end;
        unsigned long value = strtoul(pair, &end, 16);
        if (*end) return 0;
        dst[i] = (uint8_t)value;
    }
    return 1;
}

static unsigned long exp_value(const uint8_t exp[3]) {
    return ((unsigned long)exp[0] << 16) |
           ((unsigned long)exp[1] << 8) |
           (unsigned long)exp[2];
}

static unsigned long bcd_value(const uint8_t *src, int bytes) {
    unsigned long value = 0;
    for (int i = 0; i < bytes; i++) {
        value = value * 100 + ((src[i] >> 4) & 0xf) * 10 + (src[i] & 0xf);
    }
    return value;
}

static void apply_stat_exp(box_mon_t *mon, char **fields, int first) {
    long value;
    value = strtol(fields[first], NULL, 0); if (value >= 0) mon->stat_exp_hp = (uint16_t)value;
    value = strtol(fields[first + 1], NULL, 0); if (value >= 0) mon->stat_exp_atk = (uint16_t)value;
    value = strtol(fields[first + 2], NULL, 0); if (value >= 0) mon->stat_exp_def = (uint16_t)value;
    value = strtol(fields[first + 3], NULL, 0); if (value >= 0) mon->stat_exp_spd = (uint16_t)value;
    value = strtol(fields[first + 4], NULL, 0); if (value >= 0) mon->stat_exp_spc = (uint16_t)value;
}

static long apply_mon_metadata(box_mon_t *mon, char **fields, int first) {
    long value;
    long requested_hp;
    value = strtol(fields[first], NULL, 0); if (value >= 0) mon->status = (uint8_t)value;
    requested_hp = strtol(fields[first + 1], NULL, 0);
    value = strtol(fields[first + 2], NULL, 0); if (value >= 0) mon->dvs = (uint16_t)value;
    apply_stat_exp(mon, fields, first + 3);
    value = strtol(fields[first + 8], NULL, 0); if (value >= 0) mon->ot_id = (uint16_t)value;
    value = strtol(fields[first + 9], NULL, 0);
    if (value >= 0) {
        unsigned long exp = (unsigned long)value;
        mon->exp[0] = (uint8_t)(exp >> 16);
        mon->exp[1] = (uint8_t)(exp >> 8);
        mon->exp[2] = (uint8_t)exp;
    }
    for (int i = 0; i < 4; i++) {
        value = strtol(fields[first + 10 + i], NULL, 0);
        if (value >= 0) mon->pp[i] = (uint8_t)value;
    }
    return requested_hp;
}

static void set_box_default_hp(box_mon_t *mon) {
    party_mon_t temp;
    memset(&temp, 0, sizeof temp);
    temp.base = *mon;
    temp.level = mon->box_level;
    if (Pokemon_RecalculatePartyData(&temp)) mon->hp = temp.max_hp;
}

static void write_mon_json(FILE *fp, const box_mon_t *mon) {
    fprintf(fp,
            "{\"species\":%u,\"level\":%u,\"status\":%u,\"current_hp\":%u,"
            "\"dvs\":%u,\"stat_exp\":[%u,%u,%u,%u,%u],\"ot_id\":%u,"
            "\"experience\":%lu,\"moves\":[%u,%u,%u,%u],\"pp\":[%u,%u,%u,%u]}",
            mon->species, mon->box_level, mon->status, mon->hp, mon->dvs,
            mon->stat_exp_hp, mon->stat_exp_atk, mon->stat_exp_def,
            mon->stat_exp_spd, mon->stat_exp_spc, mon->ot_id,
            exp_value(mon->exp), mon->moves[0], mon->moves[1], mon->moves[2],
            mon->moves[3], mon->pp[0], mon->pp[1], mon->pp[2], mon->pp[3]);
}

static void write_hex_json(FILE *fp, const uint8_t *data, size_t size) {
    fputc('"', fp);
    for (size_t i = 0; i < size; i++) fprintf(fp, "%02x", data[i]);
    fputc('"', fp);
}

static void write_items_json(FILE *fp, const uint8_t *items, int count) {
    fputc('[', fp);
    for (int i = 0; i < count; i++) {
        if (i) fputc(',', fp);
        fprintf(fp, "{\"id\":%u,\"quantity\":%u}", items[i * 2], items[i * 2 + 1]);
    }
    fputc(']', fp);
}

static void write_applied_report(const char *map, int x, int y, int facing,
                                 const char *base) {
    extern unsigned long gPlayTimeFrames;
    uint8_t visited[2];
    TownMap_GetVisited(visited);
    FILE *fp = fopen("scenario_applied.json", "w");
    if (!fp) return;
    fprintf(fp,
            "{\n  \"base\":\"%s\",\n  \"location\":{\"vmap\":\"%s\",\"x\":%d,\"y\":%d,\"facing\":%d},\n"
            "  \"player\":{\"trainer_id\":%u,\"money\":%lu,\"coins\":%lu,\"badges\":%u},\n"
            "  \"options\":%u,\n  \"play_time_frames\":%lu,\n  \"movement\":%u,\n"
            "  \"party\":[",
            base, map, x, y, facing, wPlayerID, bcd_value(wPlayerMoney, 3),
            bcd_value(wPlayerCoins, 2), wObtainedBadges, wOptions,
            gPlayTimeFrames, wWalkBikeSurfState);
    for (int i = 0; i < wPartyCount; i++) {
        if (i) fputc(',', fp);
        write_mon_json(fp, &wPartyMons[i].base);
    }
    fprintf(fp, "],\n  \"current_box\":%u,\n  \"boxes\":[", wCurrentBoxNum + 1);
    for (int box = 0; box < NUM_BOXES; box++) {
        if (box) fputc(',', fp);
        fprintf(fp, "{\"number\":%d,\"pokemon\":[", box + 1);
        for (int slot = 0; slot < wBoxCount[box]; slot++) {
            if (slot) fputc(',', fp);
            write_mon_json(fp, &wBoxMons[box][slot]);
        }
        fprintf(fp, "]}");
    }
    fprintf(fp, "],\n  \"bag\":");
    write_items_json(fp, wBagItems, wNumBagItems);
    fprintf(fp, ",\n  \"pc_items\":");
    write_items_json(fp, wBoxItems, wNumBoxItems);
    fprintf(fp, ",\n  \"pokedex_seen_bits\":");
    write_hex_json(fp, wPokedexSeen, sizeof(wPokedexSeen));
    fprintf(fp, ",\n  \"pokedex_owned_bits\":");
    write_hex_json(fp, wPokedexOwned, sizeof(wPokedexOwned));
    fprintf(fp, ",\n  \"event_bits\":");
    write_hex_json(fp, wEventFlags, sizeof(wEventFlags));
    fprintf(fp, ",\n  \"hand_authored_event_bits\":");
    write_hex_json(fp, wHandAuthoredEventFlags, sizeof(wHandAuthoredEventFlags));
    fprintf(fp, ",\n  \"picked_up_item_bits\":");
    write_hex_json(fp, (const uint8_t *)wPickedUpItems, sizeof(wPickedUpItems));
    fprintf(fp, ",\n  \"completed_trade_bits\":%u,\n  \"visited_town_bits\":%u",
            wCompletedInGameTradeFlags,
            (unsigned)visited[0] | ((unsigned)visited[1] << 8));
    fprintf(fp, ",\n  \"rng\":{\"add\":%u,\"sub\":%u},\n", hRandomAdd, hRandomSub);
    fprintf(fp, "  \"respawn\":{\"used_pokecenter\":%d,\"map_id\":%u,\"vmap\":\"%s\"},\n",
            Pokecenter_GetUsedFlag(), wLastHealTownMap, wLastHealTownName);
    fprintf(fp, "  \"rival_starter\":%u,\n  \"daycare\":", wRivalStarter);
    if (wDayCareInUse) write_mon_json(fp, &wDayCareMon);
    else fputs("null", fp);
    fprintf(fp, "\n}\n");
    fclose(fp);
}

int Scenario_LoadAndStart(const char *path, char *error, size_t error_size) {
    FILE *fp = fopen(path, "r");
    char buf[1024], map[64] = "RedsHouse2F";
    int line = 0, header = 0, base_seen = 0, fresh = 1;
    int x = 3, y = 6, facing = 0;
    int rng_seen = 0;
    uint8_t rng_add = 0, rng_sub = 0;
    int respawn_mode = -1;
    char respawn_map[64] = "";
    if (!fp) { set_error(error,error_size,0,"could not open scenario state"); return 0; }

    while (fgets(buf, sizeof buf, fp)) {
        char *f[24] = {0}, *p = buf;
        int n = 0;
        line++;
        buf[strcspn(buf, "\r\n")] = 0;
        if (!buf[0]) continue;
        do { f[n++] = p; p = strchr(p, '\t'); if (p) *p++ = 0; } while (p && n < 24);

        if (!strcmp(f[0], "scenario")) {
            if (n != 2 || strcmp(f[1], "1")) { set_error(error,error_size,line,"unsupported scenario version"); goto bad; }
            header = 1;
        } else if (!strcmp(f[0], "base")) {
            if (n != 2 || (strcmp(f[1],"fresh") && strcmp(f[1],"current_save"))) { set_error(error,error_size,line,"invalid base"); goto bad; }
            fresh = !strcmp(f[1], "fresh"); base_seen = 1;
            if (fresh) Intro_InitPlayerDataForScenario();
        } else if (!strcmp(f[0], "player")) {
            if (n != 7) { set_error(error,error_size,line,"invalid player record"); goto bad; }
            Pokemon_EncodeNameString(f[1], wPlayerName);

            for (int i = 0; i + 1 < NAME_LENGTH; i++) {
                if (wPlayerName[i] == 0x50) {
                    memset(wPlayerName + i + 1, 0, NAME_LENGTH - i - 1);
                    break;
                }
            }
            Pokemon_EncodeNameString(f[2], wRivalName);
            wPlayerID = (uint16_t)strtoul(f[3],NULL,0);
            set_bcd(wPlayerMoney,3,(uint32_t)strtoul(f[4],NULL,0));
            set_bcd(wPlayerCoins,2,(uint32_t)strtoul(f[5],NULL,0));
            wObtainedBadges = (uint8_t)strtoul(f[6],NULL,0);
        } else if (!strcmp(f[0], "location")) {
            if (n != 5) { set_error(error,error_size,line,"invalid location record"); goto bad; }
            snprintf(map,sizeof map,"%s",f[1]); x=atoi(f[2]); y=atoi(f[3]); facing=atoi(f[4]);
        } else if (!strcmp(f[0], "respawn")) {
            if (n != 2) { set_error(error,error_size,line,"invalid respawn record"); goto bad; }
            respawn_mode = strcmp(f[1], "-") ? 1 : 0;
            if (respawn_mode) snprintf(respawn_map, sizeof respawn_map, "%s", f[1]);
        } else if (!strcmp(f[0], "clear_party")) {
            wPartyCount=0; memset(wPartyMons,0,sizeof(wPartyMons)); memset(wPartySpecies,0xff,sizeof(wPartySpecies));
        } else if (!strcmp(f[0], "clear_bag")) {
            wNumBagItems=0; memset(wBagItems,0,sizeof(wBagItems)); wBagItems[0]=0xff;
        } else if (!strcmp(f[0], "clear_pc_items")) {
            wNumBoxItems=0; memset(wBoxItems,0,sizeof(wBoxItems)); wBoxItems[0]=0xff;
        } else if (!strcmp(f[0], "clear_dex")) {
            memset(wPokedexSeen,0,sizeof(wPokedexSeen)); memset(wPokedexOwned,0,sizeof(wPokedexOwned));
        } else if (!strcmp(f[0], "clear_boxes")) {
            wCurrentBoxNum=0; memset(wBoxCount,0,sizeof(wBoxCount)); memset(wBoxSpecies,0xff,sizeof(wBoxSpecies)); memset(wBoxMons,0,sizeof(wBoxMons)); memset(wBoxMonOT,0,sizeof(wBoxMonOT)); memset(wBoxMonNicks,0,sizeof(wBoxMonNicks));
        } else if (!strcmp(f[0], "party")) {
            if (n != 11 || wPartyCount >= PARTY_LENGTH) { set_error(error,error_size,line,"invalid party record"); goto bad; }
            uint8_t species=(uint8_t)strtoul(f[1],NULL,0), level=(uint8_t)strtoul(f[2],NULL,0);
            Pokemon_AddToParty(species,level);
            if (!wPartyCount || wPartyMons[wPartyCount-1].base.species != species) { set_error(error,error_size,line,"species could not be created"); goto bad; }
            party_mon_t *mon=&wPartyMons[wPartyCount-1];
            for(int i=0;i<4;i++){ long mv=strtol(f[3+i],NULL,0); if(mv>=0){mon->base.moves[i]=(uint8_t)mv;mon->base.pp[i]=(mv<NUM_MOVE_DEFS)?gMoves[mv].pp:0;} }
            if (strcmp(f[7],"-")) Pokemon_EncodeNameString(f[7],wPartyMonNicks[wPartyCount-1]);
            if (strcmp(f[8],"-")) Pokemon_EncodeNameString(f[8],wPartyMonOT[wPartyCount-1]);
            if(!apply_name_hex(f[9],wPartyMonNicks[wPartyCount-1])||!apply_name_hex(f[10],wPartyMonOT[wPartyCount-1])){set_error(error,error_size,line,"invalid encoded party name");goto bad;}
        } else if (!strcmp(f[0], "party_meta")) {
            if (n != 16) { set_error(error,error_size,line,"invalid party metadata"); goto bad; }
            int slot=atoi(f[1]); if(slot<0||slot>=wPartyCount){set_error(error,error_size,line,"party metadata slot missing");goto bad;}
            party_mon_t *mon=&wPartyMons[slot]; box_mon_t *b=&mon->base;
            long requested_hp=apply_mon_metadata(b,f,2);
            Pokemon_RecalculatePartyMon(slot); b->hp=requested_hp>=0?(uint16_t)requested_hp:mon->max_hp;
        } else if (!strcmp(f[0], "box")) {
            if(n!=12){set_error(error,error_size,line,"invalid box record");goto bad;} int box=atoi(f[1]); if(box<0||box>=NUM_BOXES){set_error(error,error_size,line,"invalid box number");goto bad;}
            wCurrentBoxNum=(uint8_t)box; uint8_t species=(uint8_t)strtoul(f[2],NULL,0),level=(uint8_t)atoi(f[3]); if(!Pokemon_AddToBox(species,level)){set_error(error,error_size,line,"box monster could not be created");goto bad;}
            int slot=wBoxCount[box]-1; box_mon_t *mon=&wBoxMons[box][slot]; for(int i=0;i<4;i++){long mv=strtol(f[4+i],NULL,0);if(mv>=0){mon->moves[i]=(uint8_t)mv;mon->pp[i]=(mv<NUM_MOVE_DEFS)?gMoves[mv].pp:0;}} if(strcmp(f[8],"-"))Pokemon_EncodeNameString(f[8],wBoxMonNicks[box][slot]);
            if(strcmp(f[9],"-"))Pokemon_EncodeNameString(f[9],wBoxMonOT[box][slot]);
            if(!apply_name_hex(f[10],wBoxMonNicks[box][slot])||!apply_name_hex(f[11],wBoxMonOT[box][slot])){set_error(error,error_size,line,"invalid encoded box name");goto bad;}
        } else if (!strcmp(f[0], "box_meta")) {
            if(n!=17){set_error(error,error_size,line,"invalid box metadata");goto bad;} int box=atoi(f[1]),slot=atoi(f[2]); if(box<0||box>=NUM_BOXES||slot<0||slot>=wBoxCount[box]){set_error(error,error_size,line,"box metadata slot missing");goto bad;}
            box_mon_t *b=&wBoxMons[box][slot];long requested_hp=apply_mon_metadata(b,f,3);if(requested_hp>=0)b->hp=(uint16_t)requested_hp;else set_box_default_hp(b);
        } else if (!strcmp(f[0], "current_box")) {
            if(n!=2){set_error(error,error_size,line,"invalid current box");goto bad;} wCurrentBoxNum=(uint8_t)atoi(f[1]);
        } else if (!strcmp(f[0], "clear_daycare")) {
            wDayCareInUse=0; memset(&wDayCareMon,0,sizeof(wDayCareMon)); memset(wDayCareMonOT,0,sizeof(wDayCareMonOT)); memset(wDayCareMonName,0,sizeof(wDayCareMonName));
        } else if (!strcmp(f[0], "daycare")) {
            if(n!=11){set_error(error,error_size,line,"invalid daycare record");goto bad;}
            party_mon_t temp; Pokemon_InitMon(&temp,(uint8_t)strtoul(f[1],NULL,0),(uint8_t)atoi(f[2])); wDayCareMon=temp.base; wDayCareInUse=1;
            for(int i=0;i<4;i++){long mv=strtol(f[3+i],NULL,0);if(mv>=0){wDayCareMon.moves[i]=(uint8_t)mv;wDayCareMon.pp[i]=(mv<NUM_MOVE_DEFS)?gMoves[mv].pp:0;}}
            Pokemon_EncodeNameString(strcmp(f[7],"-")?f[7]:Pokemon_GetNameBySpecies(wDayCareMon.species),wDayCareMonName);
            if(strcmp(f[8],"-"))Pokemon_EncodeNameString(f[8],wDayCareMonOT);else memcpy(wDayCareMonOT,wPlayerName,NAME_LENGTH);
            if(!apply_name_hex(f[9],wDayCareMonName)||!apply_name_hex(f[10],wDayCareMonOT)){set_error(error,error_size,line,"invalid encoded daycare name");goto bad;}
        } else if (!strcmp(f[0], "daycare_meta")) {
            if(n!=15||!wDayCareInUse){set_error(error,error_size,line,"invalid daycare metadata");goto bad;} box_mon_t *b=&wDayCareMon;long requested_hp=apply_mon_metadata(b,f,1);if(requested_hp>=0)b->hp=(uint16_t)requested_hp;else set_box_default_hp(b);
        } else if (!strcmp(f[0], "options")) {
            if(n!=2){set_error(error,error_size,line,"invalid options");goto bad;} wOptions=(uint8_t)strtoul(f[1],NULL,0);
        } else if (!strcmp(f[0], "play_time")) {
            if(n!=2){set_error(error,error_size,line,"invalid play time");goto bad;} extern unsigned long gPlayTimeFrames; gPlayTimeFrames=strtoul(f[1],NULL,0);
        } else if (!strcmp(f[0], "movement")) {
            if(n!=2){set_error(error,error_size,line,"invalid movement");goto bad;} wWalkBikeSurfState=(uint8_t)atoi(f[1]);
        } else if (!strcmp(f[0], "rival_starter")) {
            if(n!=2){set_error(error,error_size,line,"invalid rival starter");goto bad;} wRivalStarter=(uint8_t)strtoul(f[1],NULL,0);
        } else if (!strcmp(f[0], "bag") || !strcmp(f[0], "pc_item")) {
            if (n != 3) { set_error(error,error_size,line,"invalid inventory record"); goto bad; }
            uint8_t *count=!strcmp(f[0],"bag")?&wNumBagItems:&wNumBoxItems;
            uint8_t *items=!strcmp(f[0],"bag")?wBagItems:wBoxItems;
            int cap=!strcmp(f[0],"bag")?BAG_ITEM_CAPACITY:PC_ITEM_CAPACITY;
            if (*count >= cap) { set_error(error,error_size,line,"inventory capacity exceeded"); goto bad; }
            items[*count*2]=(uint8_t)strtoul(f[1],NULL,0); items[*count*2+1]=(uint8_t)strtoul(f[2],NULL,0); (*count)++; items[*count*2]=0xff;
        } else if (!strcmp(f[0], "event")) {
            if (n != 3) { set_error(error,error_size,line,"invalid event record"); goto bad; }
            uint16_t id=(uint16_t)strtoul(f[1],NULL,0); if(atoi(f[2])) SetEvent(id); else ClearEvent(id);
        } else if (!strcmp(f[0], "clear_progress_flags")) {
            uint8_t visited[2] = {0, 0};
            wCompletedInGameTradeFlags = 0;
            memset(wPickedUpItems, 0, sizeof(wPickedUpItems));
            TownMap_SetVisited(visited);
        } else if (!strcmp(f[0], "completed_trades")) {
            if (n != 2) { set_error(error,error_size,line,"invalid completed-trades record"); goto bad; }
            wCompletedInGameTradeFlags = (uint16_t)strtoul(f[1],NULL,0);
        } else if (!strcmp(f[0], "visited_towns")) {
            uint16_t mask;
            uint8_t visited[2];
            if (n != 2) { set_error(error,error_size,line,"invalid visited-towns record"); goto bad; }
            mask = (uint16_t)strtoul(f[1],NULL,0);
            visited[0] = (uint8_t)mask;
            visited[1] = (uint8_t)(mask >> 8);
            TownMap_SetVisited(visited);
        } else if (!strcmp(f[0], "picked_up_items")) {
            int map_id;
            if (n != 3) { set_error(error,error_size,line,"invalid picked-up-items record"); goto bad; }
            map_id = atoi(f[1]);
            if (map_id < 0 || map_id >= 248) { set_error(error,error_size,line,"picked-up-items map id out of range"); goto bad; }
            wPickedUpItems[map_id] = (uint16_t)strtoul(f[2],NULL,0);
        } else if (!strcmp(f[0], "dex")) {
            if (n != 3) { set_error(error,error_size,line,"invalid dex record"); goto bad; }
            int species=atoi(f[1]); if(atoi(f[2])==2) Pokedex_SetOwned(species); else Pokedex_SetSeen(species);
        } else if (!strcmp(f[0], "rng")) {
            if (n != 3) { set_error(error,error_size,line,"invalid rng record"); goto bad; }
            rng_add=(uint8_t)strtoul(f[1],NULL,0); rng_sub=(uint8_t)strtoul(f[2],NULL,0); rng_seen=1;
        } else { set_error(error,error_size,line,"unknown scenario record"); goto bad; }
    }
    fclose(fp);
    if (!header || !base_seen) { set_error(error,error_size,0,"scenario header or base missing"); return 0; }
    if (!Game_EnterScenarioOverworld(map,x,y,facing)) { set_error(error,error_size,0,"could not enter requested location"); return 0; }
    if (respawn_mode == 0) {
        wLastBlackoutMap=0xff; wLastHealTownMap=0; wLastHealTownName[0]=0; Pokecenter_SetUsedFlag(0);
    } else if (respawn_mode == 1) {
        int real=Map_RealIdForName(respawn_map), tx, ty;
        if(real<0||!TownMap_GetFlyDest((uint8_t)real,&tx,&ty)){set_error(error,error_size,0,"respawn vmap is not a valid healing town");return 0;}
        wLastBlackoutMap=(uint8_t)real; wLastHealTownMap=(uint8_t)real; snprintf(wLastHealTownName,sizeof(wLastHealTownName),"%s",respawn_map); Pokecenter_SetUsedFlag(1);
    }
    if (rng_seen) { hRandomAdd = rng_add; hRandomSub = rng_sub; }
    write_applied_report(map, x, y, facing, fresh ? "fresh" : "current_save");
    printf("[scenario] loaded %s at %s (%d,%d), base=%s\n",path,map,x,y,fresh?"fresh":"current_save");
    return 1;
bad:
    fclose(fp);
    return 0;
}
