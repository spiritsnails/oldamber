
#include "debug_overlay.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "warp.h"
#include "pokemon.h"
#include "battle/battle.h"
#include "../data/event_data.h"
#include "../data/map_data.h"
#include "../data/moves_data.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../game/constants.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "gen2_species.h"

#define GRID_W  10
#define GRID_H   9
#define STEP     2

static int s_overlay_on = 0;

#define OV_SOLID  0xFF000068u
#define OV_GRASS  0x00AA0068u
#define OV_LEDGE  0xEEBB0068u
#define OV_WARP   0x00BBFF68u
#define OV_NPC    0xFF00FF80u
#define OV_SIGN   0xFF880068u
#define OV_ITEM   0xFFDD0068u
#define OV_HIDDEN 0x8800FF68u

void Debug_SetCollisionOverlay(int on) {
    s_overlay_on = on;
    Display_SetOverlayEnabled(on);
}

int Debug_CollisionOverlayOn(void) { return s_overlay_on; }

void Debug_UpdateOverlay(void) {
    if (!s_overlay_on) return;
    Display_ClearOverlay();

    const map_events_t *ev = (wCurMap < PKS_VIRTUAL_MAP_FIRST) ? &gMapEvents[wCurMap] : NULL;
    int nc = NPC_GetCount();

    for (int ty = 0; ty < SCREEN_HEIGHT; ty++) {
        for (int tx = 0; tx < SCREEN_WIDTH; tx++) {
            int mx = gCamX + tx;
            int my = gCamY + ty;
            uint8_t tid = Map_GetTile(mx, my);

            int is_npc = 0;
            for (int i = 0; i < nc; i++) {
                int ntx, nty;
                NPC_GetTilePos(i, &ntx, &nty);
                if (ntx * 2 == mx && nty * 2 + 1 == my) { is_npc = 1; break; }
            }
            if (is_npc) { Display_SetOverlayTile(tx, ty, OV_NPC); continue; }

            if (ev) {
                int is_warp = 0;
                for (int i = 0; i < ev->num_warps; i++) {
                    if ((int)ev->warps[i].x * 2 == mx && (int)ev->warps[i].y * 2 + 1 == my) {
                        is_warp = 1; break;
                    }
                }
                if (is_warp) { Display_SetOverlayTile(tx, ty, OV_WARP); continue; }

                int is_sign = 0;
                for (int i = 0; i < ev->num_signs; i++) {
                    if ((int)ev->signs[i].x * 2 == mx && (int)ev->signs[i].y * 2 + 1 == my)
                        { is_sign = 1; break; }
                }
                if (is_sign) { Display_SetOverlayTile(tx, ty, OV_SIGN); continue; }

                int is_item = 0;
                for (int i = 0; i < ev->num_items; i++) {
                    if ((int)ev->items[i].x * 2 == mx && (int)ev->items[i].y * 2 + 1 == my)
                        { is_item = 1; break; }
                }
                if (is_item) { Display_SetOverlayTile(tx, ty, OV_ITEM); continue; }

                int is_hidden = 0;
                for (int i = 0; i < ev->num_hidden_events; i++) {
                    if ((int)ev->hidden_events[i].x == mx && (int)ev->hidden_events[i].y == my)
                        { is_hidden = 1; break; }
                }
                if (is_hidden) { Display_SetOverlayTile(tx, ty, OV_HIDDEN); continue; }
            }

            if (Warp_IsDoorTile(tid))                      { Display_SetOverlayTile(tx, ty, OV_WARP);  continue; }
            if (tid == wGrassTile && wGrassTile != 0xFF)   { Display_SetOverlayTile(tx, ty, OV_GRASS); continue; }
            if (Player_GetLedgeDir(tid) >= 0)              { Display_SetOverlayTile(tx, ty, OV_LEDGE); continue; }
            if (!Tile_IsPassable(tid))                     { Display_SetOverlayTile(tx, ty, OV_SOLID); continue; }

        }
    }
}

static const char *status_str(uint8_t s) {
    if (IS_ASLEEP(s))    return "SLP";
    if (IS_POISONED(s))  return "PSN";
    if (IS_BURNED(s))    return "BRN";
    if (IS_FROZEN(s))    return "FRZ";
    if (IS_PARALYZED(s)) return "PAR";
    return "OK";
}
static int stage_delta(uint8_t s) { return (int)s - 7; }

static void bprint(FILE *fp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    if (fp) { va_start(ap, fmt); vfprintf(fp, fmt, ap); va_end(ap); }
}

static void dump_bmon(FILE *fp, const char *side, const battle_mon_t *m,
                      const uint8_t mods[NUM_STAT_MODS],
                      uint8_t s1, uint8_t s2, uint8_t s3,
                      uint8_t conf, uint8_t tox, uint8_t dis) {
    static const char *snames[] = {"Atk","Def","Spd","Spc","Acc","Eva"};
    bprint(fp, "%s: %s Lv%d  HP %d/%d  [%s]\n",
           side,
           Pokemon_GetName(Species_Dex(m->species)),
           m->level, m->hp, m->max_hp, status_str(m->status));
    bprint(fp, "  Moves: ");
    for (int i = 0; i < 4; i++)
        bprint(fp, "%s/%dpp  ", BMOVE(m->moves[i]), m->pp[i] & 0x3F);
    bprint(fp, "\n");
    bprint(fp, "  Stats:  ATK=%d DEF=%d SPD=%d SPC=%d\n",
           m->atk, m->def, m->spd, m->spc);
    bprint(fp, "  Stages:");
    for (int i = 0; i < NUM_STAT_MODS; i++)
        bprint(fp, " %s%+d", snames[i], stage_delta(mods[i]));
    bprint(fp, "\n");
    bprint(fp, "  Flags: 0x%02X / 0x%02X / 0x%02X  Confused=%d Toxic=%d Disabled=0x%02X\n",
           s1, s2, s3, conf, tox, dis);
}

void Debug_PrintBattleState(void) {
    if (!wIsInBattle) { printf("[battle] Not in battle.\n"); return; }

    char path[256];
    time_t now = time(NULL);
    snprintf(path, sizeof(path), "bugs/battle_%lu.txt", (unsigned long)now);
    FILE *fp = fopen(path, "w");

    bprint(fp, "\n=== BATTLE STATE  frame=0x%02X  RNG add=0x%02X sub=0x%02X ===\n",
           hFrameCounter, hRandomAdd, hRandomSub);
    bprint(fp, "Mode: %s\n", wIsInBattle == 1 ? "wild" : "trainer");

    dump_bmon(fp, "PLAYER", &wBattleMon,
              wPlayerMonStatMods,
              wPlayerBattleStatus1, wPlayerBattleStatus2, wPlayerBattleStatus3,
              wPlayerConfusedCounter, wPlayerToxicCounter, wPlayerDisabledMove);

    dump_bmon(fp, "ENEMY", &wEnemyMon,
              wEnemyMonStatMods,
              wEnemyBattleStatus1, wEnemyBattleStatus2, wEnemyBattleStatus3,
              wEnemyConfusedCounter, wEnemyToxicCounter, wEnemyDisabledMove);

    bprint(fp, "wDamage=%d  crit=0x%02X  miss=%d  whose_turn=%d\n",
           wDamage, wCriticalHitOrOHKO, wMoveMissed, hWhoseTurn);
    bprint(fp, "Selected: player=%s(%d) enemy=%s(%d)\n",
           BMOVE(wPlayerSelectedMove), wPlayerSelectedMove,
           BMOVE(wEnemySelectedMove),  wEnemySelectedMove);

    if (fp) { fclose(fp); printf("[debug] Battle state -> %s\n", path); }
}

static FILE *s_clog_fp  = NULL;
static int   s_clog_on  = 0;

static void combat_log_sink(const char *line) {
    if (s_clog_fp) { fprintf(s_clog_fp, "%s\n", line); fflush(s_clog_fp); }
}

void Debug_SetCombatLog(int on) {
    if (on && !s_clog_on) {
        char path[256];
        time_t now = time(NULL);
        snprintf(path, sizeof(path), "bugs/combat_log_%lu.txt", (unsigned long)now);
        s_clog_fp = fopen(path, "w");
        if (s_clog_fp) {
            s_clog_on = 1;
            gCombatLogSink = combat_log_sink;
            printf("[debug] Combat log: %s\n", path);
        }
    } else if (!on && s_clog_on) {
        gCombatLogSink = NULL;
        if (s_clog_fp) { fclose(s_clog_fp); s_clog_fp = NULL; }
        s_clog_on = 0;
        printf("[debug] Combat log OFF.\n");
    }
}

int Debug_CombatLogOn(void) { return s_clog_on; }

void Debug_DumpWRAM(void) {
    char path[256];
    time_t now = time(NULL);
    snprintf(path, sizeof(path), "bugs/wram_%lu.txt", (unsigned long)now);
    FILE *fp = fopen(path, "w");
    if (!fp) { printf("[debug] WRAM dump open failed.\n"); return; }

    const map_info_t *mi = (wCurMap < PKS_VIRTUAL_MAP_FIRST) ? &gMapTable[wCurMap] : NULL;
    fprintf(fp, "=== WRAM DUMP  frame=0x%02X ===\n", hFrameCounter);
    fprintf(fp, "Position: map=%d (%s)  pos=(%d,%d)  facing=%d  battle=%d\n",
            wCurMap, mi ? mi->name : "???", wXCoord, wYCoord,
            gPlayerFacing, wIsInBattle);

    fprintf(fp, "\nParty (%d):\n", wPartyCount);
    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++) {
        const party_mon_t *p = &wPartyMons[i];
        fprintf(fp, "  [%d] %s Lv%d HP %d/%d [%s]  moves: %s %s %s %s\n",
                i, Pokemon_GetName(Species_Dex(p->base.species)),
                p->level, p->base.hp, p->max_hp,
                status_str(p->base.status),
                BMOVE(p->base.moves[0]), BMOVE(p->base.moves[1]),
                BMOVE(p->base.moves[2]), BMOVE(p->base.moves[3]));
    }

    fprintf(fp, "\nBag (%d items):\n", wNumBagItems);
    for (int i = 0; i < wNumBagItems; i++)
        fprintf(fp, "  item=0x%02X qty=%d\n",
                wBagItems[i * 2], wBagItems[i * 2 + 1]);

    fprintf(fp, "\nMoney (BCD): %02X%02X%02X\n",
            wPlayerMoney[0], wPlayerMoney[1], wPlayerMoney[2]);
    fprintf(fp, "Badges: 0x%02X\n", wObtainedBadges);
    fprintf(fp, "RNG: add=0x%02X sub=0x%02X frame=0x%02X\n",
            hRandomAdd, hRandomSub, hFrameCounter);

    fclose(fp);
    printf("[debug] WRAM dump -> %s\n", path);
}

static char classify(int cx, int cy, int pcx, int pcy,
                     const map_events_t *ev) {
    int tx = gCamX + cx * STEP;
    int ty = gCamY + cy * STEP;

    if (cx == pcx && cy == pcy) return '@';

    int nc = NPC_GetCount();
    for (int i = 0; i < nc; i++) {
        int ntx, nty;
        NPC_GetTilePos(i, &ntx, &nty);
        if ((ntx * 2 - gCamX) / STEP == cx && (nty * 2 + 1 - gCamY) / STEP == cy)
            return 'o';
    }

    if (ev) {
        for (int i = 0; i < ev->num_warps; i++) {
            int wcx = ((int)ev->warps[i].x * 2 - gCamX) / STEP;
            int wcy = ((int)ev->warps[i].y * 2 + 1 - gCamY) / STEP;
            if (wcx == cx && wcy == cy) return 'D';
        }
    }

    uint8_t tid = Map_GetTile(tx, ty);

    if (Warp_IsDoorTile(tid))             return 'D';
    if (tid == wGrassTile && wGrassTile != 0xFF) return 'g';

    int ld = Player_GetLedgeDir(tid);
    if (ld == 0) return 'v';
    if (ld == 2) return '<';
    if (ld == 3) return '>';

    return Tile_IsPassable(tid) ? '.' : '#';
}

void Debug_PrintGrid(void) {

    int pcx = ((int)wXCoord * 2 - gCamX) / STEP;
    int pcy = ((int)wYCoord * 2 + 1 - gCamY) / STEP;

    const map_events_t *ev = (wCurMap < PKS_VIRTUAL_MAP_FIRST) ? &gMapEvents[wCurMap] : NULL;
    const map_info_t   *mi = (wCurMap < PKS_VIRTUAL_MAP_FIRST) ? &gMapTable[wCurMap]  : NULL;

    char grid[GRID_H][GRID_W + 1];
    for (int cy = 0; cy < GRID_H; cy++) {
        for (int cx = 0; cx < GRID_W; cx++)
            grid[cy][cx] = classify(cx, cy, pcx, pcy, ev);
        grid[cy][GRID_W] = '\0';
    }

    printf("\n+----------+ MAP: %s  pos:(%d,%d)\n",
           mi ? mi->name : "???", (int)wXCoord, (int)wYCoord);
    for (int cy = 0; cy < GRID_H; cy++) {
        static const char *notes[] = {
            "@=player  o=NPC  D=warp/door",
            "g=grass   v=ledgeS  <=ledgeW  >=ledgeE",
            "#=solid   .=walkable",
            "", "", "", "", "", ""
        };
        printf("|%s| %s\n", grid[cy], notes[cy]);
    }
    printf("+----------+\n\n");
}
