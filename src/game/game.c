
#include "../platform/hardware.h"
#include "rom_text.h"
#include "../platform/display.h"
#include "../platform/save.h"
#include "overworld.h"
#include "anim.h"
#include "gbc_color.h"
#include "player.h"
#include "warp.h"
#include "npc.h"
#include "text.h"
#include "menu.h"
#include "trainer_card.h"
#include "escape_anim.h"
#include "bag_menu.h"
#include "crystal_fade.h"
#include "inventory.h"
#include "pokemon.h"
#include "constants.h"
#include "../data/font_data.h"
#include "../data/event_data.h"
#include "../data/event_constants.h"
#include "../data/wild_data.h"
#include "battle/battle_init.h"
#include "battle/battle_trainer.h"
#include "battle/battle_driver.h"
#include "battle/battle_ui.h"
#include "battle/battle_loop.h"
#include "battle_transition.h"
#include "trainer_sight.h"
#include "johto_battle.h"
#include "party_menu.h"
#include "pokecenter.h"

static int g_pcnpcdbg_dumped = 0;
#include "pc_menu.h"
#include "players_pc.h"
#include "fly_anim.h"
#include "pokemart.h"
#include "music.h"
#include "../platform/audio.h"
#include "debug_cli.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "amberscript_scene.h"
#include "johto_music.h"
#include "map_music.h"
#include "../data/map_data.h"
#include "session_log.h"
#include "gate_scripts.h"
#include "cycling_road_gate_scripts.h"
#include "vermilion_gym_scripts.h"
#include "cinnabar_gym_scripts.h"
#include "pokedex.h"
#include "trade.h"
#include "town_map.h"
#include "blackboard.h"
#include "bag_list_choice.h"
#include "prize_list_choice.h"
#include "slot_machine.h"
#include "money_box.h"
#include "coin_box.h"
#include "fossil_popup.h"
#include "title_screen.h"
#include "main_menu.h"
#include "intro.h"
#include "pallet_scripts.h"
#include "oakslab_scripts.h"
#include "gym_scripts.h"
#include "viridian_gym_spinners.h"
#include "fishing.h"
#include "static_encounter.h"
#include "viridian_mart_scripts.h"
#include "route24_scripts.h"
#include "bills_house_scripts.h"
#include "bills_pokemon_list.h"
#include "badge_house_menu.h"
#include "diploma.h"
#include "route25_scripts.h"
#include "blues_house_scripts.h"
#include "reds_house1f_scripts.h"
#include "route2gate_scripts.h"
#include "ss_anne_depart.h"
#include "tmhm.h"
#include "field_moves.h"
#include "pokeflute.h"
#include "bicycle.h"
#include "naming_screen.h"
#include "name_rater_scripts.h"
#include "daycare.h"
#include "poison.h"
#include "rockethideout_scripts.h"
#include "game_corner_scripts.h"
#include "pokemontower6f_scripts.h"
#include "pokemontower7f_scripts.h"
#include "mrfujis_house_scripts.h"
#include "saffron_city_scripts.h"
#include "celadon_city_scripts.h"
#include "route1_scripts.h"
#include "seafoam_scripts.h"
#include "victory_road_scripts.h"
#include "elite_four_scripts.h"
#include "champions_room_scripts.h"
#include "hall_of_fame_scripts.h"
#include "credits_scripts.h"
#include "safari_zone_scripts.h"
#include "safari_zone_secret_house_scripts.h"
#include "wardens_house_scripts.h"
#include "mansion_scripts.h"
#include "cinnabar_island_scripts.h"
#include "itemfinder.h"
#include "yesno.h"
#include "bikeshop_menu.h"
#include "elevator_menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platform/debug_log.h"

typedef enum {
    SCENE_OVERWORLD = 0,
    SCENE_BTRANS,
    SCENE_BATTLE,
    SCENE_MENU,
    SCENE_TITLE,
    SCENE_MAIN_MENU,
    SCENE_INTRO,
    SCENE_EVOLUTION,
    SCENE_TRADE,
} GameScene;

static GameScene gScene = SCENE_OVERWORLD;
static uint8_t sBattleResultLatched = BATTLE_OUTCOME_NONE;

int  Game_GetScene(void)   { return (int)gScene; }

int Game_SceneHasMap(void) {
    switch (gScene) {
    case SCENE_OVERWORLD:
    case SCENE_BTRANS:
    case SCENE_BATTLE:
    case SCENE_MENU:
    case SCENE_EVOLUTION:
    case SCENE_TRADE:
        return 1;
    case SCENE_TITLE:
    case SCENE_MAIN_MENU:
    case SCENE_INTRO:
    default:
        return 0;
    }
}

void Game_SetScene(int s)  { gScene = (GameScene)s; }

void Game_SyncGbcColor(void) {

    int want = (gScene == SCENE_OVERWORLD || gScene == SCENE_BTRANS) &&
               !SlotMachine_IsOpen();

    {
        static int last_want = -1;
        if (gScene == SCENE_OVERWORLD && want != last_want) {
            last_want = want;
            if (!want)
                DBG_PRINTF("[COLORDBG] colour OFF in overworld: slots=%d hof=%d "
                       "credits=%d naming=%d\n",
                       SlotMachine_IsOpen(), HallOfFameScripts_IsActive(),
                       CreditsScripts_IsActive(), NamingScreen_IsOpen());
            else
                DBG_PRINTF("[COLORDBG] colour ON in overworld\n");
            fflush(stdout);
        }
    }

    if (gScene == SCENE_TITLE ||

        gScene == SCENE_MAIN_MENU ||

        gScene == SCENE_INTRO ||
        HallOfFameScripts_IsActive() ||

        BattleUI_IsEvolutionScreen() ||

        NamingScreen_IsOpen() ||

        Pokedex_IsOpen() ||
        TrainerCard_IsOpen() ||

        (PartyMenu_IsOpen() && wIsInBattle) ||
        CreditsScripts_IsActive()) {
        GbcColor_MarkDirty();
        return;
    }

    GbcColor_Sync(want, wCurMap);
}

int Game_BeginFieldEvolution(void) {
    if (gScene != SCENE_OVERWORLD) return 0;
    if (!BattleUI_BeginPendingEvolution()) return 0;
    gScene = SCENE_EVOLUTION;
    return 1;
}

int Game_BeginTradeAnim(void) {
    if (gScene != SCENE_OVERWORLD) return 0;
    TradeAnim_Begin();
    gScene = SCENE_TRADE;
    return 1;
}

static int s_overworld_tick_active = 1;
int Game_IsOverworldTickActive(void) { return s_overworld_tick_active; }

int gSkipMenu = 0;
int gSkipHallOfFameToCredits = 0;
static int gStartupHasSave = 0;

static int gMainMenuOptionsOpen = 0;

#define TITLE_TO_MENU_WHITEOUT_FRAMES 40
static int gTitleToMenuFade = 0;

static void fire_map_loaded2_callbacks(void) {
    VermilionGymScripts_OnMapLoad();
    CinnabarGymScripts_OnMapLoad();
    VictoryRoadScripts_OnMapLoad();
    GameCornerScripts_OnMapLoad();
    GymScripts_OnMapLoad();
}

static void fire_map_onload_callbacks(void) {
    AmberScript_Scene_NotifyMapLoaded();
    CreditsScripts_OnMapLoad();
    Bicycle_OnMapLoad();
    PalletScripts_OnMapLoad();
    OaksLabScripts_OnMapLoad();
    ViridianMartScripts_OnMapLoad();
    Route24Scripts_OnMapLoad();
    BluesHouseScripts_OnMapLoad();
    RedsHouse1FScripts_OnMapLoad();
    BillsHouseScripts_OnMapLoad();
    Route2GateScripts_OnMapLoad();
    VermilionGymScripts_OnMapLoad();
    GymScripts_OnMapLoad();
    ViridianGymSpinners_OnMapLoad();
    RocketHideoutScripts_OnMapLoad();
    GameCornerScripts_OnMapLoad();
    PokemonTower6FScripts_OnMapLoad();
    PokemonTower7FScripts_OnMapLoad();
    MrFujisHouseScripts_OnMapLoad();
    SaffronCityScripts_OnMapLoad();
    CeladonGiftScripts_OnMapLoad();
    SeafoamScripts_OnMapLoad();
    VictoryRoadScripts_OnMapLoad();
    EliteFourScripts_OnMapLoad();
    ChampionsRoomScripts_OnMapLoad();
    HallOfFameScripts_OnMapLoad();
    SafariZoneScripts_OnMapLoad();
    SafariZoneSecretHouseScripts_OnMapLoad();
    WardensHouseScripts_OnMapLoad();
    CinnabarGymScripts_OnMapLoad();
    MansionScripts_OnMapLoad();
    CinnabarIslandScripts_OnMapLoad();
    Trainer_LoadMap();
    Gate_LoadMap();
    PokeFlute_LoadMap();
}

static void enter_overworld(void) {

    Map_HoldForBootScreen(0);
    if (AmberScript_IsEnabled())
        AmberScript_TileMod_InvalidateSubtileCache();

    MapMusic_Adopt();

    Map_Load(wCurMap);

    Display_LoadMapPalette();
    Font_Load();
    Player_Init((uint8_t)wXCoord, (uint8_t)wYCoord);
    NPC_Load();
    fire_map_onload_callbacks();
    Map_BuildScrollView();
    NPC_BuildView(0, 0);
    gScene = SCENE_OVERWORLD;
}

int Game_WarpToRealMap(uint8_t real_id, int x, int y) {
    extern int AmberScript_MapWarp(const char *name, int x, int y);
    if (real_id >= NUM_MAPS) {
        printf("[game] Game_WarpToRealMap: real_id %u out of range (NUM_MAPS=%d)\n", real_id, NUM_MAPS);
        return 0;
    }
    if (!AmberScript_MapWarp(gMapTable[real_id].name, x, y))
        return 0;
    fire_map_onload_callbacks();
    Map_BuildScrollView();
    NPC_BuildView(0, 0);
    return 1;
}

void GameInit(void) {
    extern void WRAMClear(void);
    extern void AmberScript_MapBank_ResetAll(void);

    Map_HoldForBootScreen(1);
    WRAMClear();

    AmberScript_MapBank_ResetAll();
    wLastBlackoutMap = 0xFF;

    gStartupHasSave = (Save_Load() == 0);
    if (gStartupHasSave)
        printf("[save] Save loaded OK — map %d @ (%d,%d)\n", wCurMap, wXCoord, wYCoord);
    else
        printf("[save] No save found — showing title screen\n");

    Font_Load();

    AmberScript_TileMod_PreloadIndoorSubtiles();
    if (gSkipHallOfFameToCredits) {
        if (!gStartupHasSave) {
            printf("[skiphof] No save found; cannot jump to credits from Hall of Fame context.\n");
        } else {
            enter_overworld();
            CreditsScripts_StartImmediate();
            return;
        }
    }

    if (gSkipMenu && gStartupHasSave) {
        enter_overworld();
        return;
    }

    TitleScreen_Open();
    gScene = SCENE_TITLE;
}

static const uint8_t kWildText[] = {

    0x96,0x88,0x8B,0x8B,0x7F,0x8F,0x8E,0x8A,0x84,0x8C,0x8E,0x8D,0x9B,0x50
};

#define FADE_TICKS_PER_STEP  4
#define FADE_OUT_STEPS       4
#define FADE_IN_STEPS        4
#define MAP_ROUTE17          0x1c

static const uint8_t kFadeOut[FADE_OUT_STEPS][3] = {
    {0xE4, 0xD0, 0xE0},
    {0xF9, 0xE4, 0xE4},
    {0xFE, 0xFE, 0xF8},
    {0xFF, 0xFF, 0xFF},
};

static const uint8_t kFadeInFromWhite[FADE_IN_STEPS][3] = {
    {0x00, 0x00, 0x00},
    {0x0A, 0x01, 0x08},
    {0x28, 0x04, 0x20},
    {0xE4, 0xD0, 0xE0},
};

typedef enum { WARP_NONE = 0, WARP_FADE_OUT, WARP_FADE_IN, WARP_HOLE, WARP_TELEPORT } WarpPhase;
static WarpPhase gWarpPhase    = WARP_NONE;
static int       gWarpStep     = 0;
static int       gWarpStepTimer = 0;
int Game_IsWarpFadeActive(void) { return gWarpPhase != WARP_NONE; }

typedef enum {
    HOLE_PHASE_INIT = 0,
    HOLE_PHASE_FADE_OUT,
    HOLE_PHASE_FADE_IN,
    HOLE_PHASE_POST_DELAY,
    HOLE_PHASE_DROP_IN,
} hole_phase_t;
typedef enum {
    TELE_PHASE_RISE = 0,
    TELE_PHASE_FALL,
} tele_phase_t;

static hole_phase_t gHolePhase = HOLE_PHASE_INIT;
static const uint8_t kFadeOutToWhite[FADE_IN_STEPS][3] = {
    {0xE4, 0xD0, 0xE0},
    {0x28, 0x04, 0x20},
    {0x0A, 0x01, 0x08},
    {0x00, 0x00, 0x00},
};

#define HOLE_POST_FADE_DELAY_TICKS 25

static const int8_t kHoleDropY[] = { -20, -4, 12, 28, 44, 60 };
static int gHoleDropStep = 0;
static int gHoleSavedFacing = 0;
static tele_phase_t gTelePhase = TELE_PHASE_RISE;
static int gTeleStep = 0;
static int gTeleStepTimer = 0;
static int gTeleSavedFacing = 0;
static const int8_t kTeleRiseY[] = { 60, 44, 28, 12, -4, -20 };
static const int8_t kTeleFallY[] = { -20, -4, 12, 28, 44, 60 };

#define TELE_STEP_DELAY_TICKS 3

static void start_hole_leave_anim(void) {
    wShadowOAM[2].tile = wShadowOAM[0].tile;
    wShadowOAM[3].tile = wShadowOAM[1].tile;
    wShadowOAM[0].y = 0;
    wShadowOAM[1].y = 0;
}

static void set_player_oam_top_y(int top_y) {
    int oam_y = top_y + OAM_Y_OFS;
    wShadowOAM[0].y = (uint8_t)oam_y;
    wShadowOAM[1].y = (uint8_t)oam_y;
    wShadowOAM[2].y = (uint8_t)(oam_y + 8);
    wShadowOAM[3].y = (uint8_t)(oam_y + 8);
}

static void begin_pending_warp_transition(void) {
    if (Warp_IsPendingDungeonHole()) {
        gWarpPhase = WARP_HOLE;
        gHolePhase = HOLE_PHASE_INIT;
        gWarpStep = 0;
        gHoleDropStep = 0;
        gHoleSavedFacing = gPlayerFacing & 3;

        gWarpStepTimer = 1;
        start_hole_leave_anim();
        return;
    }
    if (Warp_IsPendingTeleportPad()) {
        gWarpPhase = WARP_TELEPORT;
        gTelePhase = TELE_PHASE_RISE;
        gTeleStep = 0;
        gTeleStepTimer = TELE_STEP_DELAY_TICKS;
        gTeleSavedFacing = gPlayerFacing & 3;
        Audio_PlaySFX_TeleportExit1();
        return;
    }

    gWarpPhase = WARP_FADE_OUT;
    gWarpStep = 0;
    gWarpStepTimer = FADE_TICKS_PER_STEP;
}

static int gPendingTrainerBattle = 0;
static int gPendingCustomTrainerBattle = 0;
static uint8_t gPendingCustomSpecies[6];
static uint8_t gPendingCustomLevel[6];
static uint8_t gPendingCustomMoves[6][4];
static uint8_t gPendingCustomCount = 0;
static uint8_t gBattleLogTrainerClass = 0;
static uint8_t gBattleLogTrainerNo = 0;
static uint8_t gBattleLogWildSpecies = 0;
static uint8_t gBattleLogEnemyLevel = 0;

static int is_elite4_room_map(uint8_t map_id) {
    return map_id == 0xF5 || map_id == 0xF6 || map_id == 0xF7 || map_id == 0x71;
}

static int is_elite4_trainer_class(uint8_t trainer_class) {
    return trainer_class == 44 || trainer_class == 33 ||
           trainer_class == 46 || trainer_class == 47;
}

#define TRAINER_CLASS_RIVAL3 43
#define TRAINER_CLASS_LANCE  47
static uint8_t battle_music_for_trainer(uint8_t trainer_class) {
    if (wGymLeaderNo)                          return MUSIC_GYM_LEADER_BATTLE;
    if (trainer_class == TRAINER_CLASS_RIVAL3) return MUSIC_FINAL_BATTLE;
    if (trainer_class == TRAINER_CLASS_LANCE)  return MUSIC_GYM_LEADER_BATTLE;
    return MUSIC_TRAINER_BATTLE;
}

void Game_StartTrainerBattleScripted(uint8_t trainer_class, uint8_t trainer_no) {
    int player_level = 5;
    if (wPartyCount > 0) player_level = wPartyMons[0].level;
    gEngagedTrainerClass = trainer_class;
    gEngagedTrainerNo = trainer_no;
    Music_Play(battle_music_for_trainer(trainer_class));
    gPendingTrainerBattle = 1;
    BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
    gScene = SCENE_BTRANS;
}

void Game_StartWildBattleScripted(uint8_t species, uint8_t level) {
    int player_level = 5;
    for (int i = 0; i < wPartyCount; i++) {
        if (wPartyMons[i].base.hp > 0) { player_level = wPartyMons[i].level; break; }
    }
    wCurPartySpecies = species;
    wCurEnemyLevel   = level;
    Music_Play(MUSIC_WILD_BATTLE);

    BattleTransition_Start(0, level, player_level);
    gScene = SCENE_BTRANS;
}

uint8_t Game_ArmCustomTrainerParty(const uint8_t species[6],
                                   const uint8_t level[6],
                                   const uint8_t moves[6][4],
                                   uint8_t count) {
    memset(gPendingCustomSpecies, 0, sizeof(gPendingCustomSpecies));
    memset(gPendingCustomLevel, 0, sizeof(gPendingCustomLevel));
    memset(gPendingCustomMoves, 0, sizeof(gPendingCustomMoves));
    if (count > 6) count = 6;
    for (int i = 0; i < 6; i++) {
        gPendingCustomSpecies[i] = species ? species[i] : 0;
        gPendingCustomLevel[i] = level ? level[i] : 0;
        for (int m = 0; m < 4; m++)
            gPendingCustomMoves[i][m] = moves ? moves[i][m] : 0;
    }
    gPendingCustomCount = count;
    gPendingCustomTrainerBattle = 1;
    return count > 0 ? gPendingCustomLevel[count - 1] : 0;
}

void Game_StartCustomTrainerBattleScripted(uint8_t trainer_class,
                                           uint8_t music_id,
                                           const uint8_t species[6],
                                           const uint8_t level[6],
                                           const uint8_t moves[6][4],
                                           uint8_t count) {
    int player_level = 5;
    uint8_t trans_level;
    if (wPartyCount > 0) player_level = wPartyMons[0].level;
    trans_level = Game_ArmCustomTrainerParty(species, level, moves, count);
    gEngagedTrainerClass = trainer_class;
    gEngagedTrainerNo = 1;
    if (music_id == 0) music_id = MUSIC_TRAINER_BATTLE;
    Music_Play(music_id);
    gPendingTrainerBattle = 1;

    BattleTransition_Start(1, trans_level, player_level);
    gScene = SCENE_BTRANS;
}

int gNoWilds = 0;

static const uint8_t kWildSlotCumChance[10] = {
    50, 101, 140, 165, 190, 215, 228, 241, 252, 255
};

static void check_wild_encounter(void) {
    if (gNoWilds) return;

    if (Player_IsLedgeJumping()) return;

    if (CheckEvent(EVENT_IN_PURIFIED_ZONE)) {
        const char *mn = AmberScript_MapBank_NameForRealId(wCurMap);
        if (mn && strcasecmp(mn, "PokemonTower5F") == 0) return;
        ClearEvent(EVENT_IN_PURIFIED_ZONE);
    }
    if (wCurMap >= NUM_MAPS) return;

    if (wRepelRemainingSteps > 0) {
        if (--wRepelRemainingSteps == 0) {
            Text_ShowASCII(RomText("_RepelWoreOffText"));
            return;
        }
    }

    uint8_t cur_tile = Map_GetGameTile((int)wXCoord, (int)wYCoord);
    int is_water = (cur_tile == 0x14);
    int is_vmap = AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST;

    if (!is_water) {

        if (is_vmap) {

            uint8_t is_grass = 0;
            int has_grass_override = AmberScript_GetGrassOverrideAt(
                (int)wXCoord * 2, (int)wYCoord * 2 + 1, &is_grass);
            if (has_grass_override) {
                if (!is_grass) return;
            } else if (!AmberScript_MapBank_IsIndoorForRealId(wCurMap)) {

                return;
            }

        } else if (wGrassTile != 0xFF && cur_tile != wGrassTile) {
            return;
        }
    }

    if (is_vmap && AmberScript_HasJohtoWildTable(wCurMap, is_water)) {
        uint8_t species = 0, level = 0;
        if (!AmberScript_TryJohtoWildEncounter(wCurMap, is_water, &species, &level)) return;
        wCurEnemyLevel   = level;
        wCurPartySpecies = species;
    } else {
        const wild_mons_t *w = AmberScript_GetWildMonsFor(wCurMap, is_water);
        if (!w->rate) return;

        uint8_t rate_roll = hRandomAdd;
        if (rate_roll >= w->rate) return;

        uint8_t slot_roll = hRandomSub;
        uint8_t slot_idx = 9;
        for (uint8_t i = 0; i < 10; i++) {
            if (slot_roll <= kWildSlotCumChance[i]) {
                slot_idx = i;
                break;
            }
        }
        wCurEnemyLevel   = w->slots[slot_idx].level;
        wCurPartySpecies = w->slots[slot_idx].species;
    }

    if (wRepelRemainingSteps > 0 && wCurEnemyLevel < wPartyMons[0].level)
        return;

    if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {

        char track[32];
        int is_kanto = AmberScript_MapBank_GetMusicForRealId(wCurMap, track, sizeof(track))
                       && KantoMusic_ForTrackName(track) != MUSIC_NONE;
        if (is_kanto) {
            Music_Play(MUSIC_WILD_BATTLE);
        } else {
            Music_Stop();
            JohtoMusic_Play(CRYSTAL_MUSIC_JOHTO_WILD_BATTLE);
        }
    } else {
        Music_Play(MUSIC_WILD_BATTLE);
    }

    int player_level = 5;
    for (int i = 0; i < wPartyCount; i++) {
        if (wPartyMons[i].base.hp > 0) {
            player_level = wPartyMons[i].level;
            break;
        }
    }

    BattleTransition_Start(0, wCurEnemyLevel, player_level);
    gScene = SCENE_BTRANS;
}

static void check_item_pickup(void) {
    if (!(hJoyPressed & PAD_A)) return;

    int fx, fy;
    Player_GetFacingTile(&fx, &fy);

    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->items) return;

    for (int i = 0; i < ev->num_items; i++) {
        const item_event_t *it = &ev->items[i];
        if ((int)it->x != fx || (int)it->y != fy) continue;

        int has_pickup_tracking = wCurMap < PKS_VIRTUAL_MAP_FIRST;
        uint16_t pks_flag_bit = has_pickup_tracking ? 0 : AmberScript_GetItemFlagBitAt(wCurMap, it->src_idx);
        int pks_has_tracking = !has_pickup_tracking && pks_flag_bit != 0;

        if (has_pickup_tracking && (wPickedUpItems[wCurMap] & (1u << i))) return;
        if (pks_has_tracking && CheckEvent(pks_flag_bit)) return;

        static char pickup_msg[48];
        if (Inventory_Add(it->item_id, 1) == 0) {

            if (has_pickup_tracking) wPickedUpItems[wCurMap] |= (uint16_t)(1u << i);
            if (pks_has_tracking) SetEvent(pks_flag_bit);
            NPC_HideSprite(ev->num_npcs + i);

            char name[16];
            Inventory_DecodeASCII(it->item_id, name, sizeof(name));

            if (name[0] == '\0') {
                snprintf(pickup_msg, sizeof(pickup_msg), "{PLAYER} found\nan item!");
            } else {
                snprintf(pickup_msg, sizeof(pickup_msg), "{PLAYER} found\n%s!", name);
            }

            Audio_PlaySFX_GetItem1();
            SessionLog_ItemPickup(it->item_id, 1);
        } else {
            snprintf(pickup_msg, sizeof(pickup_msg), "No room for\nmore items!");
            SessionLog_ItemPickup(it->item_id, 0);
        }
        Text_ShowASCII(pickup_msg);
        return;
    }
}

static int facing_counter_tile(int fx, int fy) {
    if (AmberScript_IsEnabled()) {
        uint8_t is_counter = 0;

        if (AmberScript_GetCounterOverrideAt(fx * 2, fy * 2 + 1, &is_counter))
            return is_counter ? 1 : 0;
    }
    {
        uint8_t t = Map_GetGameTile(fx, fy);
        int i;
        for (i = 0; i < 3; i++)
            if (t == wTilesetTalkingOverTiles[i]) return 1;
    }
    return 0;
}

static void check_npc_interact(void) {
    if (!(hJoyPressed & PAD_A)) return;

    int fx, fy;
    Player_GetFacingTile(&fx, &fy);

    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->npcs) return;

    int facing = gPlayerFacing & 3;
    int dist   = facing_counter_tile(fx, fy) ? 1 : 0;
    int best_i;
    {
        int tx = fx, ty = fy;
        switch (facing) {
            case 0: ty = fy + dist; break;
            case 1: ty = fy - dist; break;
            case 2: tx = fx - dist; break;
            case 3: tx = fx + dist; break;
        }
        best_i = NPC_FindAtTile(tx, ty);
    }

    if (best_i < 0) return;

    if (NPC_GetSpriteId(best_i) == 0x3F) {
        Text_ShowASCII(RomText("_BoulderText"));
        return;
    }

    if (NPC_SpriteCanFacePlayer(NPC_GetSpriteId(best_i)) &&
        !(AmberScript_IsEnabled() &&
          AmberScript_NpcSuppressesFacePlayerByDecl(wCurMap, NPC_GetDeclIdx(best_i))))
        NPC_FacePlayer(best_i);

    NPC_BuildView(gScrollPxX, gScrollPxY);
    SessionLog_NpcSpoke(best_i, fx, fy);

    if (AmberScript_IsEnabled() ? AmberScript_OnNpcInteracted(best_i) : DebugCLI_OnNpcInteracted(best_i)) return;

    if (ev->trainers) {
        int is_trainer = 0;
        for (int ti = 0; ti < ev->num_trainers; ti++) {
            const map_trainer_t *tr = &ev->trainers[ti];
            if (tr->npc_idx != best_i) continue;
            is_trainer = 1;
            if (CheckEvent(tr->flag_bit)) {
                if (tr->after_text) Text_ShowASCII(tr->after_text);
            } else {
                Trainer_EngageImmediate(best_i);
            }
            break;
        }
        if (is_trainer) return;
    }

    if (best_i >= ev->num_npcs) return;

    const npc_event_t *self = &ev->npcs[best_i];
    {
        int decl = NPC_GetDeclIdx(best_i);
        if (decl >= 0 && AmberScript_IsEnabled() &&
            wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {
            for (int k = 0; k < ev->num_npcs; k++) {
                if ((int)ev->npcs[k].src_idx == decl) { self = &ev->npcs[k]; break; }
            }
        }
    }

    if (self->script) {
        self->script();
    } else if (self->text) {

        const char *text = NULL;
        if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {

            int decl = NPC_GetDeclIdx(best_i);
            if (decl >= 0) text = AmberScript_ResolveNpcTextByDecl(wCurMap, decl);
            if (!text) text = AmberScript_ResolveNpcText(wCurMap, best_i);
        }
        Text_ShowASCII(text ? text : self->text);
    }
}

static void check_hidden_event(void) {
    if (!(hJoyPressed & PAD_A)) return;

    int fx, fy;
    Player_GetFacingTile(&fx, &fy);

    if (wCurMap >= NUM_MAPS) return;

    if (AmberScript_IsEnabled() && AmberScript_OnTileInteracted(fx, fy)) return;

    {
        int sm_index, sm_kind;
        if (AmberScript_IsEnabled() &&
            AmberScript_GetSlotMachineAt(wCurMap, fx, fy, &sm_index, &sm_kind)) {
            SlotMachine_Start(sm_index, sm_kind);
            return;
        }
    }

    {
        uint8_t hi_item; uint16_t hi_flag;
        if (AmberScript_IsEnabled() &&
            AmberScript_GetHiddenItemAt(wCurMap, fx, fy, &hi_item, &hi_flag)) {
            if (hi_flag && CheckEvent(hi_flag)) return;
            static char hi_msg[128];
            char iname[16];
            Inventory_DecodeASCII(hi_item, iname, sizeof(iname));
            if (Inventory_Add(hi_item, 1) == 0) {
                if (hi_flag) SetEvent(hi_flag);
                snprintf(hi_msg, sizeof(hi_msg), "{PLAYER} found\n%s!",
                         iname[0] ? iname : "an item");

                Text_SetPendingSFXOnPrint(Audio_PlaySFX_GetItem2);
                Text_SetCloseAfterPrintSFX();
                SessionLog_ItemPickup(hi_item, 1);
            } else {

                snprintf(hi_msg, sizeof(hi_msg),
                         "{PLAYER} found\n%s!\fBut, {PLAYER} has\nno more room for\nother items!",
                         iname[0] ? iname : "an item");
                SessionLog_ItemPickup(hi_item, 0);
            }
            Text_ShowASCII(hi_msg);
            return;
        }
    }

    {
        uint16_t hc_amount, hc_flag;
        if (AmberScript_IsEnabled() &&
            AmberScript_GetHiddenCoinAt(wCurMap, fx, fy, &hc_amount, &hc_flag)) {
            if (Inventory_GetQty(0x45 ) > 0) {
                if (!(hc_flag && CheckEvent(hc_flag))) {
                    uint32_t before = ((wPlayerCoins[0] >> 4) & 0xF) * 1000u +
                                       (wPlayerCoins[0] & 0xF)       * 100u  +
                                      ((wPlayerCoins[1] >> 4) & 0xF) * 10u   +
                                       (wPlayerCoins[1] & 0xF);
                    uint32_t after = before + hc_amount;
                    if (after > 9999u) after = 9999u;
                    wPlayerCoins[0] = (uint8_t)(((after / 1000u) << 4) | ((after / 100u) % 10u));
                    wPlayerCoins[1] = (uint8_t)((((after / 10u) % 10u) << 4) | (after % 10u));
                    if (hc_flag) SetEvent(hc_flag);
                    static char hc_msg[80];
                    snprintf(hc_msg, sizeof(hc_msg),
                             (after == 9999u) ? "{PLAYER} found\n%u coins!\fOops! Dropped\nsome coins!"
                                               : "{PLAYER} found\n%u coins!",
                             hc_amount);

                    Text_SetPendingSFXOnPrint(Audio_PlaySFX_GetItem2);
                    Text_ShowASCII(hc_msg);
                }
            }
            return;
        }
    }

    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (ev->hidden_events) {
        for (int i = 0; i < ev->num_hidden_events; i++) {
            const hidden_event_t *h = &ev->hidden_events[i];
            if ((int)h->x != fx || (int)h->y != fy) continue;

            if (h->facing != 0 && (h->facing - 1) != (gPlayerFacing & 3)) continue;
            if (h->script) {
                h->script();
            } else if (h->text) {

                const char *text = (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
                    ? AmberScript_ResolveHiddenEventText(wCurMap, i) : NULL;
                Text_ShowASCII(text ? text : h->text);
            }
            return;
        }
    }

}

static void check_sign_interact(void) {
    if (!(hJoyPressed & PAD_A)) return;

    int fx, fy;
    Player_GetFacingTile(&fx, &fy);

    if (AmberScript_IsEnabled()) {
        const char *sign_text = AmberScript_GetSignTextAt(fx, fy);
        if (sign_text) {
            Text_ShowASCII(sign_text);
            return;
        }
    }

    if (wCurMap >= NUM_MAPS) return;
    const map_events_t *ev = AmberScript_GetMapEventsFor(wCurMap);
    if (!ev->signs) return;

    for (int i = 0; i < ev->num_signs; i++) {
        const sign_event_t *s = &ev->signs[i];
        if ((int)s->x == fx && (int)s->y == fy) {

            if (wCurMap == 0xCB && (int)s->x == 1 && (int)s->y == 1) {
                if (Inventory_GetQty(0x4A) > 0) {
                    wDoNotWaitForButtonPress = 1;
                    Text_ShowASCII(RomText("_WhichFloorText"));
                    ElevatorMenu_QueueOpenRocketHideout();
                } else if (s->text) {
                    Text_ShowASCII(s->text);
                }
                return;
            }

            if (wCurMap == 0xEC && (int)s->x == 3 && (int)s->y == 0) {
                wDoNotWaitForButtonPress = 1;
                Text_ShowASCII(RomText("_WhichFloorText"));
                ElevatorMenu_QueueOpenSilphCo();
                return;
            }
            if (s->text) Text_ShowASCII(s->text);
            return;
        }
    }
}

static uint32_t s_ow_accum         = 0;
static uint8_t  s_ow_pressed_latch = 0;
enum {
    OW_GATE_NUM = 597275u,
    OW_GATE_DEN = 1250000u
};

void Game_GetOwGateState(uint32_t *accum, uint8_t *latch) {
    if (accum) *accum = s_ow_accum;
    if (latch) *latch = s_ow_pressed_latch;
}
void Game_SetOwGateState(uint32_t accum, uint8_t latch) {
    s_ow_accum = accum;
    s_ow_pressed_latch = latch;
}

unsigned long gPlayTimeFrames = 0;

static void party_move_watchdog(void) {
    static uint8_t prev[PARTY_LENGTH][4];
    static int primed = 0;
    int i, j;
    if (!primed) {
        for (i = 0; i < PARTY_LENGTH; i++)
            for (j = 0; j < 4; j++) prev[i][j] = wPartyMons[i].base.moves[j];
        primed = 1;
        return;
    }
    for (i = 0; i < PARTY_LENGTH; i++) {
        for (j = 0; j < 4; j++) {
            uint8_t now = wPartyMons[i].base.moves[j];
            if (now == prev[i][j]) continue;
            printf("[movewatch] party[%d].move[%d] %u -> %u  "
                   "(map=%d scene=%d battle=%u frame=%u)\n",
                   i + 1, j + 1, (unsigned)prev[i][j], (unsigned)now,
                   (int)wCurMap, Game_GetScene(), (unsigned)wIsInBattle,
                   (unsigned)gPlayTimeFrames);
            fflush(stdout);
            prev[i][j] = now;
        }
    }
}

void GameTick(void) {
    gPlayTimeFrames++;
    party_move_watchdog();

    #define SOFT_RESET_COMBO (PAD_A | PAD_B | PAD_START | PAD_SELECT)

    #define SOFT_RESET_DELAY_FRAMES 32
    static int s_softResetArmed = 1;
    static int s_softResetDelay = 0;

    if (s_softResetDelay) {
        if (--s_softResetDelay == 0) {
            gSkipMenu = 0;
            gSkipHallOfFameToCredits = 0;
            GameInit();
        }
        return;
    }

    if ((hJoyHeld & SOFT_RESET_COMBO) == SOFT_RESET_COMBO) {
        if (s_softResetArmed && gScene != SCENE_TITLE) {
            s_softResetArmed = 0;
            Music_Stop();
            JohtoMusic_Stop();
            Display_SetPalette(0x00, 0x00, 0x00);
            s_softResetDelay = SOFT_RESET_DELAY_FRAMES;
            return;
        }
    } else {
        s_softResetArmed = 1;
    }

    CrystalFade_Tick();

    if (gScene == SCENE_OVERWORLD && !AmberScript_SceneWantsFullRate()) {
        uint32_t probe_accum = s_ow_accum + OW_GATE_NUM;
        s_overworld_tick_active = (probe_accum >= OW_GATE_DEN);
    } else {
        s_overworld_tick_active = 1;
    }

    DebugCLI_Tick();

    StaticEncounter_Tick();
    Route25Scripts_Tick();

    GameCornerScripts_Tick();

    VermilionGymScripts_Tick();

    if (gOverworldCloseWhiteout > 0) {
        if (--gOverworldCloseWhiteout == 0)
            Display_LoadMapPalette();
        return;
    }

    if (FlyAnim_IsActive()) {
        FlyAnim_Tick();
        return;
    }

    static int s_tele_timer = 0;
    if (++s_tele_timer >= 60) {
        s_tele_timer = 0;
        FILE *tf = fopen("bugs/teleport.txt", "r");
        if (tf) {
            int map_id = -1, tx = -1, ty = -1;
            fscanf(tf, "%d %d %d", &map_id, &tx, &ty);
            fclose(tf);
            remove("bugs/teleport.txt");
            if (map_id >= 0 && map_id < NUM_MAPS) {
                Warp_ForceTeleport((uint8_t)map_id, tx, ty);
                fire_map_onload_callbacks();
                Map_BuildScrollView();
                NPC_BuildView(0, 0);
            } else
                printf("[debug] teleport: bad map_id %d (max %d)\n", map_id, NUM_MAPS - 1);
        }
    }

    if (gScene == SCENE_TITLE) {
        if (TitleScreen_IsOpen()) {
            TitleScreen_Tick();
            return;
        }

        if (gTitleToMenuFade == 0) {
            int result = TitleScreen_GetResult();
            if (result == TITLE_SCREEN_CLEAR_SAVE)
                printf("[title] UP+SELECT+B detected (clear-save dialog path not yet ported)\n");
            Display_SetPalette(0x00, 0x00, 0x00);
            memset(wShadowOAM, 0, sizeof(wShadowOAM));
            gTitleToMenuFade = TITLE_TO_MENU_WHITEOUT_FRAMES;
            return;
        }
        if (--gTitleToMenuFade > 0)
            return;

        Font_Load();

        MainMenu_Open(gStartupHasSave);
        gScene = SCENE_MAIN_MENU;
        return;
    }

    if (gScene == SCENE_MAIN_MENU) {

        if (gMainMenuOptionsOpen) {
            if (Menu_TickOptionsStandalone()) {
                gMainMenuOptionsOpen = 0;
                Font_Load();
                MainMenu_Open(gStartupHasSave);
            }
            return;
        }
        if (MainMenu_IsOpen()) {
            MainMenu_Tick();
        } else {
            int result = MainMenu_GetResult();
            if (result == MAIN_MENU_CONTINUE) {
                enter_overworld();
            } else if (result == MAIN_MENU_NEW_GAME) {

                Intro_Start();
                gScene = SCENE_INTRO;
            } else if (result == MAIN_MENU_BACK) {

                TitleScreen_OpenAtTitle();
                gScene = SCENE_TITLE;
            } else if (result == MAIN_MENU_OPTION) {
                gMainMenuOptionsOpen = 1;
                Menu_OpenOptionsStandalone();
            }
        }
        return;
    }

    if (gScene == SCENE_INTRO) {
        if (Text_IsOpen()) { Text_Update(); return; }
        if (NamingScreen_IsOpen()) { NamingScreen_Tick(); return; }
        if (Intro_IsActive()) {
            Intro_Tick();
        } else {

            enter_overworld();
        }
        return;
    }

    PokeFlute_Tick();

    Itemfinder_Tick();

    Poison_Tick();
    {

        static int s_was_blacking_out = 0;
        int now_blacking_out = Poison_IsBlackingOut();
        if (now_blacking_out && !s_was_blacking_out)
            AmberScript_Scene_Abort();
        s_was_blacking_out = now_blacking_out;
    }

    Fishing_Tick();

    if (Text_IsOpen()) {
        int text_was_open = Text_IsOpen();

        if (Pokecenter_IsActive()) {
            if (!g_pcnpcdbg_dumped) {
                int n = NPC_GetCount();
                g_pcnpcdbg_dumped = 1;
                DBG_PRINTF("[PCNPCDBG] pokecenter text open: %d npcs, hWY=%d\n",
                       n, (int)hWY);
                for (int i = 0; i < n; i++) {
                    int o = 4 + i * 4;
                    DBG_PRINTF("[PCNPCDBG]   npc%-2d sprite=%-3d hidden=%d "
                           "oam.y=%3d,%3d,%3d,%3d oam.x=%3d\n",
                           i, NPC_GetSpriteId(i), NPC_IsHidden(i),
                           wShadowOAM[o + 0].y, wShadowOAM[o + 1].y,
                           wShadowOAM[o + 2].y, wShadowOAM[o + 3].y,
                           wShadowOAM[o + 0].x);
                }
                fflush(stdout);
            }
        } else {
            g_pcnpcdbg_dumped = 0;
        }
        Text_Update();

        MoneyBox_Refresh();
        CoinBox_Refresh();

        if (YesNo_IsOpen()) {
            YesNo_Tick();
            YesNo_PostRender();
        }

        MansionScripts_Tick();
        SafariZoneScripts_PostRender();
        if (OaksLabScripts_IsActive())
            OaksLabScripts_PostRender();
        if (FieldMove_IsActive()) {
            Player_SyncOAM();
            FieldMove_Tick();
            Map_BuildScrollView();
            FieldMove_PostRender();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            return;
        }

        if (Text_IsOpen() || gScene != SCENE_BATTLE || text_was_open) return;

        hJoyPressed = 0;
    }

    if (NamingScreen_IsOpen()) {
        NamingScreen_Tick();
        return;
    }

    if (AmberScript_SceneShowingDex() && Pokedex_IsShowingData()) {
        Pokedex_ShowDataTick();
        return;
    }

    if (AmberScript_SceneShowingBillsDexList() && BillsPokemonList_IsOpen()) {
        BillsPokemonList_Tick();
        return;
    }

    if (AmberScript_SceneShowingBadgeHouseMenu() && BadgeHouseMenu_IsOpen()) {
        BadgeHouseMenu_Tick();
        return;
    }

    if (AmberScript_SceneShowingDiploma() && Diploma_IsOpen()) {
        Diploma_Tick();
        return;
    }

    ElevatorMenu_TryOpenQueued();

    if (TMHM_IsActive()) {
        TMHM_Tick();

        if (TMHM_IsActive()) {
            if (!PartyMenu_IsOpen() && !TMHM_ShowingMenuBackdrop()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }

            TMHM_PostRender();
        }
        return;
    }

    if (Menu_IsOpen()) {
        Menu_Tick();
        return;
    }

    if (TrainerCard_IsOpen()) {
        TrainerCard_Tick();
        return;
    }

    if (ElevatorMenu_IsOpen() || ElevatorMenu_IsBusy()) {
        ElevatorMenu_Tick();
        return;
    }

    if (BagMenu_IsOpen()) {
        BagMenu_Tick();
        return;
    }

    if (Fishing_IsActive()) {

        if (!Text_IsOpen()) {
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            Player_SyncOAM();
            Fishing_PostRender();
        }
        return;
    }

    if (NameRater_IsActive()) {
        NameRater_Tick();

        if (!PartyMenu_IsOpen() && !NamingScreen_IsOpen() && !Text_IsOpen()) {
            Map_BuildScrollView();
            NameRater_PostRender();
            NPC_BuildView(gScrollPxX, gScrollPxY);
        }
        return;
    }

    if (FieldMove_IsActive()) {
        Player_SyncOAM();
        FieldMove_Tick();
        Map_BuildScrollView();
        FieldMove_PostRender();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        return;
    }

    if (EscapeAnim_IsActive()) {
        EscapeAnim_Tick();
        Map_BuildScrollView();
        Player_SyncOAM();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        return;
    }

    if (Pokedex_IsOpen()) {
        Pokedex_Tick();
        return;
    }

    if (TownMap_IsOpen()) {
        TownMap_Tick();
        return;
    }

    if (Blackboard_IsOpen()) {
        Blackboard_Tick();
        return;
    }

    if (LinkCableHelp_IsOpen()) {
        LinkCableHelp_Tick();
        return;
    }

    if (BagListChoice_IsOpen()) {
        BagListChoice_Tick();
        return;
    }

    if (PrizeListChoice_IsOpen()) {
        PrizeListChoice_Tick();
        return;
    }

    if (SlotMachine_IsOpen()) {
        SlotMachine_Tick();
        return;
    }

    if (Fossil_IsOpen()) {
        Fossil_Tick();
        return;
    }

    if (PCMenu_IsOpen()) {
        PCMenu_Tick();
        return;
    }

    if (PlayersPC_IsOpen()) {
        PlayersPC_Tick();
        return;
    }

    if (Daycare_IsActive()) {
        Daycare_Tick();
        if (!PartyMenu_IsOpen() && !Text_IsOpen()) {
            Map_BuildScrollView();
            Daycare_PostRender();
            NPC_BuildView(gScrollPxX, gScrollPxY);
        }
        return;
    }

    if (PartyMenu_IsOpen() && gScene != SCENE_BATTLE) {
        PartyMenu_Tick();
        if (!PartyMenu_IsOpen()) {
            if (TownMap_IsOpen())
                return;

            Display_LoadMapPalette();
            Map_ReloadGfx();
            Font_Load();
            NPC_ReloadTiles();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);

            Player_SyncOAM();
        }
        return;
    }

    if (Pokemart_IsActive()) {
        Pokemart_Tick();
        return;
    }

    if (Pokecenter_IsActive()) {
        Pokecenter_Tick();

        if (!Text_IsOpen() && Pokecenter_IsWaitingYesNo()) {
            NPC_BuildView(gScrollPxX, gScrollPxY);
            NPC_HideOverUITiles();
            Player_SyncOAM();
            Player_HideIfOverUI();
        }
        return;
    }

    if (gScene == SCENE_BTRANS) {
        if (BattleTransition_Tick()) {

            memset(wShadowOAM + 4, 0, (MAX_SPRITES - 4) * sizeof(wShadowOAM[0]));

            if (gPendingTrainerBattle) {
                gBattleLogTrainerClass = gEngagedTrainerClass;
                gBattleLogTrainerNo = gEngagedTrainerNo;
                gPendingTrainerBattle = 0;
                if (gPendingCustomTrainerBattle) {
                    extern void Battle_StartTrainerCustomDebug(uint8_t trainer_class,
                                                               const uint8_t species[6],
                                                               const uint8_t level[6],
                                                               const uint8_t moves[6][4],
                                                               uint8_t count);
                    Battle_StartTrainerCustomDebug(gEngagedTrainerClass,
                                                   gPendingCustomSpecies,
                                                   gPendingCustomLevel,
                                                   gPendingCustomMoves,
                                                   gPendingCustomCount);
                    gPendingCustomTrainerBattle = 0;
                } else {
                    Battle_StartTrainer(gEngagedTrainerClass, gEngagedTrainerNo);
                }
                gBattleLogWildSpecies = 0;
                gBattleLogEnemyLevel = wEnemyMon.level;
                SessionLog_BattleStart(1, gBattleLogTrainerClass, gBattleLogTrainerNo, 0, gBattleLogEnemyLevel);
            } else {
                gBattleLogTrainerClass = 0;
                gBattleLogTrainerNo = 0;
                gBattleLogWildSpecies = wCurPartySpecies;
                gBattleLogEnemyLevel = wCurEnemyLevel;
                Battle_Start();
                SessionLog_BattleStart(0, 0, 0, gBattleLogWildSpecies, gBattleLogEnemyLevel);
            }
            sBattleResultLatched = BATTLE_OUTCOME_NONE;
            BattleUI_Enter();
            gScene = SCENE_BATTLE;
        }
        return;
    }

    if (gScene == SCENE_EVOLUTION) {

        static int s_evo_exit = 0, s_evo_fstep = 0, s_evo_ftimer = 0;
        #define EVO_FADE_FRAMES 3

        if (s_evo_exit == 0) {
            BattleUI_Tick();
            if (!BattleUI_IsActive()) {
                s_evo_exit = 1; s_evo_fstep = 0; s_evo_ftimer = 0;
            }
            return;
        }

        if (s_evo_ftimer > 0) { s_evo_ftimer--; return; }
        s_evo_ftimer = EVO_FADE_FRAMES;

        if (s_evo_exit == 1) {
            Display_SetPalette(kFadeOutToWhite[s_evo_fstep][0],
                               kFadeOutToWhite[s_evo_fstep][1],
                               kFadeOutToWhite[s_evo_fstep][2]);
            if (++s_evo_fstep >= FADE_IN_STEPS) {

                Map_ReloadGfx();
                Font_Load();
                NPC_ReloadTiles();
                Map_ResetScrollState();
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
                Display_SetPalette(0xFF, 0xFF, 0xFF);
                s_evo_exit = 2; s_evo_fstep = 0;
            }
            return;
        }

        Display_SetPalette(kFadeInFromWhite[s_evo_fstep][0],
                           kFadeInFromWhite[s_evo_fstep][1],
                           kFadeInFromWhite[s_evo_fstep][2]);
        if (++s_evo_fstep >= FADE_IN_STEPS) {
            Display_LoadMapPalette();

            MapMusic_Restart();
            s_evo_exit = 0;
            gScene = SCENE_OVERWORLD;
        }
        return;
        #undef EVO_FADE_FRAMES
    }

    if (gScene == SCENE_TRADE) {

        static int s_tr_exit = 0, s_tr_timer = 0;

        if (s_tr_exit == 0) {
            if (Text_IsOpen()) Text_Update();
            TradeAnim_Tick();
            if (!TradeAnim_IsActive()) { s_tr_exit = 1; s_tr_timer = 0; }
            return;
        }

        if (s_tr_exit == 1) {
            Display_SetPalette(0x00, 0x00, 0x00);
            s_tr_timer = 3;
            s_tr_exit  = 2;
            return;
        }

        if (s_tr_exit == 2) {
            if (--s_tr_timer > 0) return;

            Map_ReloadGfx();
            Font_Load();
            NPC_ReloadTiles();
            Map_ResetScrollState();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);

            Player_SyncOAM();
            s_tr_timer = 3;
            s_tr_exit  = 3;
            return;
        }

        if (s_tr_exit == 3) {
            if (--s_tr_timer > 0) return;
            Display_LoadMapPalette();
            s_tr_timer = 10;
            s_tr_exit  = 4;
            return;
        }

        if (--s_tr_timer > 0) return;
        s_tr_exit = 0;
        gScene = SCENE_OVERWORLD;
        return;
    }

    if (gScene == SCENE_BATTLE) {

        uint8_t saved_battle_result = wBattleResult;
        if (saved_battle_result != BATTLE_OUTCOME_NONE)
            sBattleResultLatched = saved_battle_result;
        BattleUI_Tick();
        if (!BattleUI_IsActive()) {
            int keep_post_battle_music = 0;
            uint8_t resolved_battle_result =
                (saved_battle_result != BATTLE_OUTCOME_NONE)
                ? saved_battle_result
                : sBattleResultLatched;
            SessionLog_BattleEnd(resolved_battle_result,
                                 gBattleLogTrainerClass != 0,
                                 gBattleLogTrainerClass,
                                 gBattleLogTrainerNo,
                                 gBattleLogWildSpecies,
                                 gBattleLogEnemyLevel);

            {

                AmberScript_Scene_NotifyBattleEnded();

                fire_map_loaded2_callbacks();
                int was_gym_trainer    = GymScripts_ConsumeGymTrainer();

                int was_rocket_r24      = Route24Scripts_ConsumeRocketBattle();
                int was_pt7_rocket = PokemonTower7FScripts_ConsumeBattle();
                int was_elite4 = EliteFourScripts_ConsumeBattle();
                int was_champion_room = ChampionsRoomScripts_ConsumeBattle();

                int e4_real = Map_CurrentRealId();
                int elite4_fallback =
                    e4_real >= 0 &&
                    is_elite4_room_map((uint8_t)e4_real) &&
                    is_elite4_trainer_class((uint8_t)gEngagedTrainerClass);
                printf("[battle end] result=%u engaged=(class=%u no=%u) gymleader=%u flags: gym=%d route24_rocket=%d\n",
                       (unsigned)resolved_battle_result,
                       (unsigned)gEngagedTrainerClass,
                       (unsigned)gEngagedTrainerNo,
                       (unsigned)wGymLeaderNo,
                       was_gym_trainer,
                       was_rocket_r24);
                if (resolved_battle_result == BATTLE_OUTCOME_TRAINER_VICTORY) {

                    {
                        const char *m = AmberScript_MapBank_NameForRealId(Map_CurrentRealId());
                        if (m && strcmp(m, "SilphCo11F") == 0)
                            SetEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
                    }
                    if (wGymLeaderNo && !AmberScript_Scene_IsActive())
                        GymScripts_OnVictory();
                    else if (was_gym_trainer)
                        GymScripts_OnGymTrainerVictory();
                    else if (was_rocket_r24)
                        Route24Scripts_OnVictory();
                    else if (was_pt7_rocket)
                        PokemonTower7FScripts_OnVictory();
                    else if (was_elite4)
                        EliteFourScripts_OnVictory();
                    else if (elite4_fallback) {

                        Trainer_MarkCurrentDefeated();
                        EliteFourScripts_OnRoomTrainerVictory();
                    }
                    else if (was_champion_room)
                        ChampionsRoomScripts_OnVictory();
                    else
                        Trainer_MarkCurrentDefeated();
                } else if (was_rocket_r24) {
                    Route24Scripts_OnDefeat();
                } else if (was_pt7_rocket) {
                    PokemonTower7FScripts_OnDefeat();
                } else if (was_elite4 || elite4_fallback) {
                    EliteFourScripts_OnDefeat();
                } else if (was_champion_room) {
                    ChampionsRoomScripts_OnDefeat();
                }

                if (was_champion_room &&
                    resolved_battle_result == BATTLE_OUTCOME_TRAINER_VICTORY) {
                    keep_post_battle_music = 1;
                }

                if (resolved_battle_result != BATTLE_OUTCOME_TRAINER_VICTORY &&
                    wGymLeaderNo) {
                    wGymLeaderNo = 0;
                }
            }

            AmberScript_Scene_OnBattleOutcome(resolved_battle_result);

            gBattleNoBlackoutOnLoss = 0;

            {
                int was_pt6_marowak = PokemonTower6FScripts_ConsumeBattle();
                int was_snorlax = PokeFlute_ConsumeSnorlaxPostBattle();
                int was_articuno = SeafoamScripts_ConsumeArticunoPostBattle();
                int was_moltres = VictoryRoadScripts_ConsumeMoltresPostBattle();
                if (was_snorlax) {
                    if (resolved_battle_result == BATTLE_OUTCOME_WILD_VICTORY)
                        PokeFlute_OnSnorlaxVictory();
                    else if (resolved_battle_result == BATTLE_OUTCOME_CAUGHT)
                        PokeFlute_OnSnorlaxCaught();

                }
                if (was_articuno) {
                    SeafoamScripts_OnArticunoBattleOutcome(resolved_battle_result);
                }
                if (was_moltres) {
                    VictoryRoadScripts_OnMoltresBattleOutcome(resolved_battle_result);
                }
                if (was_pt6_marowak) {
                    PokemonTower6FScripts_OnBattleOutcome(resolved_battle_result);
                }
            }

            gScene = SCENE_OVERWORLD;
            gStepJustCompleted = 0;
            memset(wShadowOAM, 0, sizeof(wShadowOAM));
            if (resolved_battle_result == BATTLE_OUTCOME_BLACKOUT) {

                AmberScript_Scene_Abort();
                Map_ReloadGfx();
                Font_Load();
                Poison_StartBattleBlackout();
                sBattleResultLatched = BATTLE_OUTCOME_NONE;
                return;
            }
            {

                if (gMapPalOffset != 0) {
                    Display_LoadMapPalette();
                } else {

                    Display_SetPalette(0x00, 0x00, 0x00);
                    gWarpPhase     = WARP_FADE_IN;
                    gWarpStep      = 1;
                    gWarpStepTimer = FADE_TICKS_PER_STEP;
                }
            }
            sBattleResultLatched = BATTLE_OUTCOME_NONE;
            Map_ReloadGfx();
            Map_ResetScrollState();
            Font_Load();
            if (!keep_post_battle_music) {

                MapMusic_TryRestart();
            }

            NPC_ReloadTiles();

            Map_UpdateCamera();
            Player_SyncOAM();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
        }
        return;
    }

    if (!AmberScript_IsEnabled() || wCurMap < PKS_VIRTUAL_MAP_FIRST || wCurMap > PKS_VIRTUAL_MAP_LAST)
        Anim_UpdateTiles();

    if (!HallOfFameScripts_IsActive() && !PCMenu_IsOpen())
        AmberScript_TickTileAnimations();

    if (gScene == SCENE_OVERWORLD) {

        int hof_fullrate =
            (Map_CurrentRealId() == 0x76) &&
            HallOfFameScripts_IsActive() &&
            !HallOfFameScripts_ShouldUpdateOverworld();
        if (hof_fullrate || CreditsScripts_IsActive() || RedsHouse1FScripts_IsActive() ||
            AmberScript_SceneWantsFullRate()) {

        } else {

        s_ow_accum += OW_GATE_NUM;
        if (s_ow_accum < OW_GATE_DEN) {

            s_ow_pressed_latch |= hJoyPressed;
            return;
        }
        s_ow_accum -= OW_GATE_DEN;

        hJoyPressed        |= s_ow_pressed_latch;
        s_ow_pressed_latch  = 0;
        }
    }

    if (gWarpPhase == WARP_FADE_OUT) {
        Display_SetPalette(kFadeOut[gWarpStep][0],
                           kFadeOut[gWarpStep][1],
                           kFadeOut[gWarpStep][2]);
        if (--gWarpStepTimer == 0) {
            gWarpStep++;
            gWarpStepTimer = FADE_TICKS_PER_STEP;
            if (gWarpStep >= FADE_OUT_STEPS) {

                Warp_Execute();
                fire_map_onload_callbacks();
                Map_BuildScrollView();
                NPC_BuildView(0, 0);
                Player_SyncOAM();
                Display_LoadMapPalette();

                if (!SSAnneDepart_IsActive())
                    MapMusic_FadeToForMap(wCurMap);
                gWarpPhase = WARP_NONE;
            }
        }
        return;
    }

    if (gWarpPhase == WARP_HOLE) {
        if (gHolePhase == HOLE_PHASE_INIT) {
            if (--gWarpStepTimer == 0) {

                wShadowOAM[2].y = 0;
                wShadowOAM[3].y = 0;
                gHolePhase = HOLE_PHASE_FADE_OUT;
                gWarpStep = 0;
                gWarpStepTimer = FADE_TICKS_PER_STEP;
                Display_SetPalette(kFadeOutToWhite[gWarpStep][0],
                                   kFadeOutToWhite[gWarpStep][1],
                                   kFadeOutToWhite[gWarpStep][2]);
            }
            return;
        }

        if (gHolePhase == HOLE_PHASE_FADE_OUT) {
            Display_SetPalette(kFadeOutToWhite[gWarpStep][0],
                               kFadeOutToWhite[gWarpStep][1],
                               kFadeOutToWhite[gWarpStep][2]);
            if (--gWarpStepTimer == 0) {
                gWarpStep++;
                gWarpStepTimer = FADE_TICKS_PER_STEP;
                if (gWarpStep >= FADE_IN_STEPS) {
                    Warp_Execute();
                    fire_map_onload_callbacks();
                    Map_BuildScrollView();
                    NPC_BuildView(0, 0);
                    Player_SyncOAM();

                    set_player_oam_top_y((int)kHoleDropY[0]);
                    gHolePhase = HOLE_PHASE_FADE_IN;
                    gWarpStep = 0;
                    gWarpStepTimer = FADE_TICKS_PER_STEP;
                    Display_SetPalette(kFadeInFromWhite[gWarpStep][0],
                                       kFadeInFromWhite[gWarpStep][1],
                                       kFadeInFromWhite[gWarpStep][2]);
                }
            }
            return;
        }

        if (gHolePhase == HOLE_PHASE_FADE_IN) {
            Display_SetPalette(kFadeInFromWhite[gWarpStep][0],
                               kFadeInFromWhite[gWarpStep][1],
                               kFadeInFromWhite[gWarpStep][2]);
            if (--gWarpStepTimer == 0) {
                gWarpStep++;
                gWarpStepTimer = FADE_TICKS_PER_STEP;
                if (gWarpStep >= FADE_IN_STEPS) {
                    Display_LoadMapPalette();

                    Audio_PlaySFX_TeleportEnter1();
                    gHolePhase = HOLE_PHASE_POST_DELAY;
                    gWarpStepTimer = HOLE_POST_FADE_DELAY_TICKS;
                }
            }
            return;
        }

        if (gHolePhase == HOLE_PHASE_POST_DELAY) {
            if (--gWarpStepTimer <= 0) {
                gHolePhase = HOLE_PHASE_DROP_IN;
                gHoleDropStep = 0;
            }
            return;
        }

        if (gHoleDropStep < (int)(sizeof(kHoleDropY) / sizeof(kHoleDropY[0]))) {
            gPlayerFacing = 0;
            Player_SyncOAM();
            set_player_oam_top_y((int)kHoleDropY[gHoleDropStep]);
            gHoleDropStep++;
            return;
        }

        gPlayerFacing = gHoleSavedFacing & 3;
        Player_SyncOAM();
        gWarpPhase = WARP_NONE;
        return;
    }

    if (gWarpPhase == WARP_TELEPORT) {
        static const int kSpinOrder[4] = { 0, 2, 1, 3 };
        int start = 0;
        for (int i = 0; i < 4; i++) {
            if (kSpinOrder[i] == (gTeleSavedFacing & 3)) {
                start = i;
                break;
            }
        }

        if (gTelePhase == TELE_PHASE_RISE) {
            if (gTeleStep < (int)(sizeof(kTeleRiseY) / sizeof(kTeleRiseY[0]))) {
                if (--gTeleStepTimer > 0) return;
                gTeleStepTimer = TELE_STEP_DELAY_TICKS;
                int fade_idx = gTeleStep;
                if (fade_idx >= FADE_IN_STEPS) fade_idx = FADE_IN_STEPS - 1;
                Display_SetPalette(kFadeOutToWhite[fade_idx][0],
                                   kFadeOutToWhite[fade_idx][1],
                                   kFadeOutToWhite[fade_idx][2]);
                gPlayerFacing = kSpinOrder[(start + gTeleStep) & 3];
                Player_SyncOAM();
                set_player_oam_top_y((int)kTeleRiseY[gTeleStep]);
                gTeleStep++;
                return;
            }
            Warp_Execute();
            fire_map_onload_callbacks();
            Map_BuildScrollView();
            NPC_BuildView(0, 0);
            gTelePhase = TELE_PHASE_FALL;
            gTeleStep = 0;
            gTeleStepTimer = TELE_STEP_DELAY_TICKS;
            Audio_PlaySFX_TeleportEnter1();
            return;
        }

        if (gTeleStep < (int)(sizeof(kTeleFallY) / sizeof(kTeleFallY[0]))) {
            if (--gTeleStepTimer > 0) return;
            gTeleStepTimer = TELE_STEP_DELAY_TICKS;
            int fade_idx = gTeleStep;
            if (fade_idx >= FADE_IN_STEPS) fade_idx = FADE_IN_STEPS - 1;
            Display_SetPalette(kFadeInFromWhite[fade_idx][0],
                               kFadeInFromWhite[fade_idx][1],
                               kFadeInFromWhite[fade_idx][2]);
            gPlayerFacing = kSpinOrder[(start + gTeleStep) & 3];
            Player_SyncOAM();
            set_player_oam_top_y((int)kTeleFallY[gTeleStep]);
            gTeleStep++;
            return;
        }

        gPlayerFacing = gTeleSavedFacing & 3;
        Player_SyncOAM();
        Display_LoadMapPalette();
        gWarpPhase = WARP_NONE;
        return;
    }

    if (gWarpPhase == WARP_FADE_IN) {
        Display_SetPalette(kFadeInFromWhite[gWarpStep][0],
                           kFadeInFromWhite[gWarpStep][1],
                           kFadeInFromWhite[gWarpStep][2]);
        if (--gWarpStepTimer == 0) {
            gWarpStep++;
            gWarpStepTimer = FADE_TICKS_PER_STEP;
            if (gWarpStep >= FADE_IN_STEPS) {
                Display_LoadMapPalette();
                gWarpPhase = WARP_NONE;
            }
        }
        return;
    }

    if (Trainer_IsEngaging()) {
        int r = Trainer_SightTick();
        NPC_Update();

        if (!Text_IsOpen()) {
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
        }
        if (r) {

            int player_level = 5;

            uint8_t johto_level = JohtoBattle_ArmPending(gEngagedJohtoParty);
            for (int i = 0; i < wPartyCount; i++) {
                if (wPartyMons[i].base.hp > 0) {
                    player_level = wPartyMons[i].level;
                    break;
                }
            }

            if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST && !wGymLeaderNo) {
                char track[32];
                int is_kanto = AmberScript_MapBank_GetMusicForRealId(wCurMap, track, sizeof(track))
                               && KantoMusic_ForTrackName(track) != MUSIC_NONE;
                if (is_kanto) {

                    Music_Play(battle_music_for_trainer(gEngagedTrainerClass));
                } else {

                    Music_Stop();
                    JohtoMusic_Play(CRYSTAL_MUSIC_JOHTO_TRAINER_BATTLE);
                }
            }
            else
                Music_Play(battle_music_for_trainer(gEngagedTrainerClass));
            gPendingTrainerBattle = 1;

            BattleTransition_Start(1, johto_level ? johto_level
                : Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
            gScene = SCENE_BTRANS;
        }
        return;
    }

    {
        int pallet_was_active = PalletScripts_IsActive();
        PalletScripts_Tick();
        if (PalletScripts_IsActive()) {

            if (pallet_was_active) {
                Player_Update();
                if (Warp_JustHappened()) {
                    begin_pending_warp_transition();
                    return;
                }
            }
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {

        int oakslab_was_active = OaksLabScripts_IsActive();
        OaksLabScripts_Tick();

        if (NamingScreen_IsOpen()) {
            return;
        }
        {
            uint8_t tr_class, tr_no;
            if (OaksLabScripts_GetPendingBattle(&tr_class, &tr_no)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                gEngagedTrainerClass = tr_class;
                gEngagedTrainerNo    = tr_no;
                Music_Play(battle_music_for_trainer(tr_class));
                gPendingTrainerBattle = 1;
                BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (OaksLabScripts_IsActive()) {

            if (Pokedex_IsShowingData()) {
                Pokedex_ShowDataTick();
                return;
            }
            if (oakslab_was_active) {
                Player_Update();
                if (Warp_JustHappened()) {
                    begin_pending_warp_transition();
                    return;
                }
            }
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                OaksLabScripts_PostRender();
            }

            NPC_BuildView(gScrollPxX, gScrollPxY);
            return;
        }
    }

    {
        RedsHouse1FScripts_Tick();
        if (RedsHouse1FScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        BluesHouseScripts_Tick();
        if (BluesHouseScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        ViridianMartScripts_Tick();
        if (ViridianMartScripts_IsActive()) {
            Player_Update();
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        CinnabarGymScripts_Tick();
        ViridianGymSpinners_Tick();
        GymScripts_Tick();
        {
            uint8_t tr_class, tr_no;
            if (GymScripts_GetPendingBattle(&tr_class, &tr_no)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                gEngagedTrainerClass = tr_class;
                gEngagedTrainerNo    = tr_no;
                Music_Play(battle_music_for_trainer(tr_class));
                gPendingTrainerBattle = 1;
                BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (GymScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        EliteFourScripts_Tick();
        {
            uint8_t tr_class, tr_no;
            if (EliteFourScripts_GetPendingBattle(&tr_class, &tr_no)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                gEngagedTrainerClass = tr_class;
                gEngagedTrainerNo    = tr_no;

                Music_Play(battle_music_for_trainer(tr_class));
                gPendingTrainerBattle = 1;

                BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (EliteFourScripts_IsActive()) {
            Player_Update();
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        ChampionsRoomScripts_Tick();
        {
            uint8_t tr_class, tr_no;
            if (ChampionsRoomScripts_GetPendingBattle(&tr_class, &tr_no)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                gEngagedTrainerClass = tr_class;
                gEngagedTrainerNo    = tr_no;

                Music_Play(MUSIC_CHAMPION_BATTLE);
                gPendingTrainerBattle = 1;
                BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (ChampionsRoomScripts_IsActive()) {
            Player_Update();
            if (Warp_JustHappened()) {
                begin_pending_warp_transition();
                return;
            }
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        CreditsScripts_Tick();
        if (CreditsScripts_ConsumeRestartRequest()) {

            Game_WarpToRealMap(0x00, 5, 6);
            wLastMap = 0x00;
            wLastBlackoutMap = 0x00;
            Save_Write();

            Music_Stop();
            JohtoMusic_Stop();
            gSkipMenu = 0;
            gSkipHallOfFameToCredits = 0;
            GameInit();
            return;
        }
        if (CreditsScripts_IsActive()) {
            return;
        }

        HallOfFameScripts_Tick();

        if (CreditsScripts_IsActive()) {
            return;
        }
        if (HallOfFameScripts_IsActive()) {
            if (HallOfFameScripts_ShouldUpdateOverworld()) {
                Player_Update();
                NPC_Update();
                if (!Text_IsOpen()) {
                    Map_BuildScrollView();
                    NPC_BuildView(gScrollPxX, gScrollPxY);
                }
            }
            return;
        }
    }

    {
        RocketHideoutScripts_Tick();
        if (RocketHideoutScripts_IsActive()) {

            Player_Update();
            if (Warp_JustHappened()) {
                begin_pending_warp_transition();
                return;
            }
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        PokemonTower6FScripts_Tick();
        {
            uint8_t species, enemy_level;
            if (PokemonTower6FScripts_GetPendingBattle(&species, &enemy_level)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                wCurPartySpecies = species;
                wCurEnemyLevel   = enemy_level;
                Music_Play(MUSIC_WILD_BATTLE);
                gPendingTrainerBattle = 0;
                BattleTransition_Start(0, enemy_level, player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (PokemonTower6FScripts_IsActive()) {

            Player_Update();
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        PokemonTower7FScripts_Tick();
        {
            uint8_t tr_class, tr_no;
            if (PokemonTower7FScripts_GetPendingBattle(&tr_class, &tr_no)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                gEngagedTrainerClass = tr_class;
                gEngagedTrainerNo    = tr_no;
                Music_Play(battle_music_for_trainer(tr_class));
                gPendingTrainerBattle = 1;
                BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (PokemonTower7FScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        Route24Scripts_Tick();
        {
            uint8_t tr_class, tr_no;
            if (Route24Scripts_GetPendingBattle(&tr_class, &tr_no)) {
                int player_level = 5;
                for (int i = 0; i < wPartyCount; i++) {
                    if (wPartyMons[i].base.hp > 0) {
                        player_level = wPartyMons[i].level;
                        break;
                    }
                }
                gEngagedTrainerClass = tr_class;
                gEngagedTrainerNo    = tr_no;
                Music_Play(battle_music_for_trainer(tr_class));
                gPendingTrainerBattle = 1;
                BattleTransition_Start(1, Battle_PeekTrainerLevelForTransition(gEngagedTrainerClass, gEngagedTrainerNo), player_level);
                gScene = SCENE_BTRANS;
                return;
            }
        }
        if (Route24Scripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        BillsHouseScripts_Tick();
        if (BillsHouseScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                BillsHouseScripts_PostRender();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        Route2GateScripts_Tick();
        if (Route2GateScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    SSAnneDepart_Tick();

    Route1Scripts_Tick();

    {
        CeladonGiftScripts_Tick();
        if (CeladonGiftScripts_IsActive()) {

            if (NamingScreen_IsOpen()) return;
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                CeladonGiftScripts_PostRender();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        SaffronCityScripts_Tick();
        if (SaffronCityScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        SafariZoneScripts_Tick();
        if (SafariZoneScripts_IsActive()) {

            if (!Text_IsOpen() && !YesNo_IsOpen()) {
                Player_Update();
                if (Warp_JustHappened()) {
                    begin_pending_warp_transition();
                    return;
                }
            }
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
                SafariZoneScripts_PostRender();
            }
            return;
        }
    }

    {
        SafariZoneSecretHouseScripts_Tick();
        if (SafariZoneSecretHouseScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    {
        WardensHouseScripts_Tick();
        if (WardensHouseScripts_IsActive()) {
            NPC_Update();
            if (!Text_IsOpen()) {
                Map_BuildScrollView();
                NPC_BuildView(gScrollPxX, gScrollPxY);
            }
            return;
        }
    }

    const int turning = Player_IsTurning();

    if ((hJoyPressed & PAD_START) && !turning && !Player_IsMoving() && gWarpPhase == WARP_NONE
        && !AmberScript_Scene_IsActive() && !Player_IsSimulatingMovement()) {
        Audio_PlaySFX_StartMenu();
        Menu_Open();
        return;
    }

    check_item_pickup();

    MansionScripts_Tick();
    if (MansionScripts_IsActive()) {

        if (YesNo_IsOpen()) {
            YesNo_Tick();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            YesNo_PostRender();
        }
        return;
    }

    CyclingRoadGate_Tick();
    if (Text_IsOpen()) return;
    if (!turning && !Gate_PewterIsActive() && !AmberScript_Scene_IsActive())
        check_npc_interact();

    if (Pokemart_IsActive() || Pokecenter_IsActive() || OaksLabScripts_IsActive() || ViridianMartScripts_IsActive() || Daycare_IsActive()) return;
    SafariZoneScripts_GateStepCheck();
    if (Text_IsOpen()) return;

    NPC_BuildView(0, 0);
    if (Text_IsOpen()) return;
    if (!turning) check_hidden_event();
    if (Text_IsOpen()) return;
    if (!turning) check_sign_interact();
    if (Text_IsOpen()) return;
    if (ElevatorMenu_IsOpen()) return;
    if (YesNo_IsOpen()) {

        YesNo_Tick();
        Map_BuildScrollView();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        YesNo_PostRender();

        MoneyBox_Refresh();
        CoinBox_Refresh();
        return;
    }
    if (BikeShopMenu_IsOpen()) {

        BikeShopMenu_Tick();
        Map_BuildScrollView();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        BikeShopMenu_PostRender();
        return;
    }

    if (Warp_HasDoorStep()) {
        if (Warp_ConsumeDoorStepFaceUp())
            Player_ForceStepFromDoorFacingUp();
        else
            Player_ForceStepFromDoor();
    }

    Gate_ViridianDoPush();
    Gate_CinnabarGymLockDoPush();
    Gate_SaffronDoPush();
    Gate_BadgeJingleTick();
    Gate_Route22GateTick();
    Gate_Route22DoPush();
    Gate_Route23DoPush();
    Gate_CyclingRoadTick();
    Gate_PewterTick();
    SeafoamScripts_Tick();
    VictoryRoadScripts_Tick();

    if (PokeFlute_ConsumeSnorlaxBattle()) {
        wCurPartySpecies = SPECIES_SNORLAX;
        wCurEnemyLevel   = 30;
        Music_Play(MUSIC_WILD_BATTLE);
        int player_level = 5;
        for (int i = 0; i < wPartyCount; i++) {
            if (wPartyMons[i].base.hp > 0) {
                player_level = wPartyMons[i].level;
                break;
            }
        }
        BattleTransition_Start(0, 30, player_level);
        gScene = SCENE_BTRANS;
        return;
    }

    if (SeafoamScripts_ConsumeArticunoBattle()) {
        wCurPartySpecies = SPECIES_ARTICUNO;
        wCurEnemyLevel   = 50;
        Music_Play(MUSIC_WILD_BATTLE);
        int player_level = 5;
        for (int i = 0; i < wPartyCount; i++) {
            if (wPartyMons[i].base.hp > 0) {
                player_level = wPartyMons[i].level;
                break;
            }
        }
        BattleTransition_Start(0, 50, player_level);
        gScene = SCENE_BTRANS;
        return;
    }

    if (VictoryRoadScripts_ConsumeMoltresBattle()) {
        wCurPartySpecies = SPECIES_MOLTRES;
        wCurEnemyLevel   = 50;
        Music_Play(MUSIC_WILD_BATTLE);
        int player_level = 5;
        for (int i = 0; i < wPartyCount; i++) {
            if (wPartyMons[i].base.hp > 0) {
                player_level = wPartyMons[i].level;
                break;
            }
        }
        BattleTransition_Start(0, 50, player_level);
        gScene = SCENE_BTRANS;
        return;
    }

    if (Bicycle_IsCyclingRoad() && !Trainer_IsEngaging() &&
        (hJoyHeld & (PAD_CTRL_PAD | PAD_A | PAD_B)) == 0) {
        hJoyHeld = PAD_DOWN;
    }

    Player_Update();

    if (Warp_JustHappened()) {
        begin_pending_warp_transition();
        return;
    }

    NPC_Update();

    if (gStepJustCompleted) {
        gStepJustCompleted = 0;

        Gate_ViridianStepCheck();
        Gate_CinnabarGymLockStepCheck();
        Gate_Route22StepCheck();
        Gate_Route23StepCheck();
        Gate_SaffronStepCheck();
        Gate_CyclingRoadStepCheck();
        Gate_PewterEastCheck();
        Route24Scripts_StepCheck();
        PokemonTower6FScripts_StepCheck();
        PokemonTower7FScripts_StepCheck();
        RocketHideoutScripts_StepCheck();
        ViridianGymSpinners_StepCheck();
        SafariZoneScripts_StepCheck();
        SafariZoneScripts_GateStepCheck();

        Bicycle_SeafoamCurrentStepCheck();
        SeafoamScripts_StepCheck();

        Daycare_StepCheck();
        Poison_StepCheck();
        if (Text_IsOpen()) return;

        Trainer_CheckSight();
        if (!Trainer_IsEngaging())
            check_wild_encounter();
    }

    if (gScene == SCENE_OVERWORLD) {
        Map_BuildScrollView();
        DebugCLI_PostRender();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        if (YesNo_IsOpen())
            YesNo_PostRender();
        if (BikeShopMenu_IsOpen())
            BikeShopMenu_PostRender();
    }
}
