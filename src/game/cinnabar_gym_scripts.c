#include "cinnabar_gym_scripts.h"
#include "trainer_sight.h"
#include "rom_text.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "gym_scripts.h"
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "text.h"
#include "yesno.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_CINNABAR_GYM 0xA6
#define DIR_UP 1
#define DIR_LEFT 2

static int in_cinnabar_gym(void) {
    int real = Map_CurrentRealId();
    return (real >= 0 ? real : (int)wCurMap) == MAP_CINNABAR_GYM;
}

typedef struct {
    uint8_t x, y;
    uint8_t closed_block;
} gate_block_t;

typedef struct {
    uint8_t trainer_class;
    uint8_t trainer_no;
    uint16_t beat_flag;
    const char *pre_text;
    const char *end_text;
    const char *after_text;
} cinnabar_trainer_t;

static const gate_block_t kGateBlocks[6] = {
    { 9, 3, 0x54 }, { 6, 3, 0x54 }, { 6, 6, 0x54 },
    { 3, 8, 0x5f }, { 2, 6, 0x54 }, { 2, 3, 0x54 },
};

static const uint8_t kTrainerTile[7][2] = {
    { 17,  2 }, { 17,  8 }, { 11,  4 }, { 11,  8 },
    { 11, 14 }, {  3, 14 }, {  3,  8 },
};

#define kQuizIntroText (RomText("CinnabarGymQuizIntroText"))

static const char *QuizQuestion(int idx) {
    switch (idx) {
    case 0: return RomText("CinnabarQuizQuestionsText1");
    case 1: return RomText("CinnabarQuizQuestionsText2");
    case 2: return RomText("CinnabarQuizQuestionsText3");
    case 3: return RomText("CinnabarQuizQuestionsText4");
    case 4: return RomText("CinnabarQuizQuestionsText5");
    case 5: return RomText("CinnabarQuizQuestionsText6");
    default: return "";
    }
}

static const uint8_t kQuizAnswerYes[6] = { 1, 0, 0, 0, 1, 0 };
#define kQuizCorrectText (RomText("_CinnabarGymQuizCorrectText"))
#define kQuizIncorrectText (RomText("CinnabarGymQuizIncorrectText"))

static cinnabar_trainer_t kTrainers[7] = {
    { 8, 9, EVENT_BEAT_CINNABAR_GYM_TRAINER_0, NULL, NULL, NULL },
    { 11, 4, EVENT_BEAT_CINNABAR_GYM_TRAINER_1, NULL, NULL, NULL },
    { 8, 10, EVENT_BEAT_CINNABAR_GYM_TRAINER_2, NULL, NULL, NULL },
    { 11, 5, EVENT_BEAT_CINNABAR_GYM_TRAINER_3, NULL, NULL, NULL },
    { 8, 11, EVENT_BEAT_CINNABAR_GYM_TRAINER_4, NULL, NULL, NULL },
    { 11, 6, EVENT_BEAT_CINNABAR_GYM_TRAINER_5, NULL, NULL, NULL },
    { 8, 12, EVENT_BEAT_CINNABAR_GYM_TRAINER_6, NULL, NULL, NULL },
};

static int s_trainerTextInit = 0;
static void init_trainer_text(void) {
    if (s_trainerTextInit) return;
    s_trainerTextInit = 1;
    kTrainers[0].pre_text   = RomText("CinnabarGymSuperNerd1.BattleText");
    kTrainers[0].end_text   = RomText("CinnabarGymSuperNerd1.EndBattleText");
    kTrainers[0].after_text = RomText("CinnabarGymSuperNerd1.AfterBattleText");
    kTrainers[1].pre_text   = RomText("CinnabarGymSuperNerd2.BattleText");
    kTrainers[1].end_text   = RomText("CinnabarGymSuperNerd2.EndBattleText");
    kTrainers[1].after_text = RomText("CinnabarGymSuperNerd2.AfterBattleText");
    kTrainers[2].pre_text   = RomText("CinnabarGymSuperNerd3.BattleText");
    kTrainers[2].end_text   = RomText("CinnabarGymSuperNerd3.EndBattleText");
    kTrainers[2].after_text = RomText("CinnabarGymSuperNerd3.AfterBattleText");
    kTrainers[3].pre_text   = RomText("CinnabarGymSuperNerd4.BattleText");
    kTrainers[3].end_text   = RomText("CinnabarGymSuperNerd4.EndBattleText");
    kTrainers[3].after_text = RomText("CinnabarGymSuperNerd4.AfterBattleText");
    kTrainers[4].pre_text   = RomText("CinnabarGymSuperNerd5.BattleText");
    kTrainers[4].end_text   = RomText("CinnabarGymSuperNerd5.EndBattleText");
    kTrainers[4].after_text = RomText("CinnabarGymSuperNerd5.AfterBattleText");
    kTrainers[5].pre_text   = RomText("CinnabarGymSuperNerd6.BattleText");
    kTrainers[5].end_text   = RomText("CinnabarGymSuperNerd6.EndBattleText");
    kTrainers[5].after_text = RomText("CinnabarGymSuperNerd6.AfterBattleText");
    kTrainers[6].pre_text   = RomText("CinnabarGymSuperNerd7.BattleText");
    kTrainers[6].end_text   = RomText("CinnabarGymSuperNerd7.EndBattleText");
    kTrainers[6].after_text = RomText("CinnabarGymSuperNerd7.AfterBattleText");
}

#define kGuidePreText (RomText("CinnabarGymGymGuideText.ChampInMakingText"))
#define kGuidePostText (RomText("CinnabarGymGymGuideText.BeatBlaineText"))

static uint8_t s_pendingWrongTrainer = 0;
static uint8_t s_pendingUnlockGate = 0;
static uint8_t s_movePhase = 0;
static uint8_t s_battleTrainer = 0;
static int     s_moveNpcIdx = -1;
static uint8_t s_quizGate = 0;
static uint8_t s_quizState = 0;

static const char *const kGatePrefix[6] = {
    "cinnabargym_gate0", "cinnabargym_gate1", "cinnabargym_gate2",
    "cinnabargym_gate3", "cinnabargym_gate4", "cinnabargym_gate5",
};

static void apply_gate_blocks(void) {
    for (int i = 0; i < 6; i++) {
        int open = CheckEvent((uint16_t)(EVENT_CINNABAR_GYM_GATE0_UNLOCKED + i));
        AmberScript_PlaceSwapBlock(kGatePrefix[i], open ? "open" : "closed",
                                   kGateBlocks[i].x, kGateBlocks[i].y);
    }

}

static void unlock_gate(uint8_t gate_idx_1based) {
    if (gate_idx_1based < 1 || gate_idx_1based > 6) return;
    if (!CheckEvent((uint16_t)(EVENT_CINNABAR_GYM_GATE0_UNLOCKED + gate_idx_1based - 1)))
        Audio_PlaySFX_GoInside();
    SetEvent((uint16_t)(EVENT_CINNABAR_GYM_GATE0_UNLOCKED + gate_idx_1based - 1));
    apply_gate_blocks();
}

static void start_trainer_by_idx(uint8_t idx1) {
    if (idx1 < 1 || idx1 > 7) return;
    init_trainer_text();
    const cinnabar_trainer_t *t = &kTrainers[idx1 - 1];
    if (CheckEvent(t->beat_flag)) {
        Text_ShowASCII(t->after_text);
        return;
    }

    s_battleTrainer = idx1;
    s_pendingUnlockGate = (idx1 >= 2) ? (uint8_t)(idx1 - 1) : 0;
    GymScripts_SetTrainerPending(t->trainer_class, t->trainer_no, t->beat_flag,
                                 t->end_text, t->after_text, t->pre_text);
}

static void quiz_interact(uint8_t gate_idx_1based) {
    if (!in_cinnabar_gym()) return;
    if (gate_idx_1based < 1 || gate_idx_1based > 6) return;

    if (gPlayerFacing != DIR_UP) return;
    if (s_quizState != 0) return;

    if (Trainer_IsEngaging()) return;

    if (s_pendingWrongTrainer != 0) return;

    if (CheckEvent((uint16_t)(EVENT_CINNABAR_GYM_GATE0_UNLOCKED + gate_idx_1based - 1)))
        return;
    s_quizGate = gate_idx_1based;
    s_quizState = 1;
    Text_ShowASCII(kQuizIntroText);
}

void CinnabarGymScripts_OnMapLoad(void) {
    if (!in_cinnabar_gym()) return;
    ClearEvent(EVENT_2A7);
    s_pendingWrongTrainer = 0;
    s_pendingUnlockGate = 0;
    s_movePhase = 0;
    s_battleTrainer = 0;
    s_quizGate = 0;
    s_quizState = 0;
    s_moveNpcIdx = -1;
    apply_gate_blocks();
}

void CinnabarGymScripts_Tick(void) {
    if (!in_cinnabar_gym()) return;

    apply_gate_blocks();

    if (s_pendingUnlockGate != 0 && s_battleTrainer != 0 && wIsInBattle == 0) {
        const uint16_t beat = kTrainers[s_battleTrainer - 1].beat_flag;
        if (CheckEvent(beat)) {
            unlock_gate(s_pendingUnlockGate);
            s_pendingUnlockGate = 0;
            s_battleTrainer = 0;
        }
    }

    if (s_quizState != 0) {
        if (s_quizState == 2) {
            if (Text_IsOpen()) return;

            YesNo_Show(QuizQuestion(s_quizGate - 1));
            s_quizState = 3;
            return;
        }
        if (s_quizState == 3) {
            YesNo_Tick();
            if (YesNo_IsOpen()) return;
            {
                int yes = YesNo_GetResult();
                if (yes == (int)kQuizAnswerYes[s_quizGate - 1]) {
                    Audio_PlaySFX_GetItem1();
                    Text_ShowASCII(kQuizCorrectText);
                    unlock_gate(s_quizGate);
                } else {
                    Audio_PlaySFX_Denied();
                    Text_ShowASCII(kQuizIncorrectText);
                    {
                        uint8_t tr = (uint8_t)(s_quizGate + 1);
                        if (!CheckEvent(kTrainers[tr - 1].beat_flag)) {
                            s_pendingWrongTrainer = tr;
                            s_pendingUnlockGate = s_quizGate;
                            s_battleTrainer = tr;
                            s_moveNpcIdx = NPC_FindAtTile(kTrainerTile[tr - 1][0],
                                                          kTrainerTile[tr - 1][1]);
                            s_movePhase = 1;
                        }
                    }
                }
            }
            s_quizState = 0;
            s_quizGate = 0;
            return;
        }
        if (s_quizState == 1) {
            if (Text_IsOpen()) return;
            s_quizState = 2;
            return;
        }
    }

    if (s_pendingWrongTrainer == 0) return;
    if (Text_IsOpen()) return;

    const int npc_idx = s_moveNpcIdx;
    if (npc_idx < 0 || npc_idx >= NPC_GetCount()) {
        s_pendingWrongTrainer = 0;
        s_movePhase = 0;
        s_moveNpcIdx = -1;
        return;
    }

    if (s_movePhase == 1) {
        if (!NPC_IsWalking(npc_idx)) {
            NPC_DoScriptedStep(npc_idx, DIR_LEFT);
            s_movePhase = (s_pendingWrongTrainer == 3) ? 2 : 3;
        }
        return;
    }
    if (s_movePhase == 2) {
        if (!NPC_IsWalking(npc_idx)) {
            NPC_DoScriptedStep(npc_idx, DIR_UP);
            s_movePhase = 3;
        }
        return;
    }
    if (s_movePhase == 3 && !NPC_IsWalking(npc_idx)) {
        start_trainer_by_idx(s_pendingWrongTrainer);
        s_pendingWrongTrainer = 0;
        s_movePhase = 0;
        s_moveNpcIdx = -1;
    }
}

void CinnabarGymScripts_Quiz1Interact(void) { quiz_interact(1); }
void CinnabarGymScripts_Quiz2Interact(void) { quiz_interact(2); }
void CinnabarGymScripts_Quiz3Interact(void) { quiz_interact(3); }
void CinnabarGymScripts_Quiz4Interact(void) { quiz_interact(4); }
void CinnabarGymScripts_Quiz5Interact(void) { quiz_interact(5); }
void CinnabarGymScripts_Quiz6Interact(void) { quiz_interact(6); }

void CinnabarGymScripts_Trainer1Interact(void) { start_trainer_by_idx(1); }
void CinnabarGymScripts_Trainer2Interact(void) { start_trainer_by_idx(2); }
void CinnabarGymScripts_Trainer3Interact(void) { start_trainer_by_idx(3); }
void CinnabarGymScripts_Trainer4Interact(void) { start_trainer_by_idx(4); }
void CinnabarGymScripts_Trainer5Interact(void) { start_trainer_by_idx(5); }
void CinnabarGymScripts_Trainer6Interact(void) { start_trainer_by_idx(6); }
void CinnabarGymScripts_Trainer7Interact(void) { start_trainer_by_idx(7); }
void CinnabarGymScripts_GuideInteract(void) {
    Text_ShowASCII(CheckEvent(EVENT_BEAT_BLAINE) ? kGuidePostText : kGuidePreText);
}
