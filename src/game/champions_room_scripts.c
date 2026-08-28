#include "champions_room_scripts.h"
#include "rom_text.h"
#include "constants.h"
#include "music.h"
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "rival_starter.h"
#include "text.h"
#include "trainer_sight.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_CHAMPIONS_ROOM 0x78
#define CHAMPIONS_ROOM_RIVAL_IDX 0
#define CHAMPIONS_ROOM_OAK_IDX 1
#define CHAMPION_RIVAL_CLASS 43

static uint8_t crs_cur_map(void) {
    int id = Map_CurrentRealId();
    return (id < 0) ? 0xFFu : (uint8_t)id;
}

typedef enum {
    CRS_IDLE = 0,
    CRS_PLAYER_ENTERS_WAIT,
    CRS_PRE_TEXT_DELAY_WAIT,
    CRS_PRE_BATTLE_WAIT,
    CRS_PRE_BATTLE_DELAY_WAIT,
    CRS_RIVAL_DEFEATED_WAIT,
    CRS_RIVAL_AFTER_WAIT,
    CRS_OAK_ARRIVES_WAIT,
    CRS_OAK_CONGRATS_WAIT,
    CRS_OAK_DISAPPOINT_WAIT,
    CRS_OAK_COME_WITH_ME_WAIT,
    CRS_OAK_EXITS_WAIT,
    CRS_CLEANUP_WAIT,
} champion_room_state_t;

static champion_room_state_t g_state = CRS_IDLE;
static int g_started_on_entry = 0;
static int g_pending_battle = 0;
static int g_battle_active = 0;
static int g_last_battle_won = 0;
static int g_oak_step = 0;
static int g_oak_exit_step = 0;
static int g_pending_after_rival_text = 0;
static int g_delay_timer = 0;

#define kRivalIntroText (RomText("ChampionsRoomRivalText.IntroText"))

#define kRivalDefeatedText (RomTextPrefixed("{RIVAL}: ", "RivalDefeatedText"))
#define kRivalAfterBattleText (RomText("ChampionsRoomRivalAfterBattleText"))
#define kOakArriveText (RomText("ChampionsRoomOakText"))

static char kOakCongratsTextBuf[192];
#define kOakDisappointedText (RomText("ChampionsRoomOakDisappointedWithRivalText"))
#define kOakComeWithMeText (RomText("ChampionsRoomOakComeWithMeText"))

static const int8_t kRivalEntry_3_7[] = { 1, 1, 1, 3, 1, -1 };
static const int8_t kRivalEntry_3_8[] = { 1, 1, 1, 1, 3, 1, -1 };
static const int8_t kRivalEntry_4_7[] = { 1, 1, 1, 1, -1 };
static const int8_t kRivalEntry_4_8[] = { 1, 1, 1, 1, 1, -1 };
static const int8_t kFollowOakMove[] = { 1, 1, 1, 1, 2, -1 };

#define OAK_SCRIPT_STEP_FRAMES 16
#define OAK_SCRIPT_STEP_PX      1

static void hide_oak_if_needed(void) {
    if (crs_cur_map() != MAP_CHAMPIONS_ROOM) return;
    if (!CheckEvent(EVENT_BEAT_CHAMPION_RIVAL)) {
        NPC_HideSprite(CHAMPIONS_ROOM_OAK_IDX);
    }
}

static uint8_t champion_rival_trainer_no(void) {
    uint8_t rival_starter = RivalStarter_Get();
    if (rival_starter == STARTER2) return 1;
    if (rival_starter == STARTER3) return 2;
    return 3;
}

void ChampionsRoomScripts_OnMapLoad(void) {
    if (crs_cur_map() != MAP_CHAMPIONS_ROOM) return;
    hide_oak_if_needed();
    g_state = CRS_IDLE;
    g_started_on_entry = 0;
    g_pending_battle = 0;
    g_battle_active = 0;
    g_last_battle_won = 0;
    g_oak_step = 0;
    g_oak_exit_step = 0;
    g_pending_after_rival_text = 0;
    g_delay_timer = 0;
}

static void start_player_move(const int8_t *seq, int count, champion_room_state_t next_state) {
    hJoyPressed = 0;
    hJoyHeld = 0;
    Player_StartSimulatedMovement(seq, count);
    g_state = next_state;
}

static void step_oak_up(void) {
    if (NPC_IsWalking(CHAMPIONS_ROOM_OAK_IDX)) return;
    if (g_oak_step < 5) {
        NPC_DoScriptedStepTimed(CHAMPIONS_ROOM_OAK_IDX, 1, OAK_SCRIPT_STEP_FRAMES, OAK_SCRIPT_STEP_PX);
        g_oak_step++;
        return;
    }
    NPC_SetFacing(CHAMPIONS_ROOM_RIVAL_IDX, 2);
    NPC_SetFacing(CHAMPIONS_ROOM_OAK_IDX, 0);

    NPC_BuildView(gScrollPxX, gScrollPxY);
    RomTextSplice(kOakCongratsTextBuf, sizeof(kOakCongratsTextBuf),
                 "_ChampionsRoomOakCongratulatesPlayerText", "{badge}", "{STARTER}");
    Text_ShowASCII(kOakCongratsTextBuf);
    g_state = CRS_OAK_DISAPPOINT_WAIT;
}

static void step_oak_exit(void) {
    if (NPC_IsWalking(CHAMPIONS_ROOM_OAK_IDX)) return;
    if (g_oak_exit_step < 2) {
        NPC_DoScriptedStepTimed(CHAMPIONS_ROOM_OAK_IDX, 1, OAK_SCRIPT_STEP_FRAMES, OAK_SCRIPT_STEP_PX);
        g_oak_exit_step++;
        return;
    }
    NPC_HideSprite(CHAMPIONS_ROOM_OAK_IDX);
    start_player_move(kFollowOakMove, 4, CRS_CLEANUP_WAIT);
}

void ChampionsRoomScripts_Tick(void) {
    if (crs_cur_map() != MAP_CHAMPIONS_ROOM) return;

    if (!CheckEvent(EVENT_BEAT_CHAMPION_RIVAL) &&
        !g_started_on_entry &&
        g_state == CRS_IDLE &&
        ((int)wYCoord == 7 || (int)wYCoord == 8) &&
        ((int)wXCoord == 3 || (int)wXCoord == 4)) {
        g_started_on_entry = 1;
        if ((int)wXCoord == 3 && (int)wYCoord == 7) start_player_move(kRivalEntry_3_7, 4, CRS_PLAYER_ENTERS_WAIT);
        else if ((int)wXCoord == 3 && (int)wYCoord == 8) start_player_move(kRivalEntry_3_8, 5, CRS_PLAYER_ENTERS_WAIT);
        else if ((int)wXCoord == 4 && (int)wYCoord == 7) start_player_move(kRivalEntry_4_7, 3, CRS_PLAYER_ENTERS_WAIT);
        else start_player_move(kRivalEntry_4_8, 4, CRS_PLAYER_ENTERS_WAIT);
        return;
    }

    switch (g_state) {
    case CRS_PLAYER_ENTERS_WAIT:
        if (!Player_IsSimulatingMovement()) {

            if (Player_IsMoving()) {
                break;
            }

            if ((int)wYCoord > 3) {
                Player_DoScriptedStep(1);
                break;
            }
            Player_IgnoreInputFrames(3);
            g_delay_timer = 3;
            g_state = CRS_PRE_TEXT_DELAY_WAIT;
        }
        break;
    case CRS_PRE_TEXT_DELAY_WAIT:
        if (g_delay_timer > 0) g_delay_timer--;
        if (g_delay_timer <= 0) {
            Text_ShowASCII(kRivalIntroText);
            g_state = CRS_PRE_BATTLE_WAIT;
        }
        break;
    case CRS_PRE_BATTLE_WAIT:
        if (!Text_IsOpen()) {
            g_delay_timer = 3;
            g_state = CRS_PRE_BATTLE_DELAY_WAIT;
        }
        break;
    case CRS_PRE_BATTLE_DELAY_WAIT:
        if (g_delay_timer > 0) g_delay_timer--;
        if (g_delay_timer <= 0) {
            gTrainerAfterText = kRivalDefeatedText;
            g_pending_battle = 1;
            g_battle_active = 0;
            g_state = CRS_RIVAL_DEFEATED_WAIT;
        }
        break;
    case CRS_RIVAL_DEFEATED_WAIT:
        if (!g_battle_active && g_last_battle_won) {
            g_pending_after_rival_text = 1;
            g_last_battle_won = 0;
        }
        if (g_pending_after_rival_text && !Text_IsOpen()) {
            Text_ShowASCII(kRivalAfterBattleText);
            g_pending_after_rival_text = 0;
            g_state = CRS_RIVAL_AFTER_WAIT;
        }
        break;
    case CRS_RIVAL_AFTER_WAIT:
        if (!Text_IsOpen()) {

            Music_PlayCities1AlternateTempo();
            Text_ShowASCII(kOakArriveText);
            g_state = CRS_OAK_ARRIVES_WAIT;
        }
        break;
    case CRS_OAK_ARRIVES_WAIT:
        if (!Text_IsOpen()) {
            NPC_ShowSprite(CHAMPIONS_ROOM_OAK_IDX);
            NPC_SetFacing(CHAMPIONS_ROOM_OAK_IDX, 1);
            g_oak_step = 0;
            g_state = CRS_OAK_CONGRATS_WAIT;
        }
        break;
    case CRS_OAK_CONGRATS_WAIT:
        if (!Text_IsOpen()) {
            step_oak_up();
        }
        break;
    case CRS_OAK_DISAPPOINT_WAIT:
        if (!Text_IsOpen()) {
            NPC_SetFacing(CHAMPIONS_ROOM_OAK_IDX, 3);
            NPC_BuildView(gScrollPxX, gScrollPxY);
            Text_ShowASCII(kOakDisappointedText);
            g_state = CRS_OAK_COME_WITH_ME_WAIT;
        }
        break;
    case CRS_OAK_COME_WITH_ME_WAIT:
        if (!Text_IsOpen()) {
            NPC_SetFacing(CHAMPIONS_ROOM_OAK_IDX, 0);
            NPC_SetFacing(CHAMPIONS_ROOM_RIVAL_IDX, 3);
            NPC_BuildView(gScrollPxX, gScrollPxY);
            Text_ShowASCII(kOakComeWithMeText);
            g_oak_exit_step = 0;
            g_state = CRS_OAK_EXITS_WAIT;
        }
        break;
    case CRS_OAK_EXITS_WAIT:
        if (!Text_IsOpen()) {
            step_oak_exit();
        }
        break;
    case CRS_CLEANUP_WAIT:
        if (!Player_IsSimulatingMovement()) {
            g_state = CRS_IDLE;
        }
        break;
    default:
        break;
    }
}

int ChampionsRoomScripts_IsActive(void) {
    return (crs_cur_map() == MAP_CHAMPIONS_ROOM) && (g_state != CRS_IDLE);
}

int ChampionsRoomScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out) {
    if (!g_pending_battle) return 0;
    g_pending_battle = 0;
    if (class_out) *class_out = CHAMPION_RIVAL_CLASS;
    if (no_out) *no_out = champion_rival_trainer_no();
    g_battle_active = 1;
    return 1;
}

int ChampionsRoomScripts_ConsumeBattle(void) {
    if (!g_battle_active) return 0;
    g_battle_active = 0;
    return 1;
}

void ChampionsRoomScripts_OnVictory(void) {
    SetEvent(EVENT_BEAT_CHAMPION_RIVAL);
    g_last_battle_won = 1;
}

void ChampionsRoomScripts_OnDefeat(void) {
    g_battle_active = 0;
    g_last_battle_won = 0;
    g_state = CRS_IDLE;
}

void ChampionsRoomScripts_RivalInteract(void) {
    if (crs_cur_map() != MAP_CHAMPIONS_ROOM) return;
    if (CheckEvent(EVENT_BEAT_CHAMPION_RIVAL)) {
        Text_ShowASCII(kRivalAfterBattleText);
        return;
    }
    if (g_state == CRS_IDLE) {
        Text_ShowASCII(kRivalIntroText);
        g_state = CRS_PRE_BATTLE_WAIT;
    }
}

void ChampionsRoomScripts_OakInteract(void) {
    if (crs_cur_map() != MAP_CHAMPIONS_ROOM) return;
    if (CheckEvent(EVENT_BEAT_CHAMPION_RIVAL)) {
        Text_ShowASCII(RomText("ChampionsRoomOakText"));
    }
}
