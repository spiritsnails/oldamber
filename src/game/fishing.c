
#include "fishing.h"
#include "player.h"
#include "overworld.h"
#include "text.h"
#include "rom_text.h"
#include "inventory.h"
#include "trainer_sight.h"
#include "amberscript_mapbank.h"
#include "amberscript_core.h"
#include "constants.h"
#include "../data/fishing_types.h"
#include "assetpack_bind.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include <string.h>
#include <stdio.h>

extern void Game_StartWildBattleScripted(uint8_t species, uint8_t level);

extern uint8_t BattleRandom(void);

#define ROD_OAM        39
#define ROD_TILE_BASE  0x78

#define TILE_WATER          0x14
#define TILE_SHORE_EAST     0x32
#define TILE_SHORE_SAFARI   0x48

#define USE_DELAY_FRAMES   80
#define PRE_ROD_FRAMES     10
#define ROD_HOLD_FRAMES   100
#define SHAKE_STEPS        10
#define SHAKE_DELAY         3

#define BUBBLE_FRAMES      60

typedef enum {
    FS_IDLE = 0,
    FS_USE_DELAY,
    FS_PRE_ROD,
    FS_ROD_HOLD,
    FS_SHAKE,
    FS_BUBBLE,
    FS_RESULT_TEXT,
    FS_START_BATTLE,
} fs_t;

static fs_t     s_state    = FS_IDLE;
static int      s_timer    = 0;
static int      s_shake    = 0;
static uint8_t  s_response = 0;
static uint8_t  s_species  = 0;
static uint8_t  s_level    = 0;
static int      s_rod_on   = 0;
static int      s_rod_hidden = 0;
static int      s_shake_phase = 0;

static int facing_shore_or_water(void) {
    int fx = 0, fy = 0;
    Player_GetFacingTile(&fx, &fy);

    if (AmberScript_IsEnabled()) {
        uint8_t surfable = 0;
        if (Map_GetSurfableOverrideAt(fx, fy, &surfable) && surfable)
            return 1;
    }

    uint8_t t = Map_GetGameTile(fx, fy);
    if (t == TILE_WATER) return 1;

    {

        const char *nm = AmberScript_MapBank_NameForRealId(wCurMap);
        if (nm && strcasecmp(nm, "VermilionDock") == 0)
            return 0;
    }
    return t == TILE_SHORE_EAST || t == TILE_SHORE_SAFARI;
}

int Fishing_CanUse(void) {
    if (wIsInBattle)                return 0;
    if (!facing_shore_or_water())   return 0;
    if (wWalkBikeSurfState == 2)    return 0;
    return 1;
}

static int fishing_init(uint8_t item_id) {
    if (!Fishing_CanUse()) return 0;

    Text_SetItemName(item_id);
    Text_ShowASCII(RomText("ItemUseText00"));
    Audio_PlaySFX_HealAilment();
    return 1;
}

static void roll_old_rod(void) {

    s_response = 1;
    s_level    = 5;
    s_species  = SPECIES_MAGIKARP;
}

static void roll_good_rod(void) {

    for (;;) {
        uint8_t r = BattleRandom();
        int idx;
        if (r & 1) { s_response = 0; return; }
        idx = (r >> 1) & 3;
        if (idx >= 2) continue;
        s_level    = gGoodRodMons[idx][0];
        s_species  = gGoodRodMons[idx][1];
        s_response = 1;
        return;
    }
}

static void roll_super_rod(void) {

    int real_id = Map_CurrentRealId();
    const super_rod_group_t *g;
    if (real_id < 0 || real_id >= (int)gSuperRodData_count) {
        s_response = 2;
        return;
    }
    g = &gSuperRodData[real_id];
    if (g->count == 0) {
        s_response = 2;
        return;
    }

    for (;;) {
        uint8_t r = BattleRandom();
        int idx;
        if (r & 1) { s_response = 0; return; }
        idx = (r >> 1) & 3;
        if (idx >= g->count) continue;
        s_level    = g->slots[idx].level;
        s_species  = g->slots[idx].species;
        s_response = 1;
        return;
    }
}

void Fishing_Use(uint8_t item_id) {
    if (!fishing_init(item_id)) { s_state = FS_IDLE; return; }

    if      (item_id == ITEM_OLD_ROD)  roll_old_rod();
    else if (item_id == ITEM_GOOD_ROD) roll_good_rod();
    else                               roll_super_rod();

    s_rod_on = 0;
    s_timer  = USE_DELAY_FRAMES;
    s_state  = FS_USE_DELAY;
}

int Fishing_IsActive(void) { return s_state != FS_IDLE; }

void Fishing_PostRender(void) {
    if (!s_rod_on) return;

    if (s_shake_phase) {
        int i;
        for (i = 0; i < 4; i++) wShadowOAM[i].y ^= 1;
    }
    if (s_rod_hidden) return;

    static const struct { int8_t dx, dy, tile, xflip; } kRod[4] = {
        {   4,  15, 0, 0 },
        {   4,  -8, 0, 0 },
        {  -8,   4, 1, 0 },
        {  16,   4, 1, 1 },
    };
    int f = gPlayerFacing & 3;

    wShadowOAM[ROD_OAM].y     = (uint8_t)(wShadowOAM[0].y + kRod[f].dy);
    wShadowOAM[ROD_OAM].x     = (uint8_t)(wShadowOAM[0].x + kRod[f].dx);
    wShadowOAM[ROD_OAM].tile  = (uint8_t)(ROD_TILE_BASE + kRod[f].tile);
    wShadowOAM[ROD_OAM].flags = kRod[f].xflip ? OAM_FLAG_FLIP_X : 0;
    if (s_shake_phase) wShadowOAM[ROD_OAM].y ^= 1;
}

static void show_result_text(void) {
    const char *sym = (s_response == 0) ? "_NoNibbleText"
                    : (s_response == 2) ? "_NothingHereText"
                                        : "_ItsABiteText";
    Text_ShowASCII(RomText(sym));
    s_state = FS_RESULT_TEXT;
}

static void rod_down(void) {
    s_rod_on = 0;
    s_rod_hidden = 0;
    s_shake_phase = 0;
    wShadowOAM[ROD_OAM].y = 0;
    wShadowOAM[ROD_OAM].x = 0;
    wShadowOAM[ROD_OAM].tile = 0;
    wShadowOAM[ROD_OAM].flags = 0;
}

void Fishing_Tick(void) {
    switch (s_state) {
    case FS_IDLE:
        break;

    case FS_USE_DELAY:

        if (--s_timer > 0) return;
        Text_Close();
        s_timer = PRE_ROD_FRAMES;
        s_state = FS_PRE_ROD;
        break;

    case FS_PRE_ROD:
        if (--s_timer > 0) return;

        for (int i = 0; i < 3; i++)
            Display_LoadSpriteTile((uint8_t)(ROD_TILE_BASE + i), gFishingRodTiles[i]);
        Player_SetFishingPose(1);
        s_rod_on = 1;
        s_timer  = ROD_HOLD_FRAMES;
        s_state  = FS_ROD_HOLD;
        break;

    case FS_ROD_HOLD:
        if (--s_timer > 0) return;

        if (s_response != 1) { show_result_text(); break; }
        s_shake = SHAKE_STEPS;
        s_timer = SHAKE_DELAY;
        s_state = FS_SHAKE;
        break;

    case FS_SHAKE:

        if (--s_timer > 0) return;
        s_shake_phase ^= 1;
        s_timer = SHAKE_DELAY;
        if (--s_shake > 0) return;

        s_rod_hidden = ((gPlayerFacing & 3) == 1);
        Emote_ShowOnPlayer();
        s_timer = BUBBLE_FRAMES;
        s_state = FS_BUBBLE;
        break;

    case FS_BUBBLE:
        if (--s_timer > 0) return;
        Emote_Hide();
        s_rod_hidden = 0;
        show_result_text();
        break;

    case FS_RESULT_TEXT:

        if (Text_IsOpen()) return;

        rod_down();
        Player_SetFishingPose(0);
        if (s_response == 1) { s_state = FS_START_BATTLE; break; }
        s_state = FS_IDLE;
        break;

    case FS_START_BATTLE:
        s_state = FS_IDLE;
        Game_StartWildBattleScripted(s_species, s_level);
        break;
    }
}
