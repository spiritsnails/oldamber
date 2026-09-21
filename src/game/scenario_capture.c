#include "scenario.h"
#include "amberscript_mapbank.h"
#include "constants.h"
#include "overworld.h"
#include "player.h"
#include "pokecenter.h"
#include "town_map.h"
#include "../data/base_stats.h"
#include "../platform/game_version.h"
#include "../platform/hardware.h"
#include <stdio.h>
#include <string.h>

static unsigned long capture_exp(const uint8_t exp[3]) {
    return ((unsigned long)exp[0] << 16) | ((unsigned long)exp[1] << 8) | exp[2];
}

static unsigned long capture_bcd(const uint8_t *src, int bytes) {
    unsigned long value = 0;
    for (int i = 0; i < bytes; i++)
        value = value * 100 + ((src[i] >> 4) & 15) * 10 + (src[i] & 15);
    return value;
}

static void decode_name(const uint8_t *in, char *out, size_t out_size) {
    size_t n = 0;
    for (int i = 0; i < NAME_LENGTH && n + 1 < out_size; i++) {
        uint8_t value = in[i];
        char ch;
        if (value == 0 || value == 0x50) break;
        if (value >= 0x80 && value <= 0x99) ch = (char)('A' + value - 0x80);
        else if (value >= 0xa0 && value <= 0xb9) ch = (char)('a' + value - 0xa0);
        else if (value >= 0xf6) ch = (char)('0' + value - 0xf6);
        else if (value == 0x7f) ch = ' ';
        else if (value == 0xe8) ch = '.';
        else if (value == 0xe3) ch = '-';
        else if (value == 0xe0) ch = '\'';
        else ch = '?';
        out[n++] = ch;
    }
    out[n] = 0;
}

static void yaml_string(FILE *fp, const char *value) {
    fputc('"', fp);
    for (; value && *value; value++) {
        if (*value == '"' || *value == '\\') fputc('\\', fp);
        if (*value == '\n') fputs("\\n", fp);
        else fputc(*value, fp);
    }
    fputc('"', fp);
}

static void write_name_hex(FILE *fp, const uint8_t value[NAME_LENGTH]) {
    fputc('"', fp);
    for (int i = 0; i < NAME_LENGTH; i++) fprintf(fp, "%02x", value[i]);
    fputc('"', fp);
}

static void write_mon(FILE *fp, const box_mon_t *mon, const uint8_t *nick,
                      const uint8_t *ot, const char *indent, int bullet) {
    char nickname[24], ot_name[24];
    char rest[24];
    decode_name(nick, nickname, sizeof nickname);
    decode_name(ot, ot_name, sizeof ot_name);
    snprintf(rest, sizeof rest, "%s%s", indent, bullet ? "  " : "");
    fprintf(fp, "%s%sspecies: %u\n%slevel: %u\n%smoves: [%u, %u, %u, %u]\n",
            indent, bullet ? "- " : "", mon->species, rest, mon->box_level, rest,
            mon->moves[0], mon->moves[1], mon->moves[2], mon->moves[3]);
    fprintf(fp, "%snickname: ", rest); yaml_string(fp, nickname); fputc('\n', fp);
    fprintf(fp, "%snickname_bytes: ", rest); write_name_hex(fp, nick); fputc('\n', fp);
    fprintf(fp, "%sot_name: ", rest); yaml_string(fp, ot_name); fputc('\n', fp);
    fprintf(fp, "%sot_name_bytes: ", rest); write_name_hex(fp, ot); fputc('\n', fp);
    fprintf(fp,
            "%sstatus: %u\n%scurrent_hp: %u\n%sdvs: %u\n"
            "%sstat_exp: {hp: %u, attack: %u, defense: %u, speed: %u, special: %u}\n"
            "%sot_id: %u\n%sexperience: %lu\n"
            "%spp: [{current: %u, pp_ups: %u}, {current: %u, pp_ups: %u}, "
            "{current: %u, pp_ups: %u}, {current: %u, pp_ups: %u}]\n",
            rest, mon->status, rest, mon->hp, rest, mon->dvs,
            rest, mon->stat_exp_hp, mon->stat_exp_atk, mon->stat_exp_def,
            mon->stat_exp_spd, mon->stat_exp_spc, rest, mon->ot_id,
            rest, capture_exp(mon->exp), rest,
            mon->pp[0] & 63, mon->pp[0] >> 6, mon->pp[1] & 63, mon->pp[1] >> 6,
            mon->pp[2] & 63, mon->pp[2] >> 6, mon->pp[3] & 63, mon->pp[3] >> 6);
}

static void write_items(FILE *fp, const char *key, const uint8_t *items, int count) {
    fprintf(fp, "\n%s:\n", key);
    if (!count) fputs("  []\n", fp);
    for (int i = 0; i < count; i++)
        fprintf(fp, "  - item: %u\n    quantity: %u\n", items[i * 2], items[i * 2 + 1]);
}

static void write_dex_list(FILE *fp, const char *key, const uint8_t bits[19]) {
    fprintf(fp, "  %s: [", key);
    int first = 1;
    for (int dex = 1; dex <= 151; dex++) {
        if (!(bits[(dex - 1) >> 3] & (1u << ((dex - 1) & 7)))) continue;
        fprintf(fp, "%s%u", first ? "" : ", ", gDexToSpecies[dex]);
        first = 0;
    }
    fputs("]\n", fp);
}

int Scenario_CaptureCurrent(const char *path, char *error, size_t error_size) {
    extern unsigned long gPlayTimeFrames;
    FILE *fp;
    const char *map_name = AmberScript_MapBank_NameForRealId(wCurMap);
    char player[24], rival[24];
    static const char *badge_names[8] = {
        "BOULDER", "CASCADE", "THUNDER", "RAINBOW",
        "SOUL", "MARSH", "VOLCANO", "EARTH"
    };
    if (!path || !path[0]) { snprintf(error,error_size,"capture path is empty"); return -1; }
    if (!map_name) { snprintf(error,error_size,"current map has no stable vmap name"); return -1; }
    fp = fopen(path, "w");
    if (!fp) { snprintf(error,error_size,"could not create capture file"); return -1; }
    decode_name(wPlayerName, player, sizeof player);
    decode_name(wRivalName, rival, sizeof rival);

    fprintf(fp, "schema: 1\ngame: %s\nbase: fresh\n\nplayer:\n  name: ", GameVersion_Current());
    yaml_string(fp, player); fputs("\n  rival_name: ", fp); yaml_string(fp, rival);
    fprintf(fp, "\n  trainer_id: %u\n  money: %lu\n  coins: %lu\n  badges: [",
            wPlayerID, capture_bcd(wPlayerMoney,3), capture_bcd(wPlayerCoins,2));
    int first = 1;
    for (int i = 0; i < 8; i++) if (wObtainedBadges & (1u << i)) {
        fprintf(fp, "%s%s", first ? "" : ", ", badge_names[i]); first = 0;
    }
    fprintf(fp, "]\n\nlocation:\n  vmap: "); yaml_string(fp, map_name);
    fprintf(fp, "\n  x: %u\n  y: %u\n  facing: %s\n",
            wXCoord, wYCoord, (gPlayerFacing&3)==0?"down":(gPlayerFacing&3)==1?"up":(gPlayerFacing&3)==2?"left":"right");

    fputs("\nrespawn:", fp);
    if (!Pokecenter_GetUsedFlag() || !wLastHealTownName[0]) fputs(" null\n", fp);
    else { fputs("\n  last_healed_vmap: ", fp); yaml_string(fp,wLastHealTownName); fputc('\n',fp); }

    fputs("\nparty:\n", fp);
    if (!wPartyCount) fputs("  []\n", fp);
    for (int i = 0; i < wPartyCount; i++) {
        write_mon(fp,&wPartyMons[i].base,wPartyMonNicks[i],wPartyMonOT[i],"  ",1);
    }

    fprintf(fp, "\nboxes:\n  current: %u\n  pokemon:\n", (wCurrentBoxNum % NUM_BOXES) + 1);
    int boxed = 0;
    for (int box = 0; box < NUM_BOXES; box++) for (int slot = 0; slot < wBoxCount[box]; slot++) {
        fprintf(fp, "    - box: %d\n", box + 1);
        write_mon(fp,&wBoxMons[box][slot],wBoxMonNicks[box][slot],wBoxMonOT[box][slot],"      ",0);
        boxed++;
    }
    if (!boxed) fputs("    []\n", fp);

    fputs("\ndaycare:", fp);
    if (!wDayCareInUse) fputs(" null\n", fp);
    else { fputc('\n',fp); write_mon(fp,&wDayCareMon,wDayCareMonName,wDayCareMonOT,"  ",0); }
    write_items(fp,"bag",wBagItems,wNumBagItems);
    write_items(fp,"pc_items",wBoxItems,wNumBoxItems);

    fputs("\npokedex:\n", fp);
    write_dex_list(fp,"seen",wPokedexSeen);
    write_dex_list(fp,"caught",wPokedexOwned);
    fputs("\nevents:\n  set: [", fp); first=1;
    for (int event = 0; event < PKS_HANDAUTHORED_EVENT_BASE + PKS_HANDAUTHORED_EVENT_COUNT; event++) {
        if (!CheckEvent((uint16_t)event)) continue;
        fprintf(fp, "%s%d", first ? "" : ", ", event); first=0;
    }
    fputs("]\n  clear: []\n", fp);

    {
        uint8_t visited[2];
        TownMap_GetVisited(visited);
        fputs("\nprogress_flags:\n  completed_trades: [", fp);
        first = 1;
        for (int trade = 0; trade < 16; trade++) {
            if (!(wCompletedInGameTradeFlags & (1u << trade))) continue;
            fprintf(fp, "%s%d", first ? "" : ", ", trade); first = 0;
        }
        fprintf(fp, "]\n  visited_towns_mask: %u\n  picked_up_items:\n",
                (unsigned)visited[0] | ((unsigned)visited[1] << 8));
        int maps = 0;
        for (int map_id = 0; map_id < 248; map_id++) {
            if (!wPickedUpItems[map_id]) continue;
            fprintf(fp, "    - map_id: %d\n      slots: [", map_id);
            first = 1;
            for (int slot = 0; slot < 16; slot++) {
                if (!(wPickedUpItems[map_id] & (1u << slot))) continue;
                fprintf(fp, "%s%d", first ? "" : ", ", slot); first = 0;
            }
            fputs("]\n", fp);
            maps++;
        }
        if (!maps) fputs("    []\n", fp);
    }

    fprintf(fp, "\nrng: {add: %u, sub: %u}\n", hRandomAdd, hRandomSub);
    fprintf(fp, "\noptions:\n  text_speed: %s\n  battle_animations: %s\n  battle_style: %s\n  sound: %s\n",
            (wOptions&7)==1?"fast":(wOptions&7)==5?"slow":"medium",
            (wOptions&128)?"false":"true", (wOptions&64)?"set":"shift", (wOptions&32)?"stereo":"mono");
    unsigned long total=gPlayTimeFrames;
    fprintf(fp, "\nplay_time:\n  hours: %lu\n  minutes: %lu\n  seconds: %lu\n  frames: %lu\n",
            total/216000, (total/3600)%60, (total/60)%60, total%60);
    fprintf(fp, "\nmovement: %s\n", wWalkBikeSurfState==1?"biking":wWalkBikeSurfState==2?"surfing":"walking");
    if (wRivalStarter) fprintf(fp, "\nrival_starter: %u\n", wRivalStarter);
    fclose(fp);
    printf("[scenario] captured current state to %s\n", path);
    return 0;
}
