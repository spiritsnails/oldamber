#include "field_moves.h"
#include "assetpack_bind.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "bicycle.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "pokemon.h"
#include "constants.h"
#include "amberscript_tilemod.h"
#include "escape_anim.h"
#include "rom_text.h"
#include "amberscript_core.h"
#include "../data/event_constants.h"
#include "../data/cut_anim_tiles.h"
#include "../data/move_anim_tiles.h"

#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetCuttableOverrideAt(int tx, int ty, uint8_t *cuttable) {
    (void)tx;
    (void)ty;
    (void)cuttable;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetCutReplacementAt(int tx, int ty, char *out_name, size_t out_cap) {
    (void)tx;
    (void)ty;
    (void)out_name;
    (void)out_cap;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_TilePlaceCustom(const char *name, int x, int y) {
    (void)name;
    (void)x;
    (void)y;
    return 0;
}

extern uint8_t wPartyCount;
extern uint8_t wPartyMonNicks[PARTY_LENGTH][NAME_LENGTH];
extern uint8_t wObtainedBadges;
extern uint8_t wWhichPokemon;

static char s_cut_text[64];
static char s_flash_text[64];
static char s_surf_text[64];
static char s_strength_text[96];
static int8_t s_surf_step_seq[1];
static int s_strength_active = 0;

typedef struct {
    int      active;
    int      timer;
    int      fx;
    int      fy;
    uint8_t  front_tile;
    uint8_t  front_block;
    uint8_t  replacement_block;

    int      is_amberscript;

    int      is_grass;

    int      swapped;
    char     amberscript_replacement_name[32];
} cut_state_t;

static cut_state_t s_cut = {0};

#define CUT_ANIM_OAM_BASE   72
#define CUT_ANIM_OAM_END    75
#define CUT_ANIM_TILE_BASE  0x7C
#define CUT_ANIM_FRAMES_TREE  8
#define CUT_ANIM_FRAMES_GRASS 32

#define MAP_ROUTE17             0x1c
#define MAP_SEAFOAM_ISLANDS_B4F 0xa2

static void decode_poke_name(const uint8_t *src, char *dst, int dst_size) {
    int out = 0;
    if (dst_size <= 0) return;
    for (int i = 0; out < dst_size - 1 && src[i] != 0x50; i++) {
        uint8_t c = src[i];
        if      (c >= 0x80 && c <= 0x99) dst[out++] = (char)('A' + (c - 0x80));
        else if (c >= 0xA0 && c <= 0xB9) dst[out++] = (char)('a' + (c - 0xA0));
        else if (c == 0x7F)              dst[out++] = ' ';
        else if (c >= 0xF6)             dst[out++] = (char)('0' + (c - 0xF6));
        else if (c == 0xE8)              dst[out++] = '.';
        else if (c == 0xE3)              dst[out++] = '-';
        else if (c == 0xF3)              dst[out++] = '/';
        else                             dst[out++] = '?';
    }
    dst[out] = '\0';
}

static int party_mon_knows_move(int slot, uint8_t move_id) {
    if (slot < 0 || slot >= (int)wPartyCount) return 0;
    for (int i = 0; i < 4; i++) {
        if (wPartyMons[slot].base.moves[i] == move_id) return 1;
    }
    return 0;
}

static int surfing_disallowed_here(char *out_text, int out_size) {

    int real_id = Map_CurrentRealId();
    if (wWalkBikeSurfState == 1 || real_id == MAP_ROUTE17) {
        snprintf(out_text, out_size, "%s", RomText("_CyclingIsFunText"));
        return 1;
    }

    if (real_id == MAP_SEAFOAM_ISLANDS_B4F) {
        if (!(CheckEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE) &&
              CheckEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE))) {
            if ((int)wYCoord == 11 && (int)wXCoord == 7) {
                snprintf(out_text, out_size, "%s", RomText("_CurrentTooFastText"));
                return 1;
            }
        }
    }

    return 0;
}

static int surf_step_allowed(int nx, int ny, int require_water, int *out_continue_surfing) {
    player_surf_step_t step = Player_ClassifySurfStep(nx, ny);
    if (step == PLAYER_SURF_STEP_INVALID) return 0;
    if (require_water && step != PLAYER_SURF_STEP_WATER) return 0;
    if (out_continue_surfing) *out_continue_surfing = (step == PLAYER_SURF_STEP_WATER);
    return 1;
}

static int cuttable_block_replacement(uint8_t block_id, uint8_t *out_block) {

    for (int i = 0; i < (int)kCutTreeBlockSwaps_count; i++) {
        if (kCutTreeBlockSwaps[i][0] != block_id) continue;
        if (out_block) *out_block = kCutTreeBlockSwaps[i][1];
        return 1;
    }
    return 0;
}

static void format_cut_text(int slot) {
    char mon_name[16];

    if (slot >= 0 && slot < (int)wPartyCount &&
        wPartyMonNicks[slot][0] != 0x00 && wPartyMonNicks[slot][0] != 0x50) {
        decode_poke_name(wPartyMonNicks[slot], mon_name, sizeof(mon_name));
    } else {
        snprintf(mon_name, sizeof(mon_name), "%s", PortText("#MON"));
    }
    snprintf(s_cut_text, sizeof(s_cut_text), "%s hacked\naway with CUT!", mon_name);
}

static void clear_cut_oam(void) {
    for (int i = CUT_ANIM_OAM_BASE; i <= CUT_ANIM_OAM_END; i++) {
        wShadowOAM[i].y = 0;
        wShadowOAM[i].x = 0;
        wShadowOAM[i].tile = 0;
        wShadowOAM[i].flags = 0;
    }
}

static void load_cut_tiles(int is_grass) {
    if (is_grass) {
        for (int i = 0; i < 4; i++)
            Display_LoadSpriteTile(CUT_ANIM_TILE_BASE + i, gMoveAnimTileset1[6]);
    } else {
        for (int i = 0; i < 4; i++)
            Display_LoadSpriteTile(CUT_ANIM_TILE_BASE + i, gCutTreeAnimTiles[i]);
    }
}

static void place_cut_oam(void) {
    int sx = (s_cut.fx * 2 - gCamX) * TILE_PX;
    int sy = (s_cut.fy * 2 - gCamY) * TILE_PX;

    int shift = s_cut.timer;

    wShadowOAM[CUT_ANIM_OAM_BASE + 0] = (oam_entry_t){
        (uint8_t)(sy + OAM_Y_OFS),
        (uint8_t)(sx + shift + OAM_X_OFS),
        CUT_ANIM_TILE_BASE + 0,
        0
    };
    wShadowOAM[CUT_ANIM_OAM_BASE + 1] = (oam_entry_t){
        (uint8_t)(sy + OAM_Y_OFS),
        (uint8_t)(sx + 8 + shift + OAM_X_OFS),
        CUT_ANIM_TILE_BASE + 1,
        0
    };
    wShadowOAM[CUT_ANIM_OAM_BASE + 2] = (oam_entry_t){
        (uint8_t)(sy + 8 + OAM_Y_OFS),
        (uint8_t)(sx - shift + OAM_X_OFS),
        CUT_ANIM_TILE_BASE + 2,
        0
    };
    wShadowOAM[CUT_ANIM_OAM_BASE + 3] = (oam_entry_t){
        (uint8_t)(sy + 8 + OAM_Y_OFS),
        (uint8_t)(sx + 8 - shift + OAM_X_OFS),
        CUT_ANIM_TILE_BASE + 3,
        0
    };
}

int FieldMove_HasFlash(int slot) {
    return party_mon_knows_move(slot, MOVE_FLASH);
}

int FieldMove_HasDig(int slot) {
    return party_mon_knows_move(slot, MOVE_DIG);
}

int FieldMove_HasTeleport(int slot) {
    return party_mon_knows_move(slot, MOVE_TELEPORT);
}

int FieldMove_TryDig(int slot) {
    wActionResultOrTookBattleTurn = 0;
    if (!FieldMove_HasDig(slot)) {

    Text_ShowASCII(PortText("That #MON\ndoesn't know DIG!"));
        return 0;
    }
    if (!EscapeAnim_CanEscapeHere()) {

        Text_ShowASCII("OAK: {PLAYER}!\nThis isn't the\ntime to use that!");
        return 0;
    }
    wActionResultOrTookBattleTurn = 1;
    EscapeAnim_StartToLastHealTown();
    return 1;
}

int FieldMove_TryTeleport(int slot) {
    wActionResultOrTookBattleTurn = 0;
    if (!FieldMove_HasTeleport(slot)) {
        Text_ShowASCII(PortText("That #MON\ndoesn't know\nTELEPORT!"));
        return 0;
    }
    if (!EscapeAnim_IsOutsideMap()) {
        char mon_name[NAME_LENGTH];
        decode_poke_name(wPartyMonNicks[slot], mon_name, sizeof(mon_name));
        snprintf(s_flash_text, sizeof(s_flash_text),
                 "%s can't\nuse TELEPORT now.", mon_name);
        Text_ShowASCII(s_flash_text);
        return 0;
    }
    wActionResultOrTookBattleTurn = 1;
    Text_ShowASCII(RomText("StartMenu_Pokemon.warpToLastPokemonCenterText"));
    return 2;
}

int FieldMove_TryFly(int slot) {
    wActionResultOrTookBattleTurn = 0;
    if (!(wObtainedBadges & (1u << BIT_THUNDERBADGE))) {
        Text_ShowASCII(RomText("_NewBadgeRequiredText"));
        return 0;
    }
    if (!EscapeAnim_IsOutsideMap()) {
        char mon_name[NAME_LENGTH];
        decode_poke_name(wPartyMonNicks[slot], mon_name, sizeof(mon_name));

        RomTextSplice(s_flash_text, sizeof(s_flash_text),
                      "_CannotFlyHereText", "{badge}", mon_name);
        Text_ShowASCII(s_flash_text);
        return 0;
    }
    wActionResultOrTookBattleTurn = 1;
    return 1;
}

int FieldMove_TryFlash(int slot) {
    wActionResultOrTookBattleTurn = 0;
    if (!(wObtainedBadges & (1u << BIT_BOULDERBADGE))) {
        Text_ShowASCII(RomText("_NewBadgeRequiredText"));
        return 0;
    }
    if (!FieldMove_HasFlash(slot)) {

        Text_ShowASCII(PortText("That #MON\ndoesn't know\vFLASH!"));
        return 0;
    }

    wActionResultOrTookBattleTurn = 1;
    gMapPalOffset = 0;
    Display_LoadMapPalette();
    snprintf(s_flash_text, sizeof(s_flash_text), "%s", RomText("_FlashLightsAreaText"));
    Text_ShowASCII(s_flash_text);
    return 2;
}

int FieldMove_UseSurfFromMenu(int slot) {
    int fx, fy;
    char mon_name[NAME_LENGTH + 1];

    wActionResultOrTookBattleTurn = 0;

    if (!party_mon_knows_move(slot, MOVE_SURF)) {
        Text_ShowASCII(PortText("That #MON\ndoesn't know SURF!"));
        return 0;
    }

    Player_GetFacingTile(&fx, &fy);

    if (wWalkBikeSurfState != 2 && surfing_disallowed_here(s_surf_text, sizeof(s_surf_text))) {
        Text_ShowASCII(s_surf_text);
        return 0;
    }

    if (wWalkBikeSurfState == 2) {
        if (NPC_FindAtTile(fx, fy) >= 0) {
            Text_ShowASCII(RomText("_SurfingNoPlaceToGetOffText"));
            return 0;
        }

        if (Player_ClassifySurfStep(fx, fy) != PLAYER_SURF_STEP_LAND) {
            Text_ShowASCII(RomText("_SurfingNoPlaceToGetOffText"));
            return 0;
        }

        wWalkBikeSurfState = 0;
        Bicycle_PlayDefaultMusic();
        wJoyIgnore = 0xFF;
        s_surf_step_seq[0] = (int8_t)(gPlayerFacing & 3);
        Player_StartSimulatedMovement(s_surf_step_seq, 0);
        Player_SyncOAM();
        wActionResultOrTookBattleTurn = 1;
        return 1;
    }

    if (!surf_step_allowed(fx, fy, 1, NULL)) {

        char surf_who[16];
        if (slot >= 0 && slot < (int)wPartyCount &&
            wPartyMonNicks[slot][0] != 0x00 && wPartyMonNicks[slot][0] != 0x50) {
            decode_poke_name(wPartyMonNicks[slot], surf_who, sizeof(surf_who));
        } else {
            snprintf(surf_who, sizeof(surf_who), "%s", PortText("#MON"));
        }
        Text_SetNameBufferString(surf_who);
        Text_ShowASCII(RomText("_NoSurfingHereText"));
        return 0;
    }

    if (slot >= 0 && slot < (int)wPartyCount &&
        wPartyMonNicks[slot][0] != 0x00 && wPartyMonNicks[slot][0] != 0x50) {
        decode_poke_name(wPartyMonNicks[slot], mon_name, sizeof(mon_name));
    } else {
        snprintf(mon_name, sizeof(mon_name), "%s", PortText("#MON"));
    }
    snprintf(s_surf_text, sizeof(s_surf_text), "{PLAYER} got on\n%s!", mon_name);

    wWalkBikeSurfState = 2;
    Bicycle_PlayDefaultMusic();
    s_surf_step_seq[0] = (int8_t)(gPlayerFacing & 3);
    Player_StartSimulatedMovement(s_surf_step_seq, 0);
    Text_ShowASCII(s_surf_text);

    wActionResultOrTookBattleTurn = 1;
    return 2;
}

int FieldMove_UseCutFromMenu(void) {
    int fx, fy;
    uint8_t tile;
    uint8_t block;
    uint8_t replacement;

    wActionResultOrTookBattleTurn = 0;

    if (!(wObtainedBadges & (1u << BIT_CASCADEBADGE))) {
        Text_ShowASCII(RomText("_NewBadgeRequiredText"));
        return 0;
    }

    Player_GetFacingTile(&fx, &fy);
    tile = Map_GetGameTile(fx, fy);
    s_cut.front_tile = tile;

    if (AmberScript_IsEnabled()) {
        uint8_t cuttable = 0;
        if (AmberScript_GetCuttableOverrideAt(fx * 2, fy * 2 + 1, &cuttable) && cuttable) {
            char replacement[32] = {0};
            if (!AmberScript_GetCutReplacementAt(fx * 2, fy * 2 + 1, replacement, sizeof(replacement))) {

                printf("[amberscript] cut refused at (%d,%d): 'cuttable yes' has no "
                       "'cut_replacement' -- add one, a cut with no visible result "
                       "is a content bug, not a valid outcome\n", fx, fy);
                Text_ShowASCII(RomText("UsedCut.NothingToCutText"));
                return 0;
            }
            {

                uint8_t grass = 0;
                AmberScript_GetGrassOverrideAt(fx * 2, fy * 2 + 1, &grass);
                s_cut.is_grass = grass ? 1 : 0;
            }
            s_cut.active = 1;
            s_cut.timer = 0;
            s_cut.swapped = 0;
            s_cut.fx = fx;
            s_cut.fy = fy;
            s_cut.is_amberscript = 1;
            snprintf(s_cut.amberscript_replacement_name, sizeof(s_cut.amberscript_replacement_name), "%s", replacement);
            load_cut_tiles(s_cut.is_grass);
            clear_cut_oam();

            wActionResultOrTookBattleTurn = 1;
            format_cut_text((int)wWhichPokemon);
            Text_ShowASCII(s_cut_text);
            return 1;
        }
    }

    if (wCurMapTileset == TILESET_OVERWORLD) {
        if (tile != 0x3D && tile != 0x52) {
            Text_ShowASCII(RomText("_NothingToCutText"));
            return 0;
        }
    } else if (wCurMapTileset == TILESET_GYM) {
        if (tile != 0x50) {
            Text_ShowASCII(RomText("_NothingToCutText"));
            return 0;
        }
    } else {
        Text_ShowASCII(RomText("_NothingToCutText"));
        return 0;
    }

    block = Map_GetBlockAt(fx, fy);
    if (!cuttable_block_replacement(block, &replacement)) {
        Text_ShowASCII(RomText("_NothingToCutText"));
        return 0;
    }

    s_cut.active = 1;
    s_cut.timer = 0;
    s_cut.swapped = 0;
    s_cut.fx = fx;
    s_cut.fy = fy;
    s_cut.front_block = block;
    s_cut.replacement_block = replacement;
    s_cut.is_amberscript = 0;
    s_cut.is_grass = (tile == 0x52) ? 1 : 0;
    load_cut_tiles(s_cut.is_grass);
    clear_cut_oam();

    wActionResultOrTookBattleTurn = 1;
    format_cut_text((int)wWhichPokemon);
    Text_ShowASCII(s_cut_text);
    return 1;
}

int FieldMove_TryStrength(int slot) {
    wActionResultOrTookBattleTurn = 0;

    if (!(wObtainedBadges & (1u << BIT_RAINBOWBADGE))) {
        Text_ShowASCII(RomText("_NewBadgeRequiredText"));
        return 0;
    }
    if (!party_mon_knows_move(slot, MOVE_STRENGTH)) {

        Text_ShowASCII(PortText("That #MON\ndoesn't know\vSTRENGTH!"));
        return 0;
    }

    s_strength_active = 1;
    wActionResultOrTookBattleTurn = 1;

    {
        char mon_name[16] = "MONSTER";
        if (slot >= 0 && slot < (int)wPartyCount &&
            wPartyMonNicks[slot][0] != 0x00 && wPartyMonNicks[slot][0] != 0x50) {
            decode_poke_name(wPartyMonNicks[slot], mon_name, sizeof(mon_name));
        }
        snprintf(s_strength_text, sizeof(s_strength_text),
                 "%s used\nSTRENGTH!\f%s can\nmove boulders.", mon_name, mon_name);
    }
    if (slot >= 0 && slot < (int)wPartyCount) {
        Audio_PlayCry(wPartyMons[slot].base.species);
    }
    Text_ShowASCII(s_strength_text);
    return 2;
}

int FieldMove_IsStrengthActive(void) {
    return s_strength_active;
}

void FieldMove_ClearStrength(void) {
    s_strength_active = 0;
}

void FieldMove_OnMapLoad(void) {
    FieldMove_ClearStrength();
}

int FieldMove_IsActive(void) {
    return s_cut.active;
}

void FieldMove_Tick(void) {
    if (!s_cut.active) return;

    if (Text_IsOpen()) return;

    if (!s_cut.swapped) {

        if (s_cut.is_amberscript) {

            if (AmberScript_GetCutSpanBlockAt(s_cut.fx * 2, s_cut.fy * 2 + 1)) {

                int bx = s_cut.fx & ~1, by = s_cut.fy & ~1;
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        char mate[32] = {0};
                        if (AmberScript_GetCutReplacementAt((bx + dx) * 2,
                                                           (by + dy) * 2 + 1,
                                                           mate, sizeof(mate)))
                            AmberScript_TilePlaceCustom(mate, bx + dx, by + dy);
                    }
                }
            } else {
                AmberScript_TilePlaceCustom(s_cut.amberscript_replacement_name, s_cut.fx, s_cut.fy);
            }
        } else {
            Map_SetBlockAt(s_cut.fx, s_cut.fy, s_cut.replacement_block);
        }
        Map_BuildScrollView();
        s_cut.swapped = 1;
        s_cut.timer = 0;
    }

    Player_SyncOAM();
    place_cut_oam();

    {
        int total_frames = s_cut.is_grass ? CUT_ANIM_FRAMES_GRASS : CUT_ANIM_FRAMES_TREE;
        if (s_cut.timer < total_frames) {
            s_cut.timer++;
            return;
        }
    }

    Audio_PlaySFX_Cut();
    clear_cut_oam();
    s_cut.active = 0;
}

void FieldMove_PostRender(void) {
}
