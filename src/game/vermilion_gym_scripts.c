
#include "vermilion_gym_scripts.h"
#include "rom_text.h"
#include "gym_scripts.h"
#include "overworld.h"
#include "text.h"
#include "amberscript_mapbank.h"
#include "amberscript_tilemod.h"
#include <stdio.h>
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdint.h>
#include <string.h>

#define VG_GENTLEMAN_CLASS  41
#define VG_GENTLEMAN_NO      3
#define VG_ROCKER_CLASS     20
#define VG_ROCKER_NO         1
#define VG_SAILOR_CLASS     19
#define VG_SAILOR_NO         8

static const uint8_t kGymTrashCans[15 * 5] = {
    2,  1,  3,  0,  0,
    3,  0,  2,  4,  0,
    2,  1,  5,  0,  0,
    3,  0,  4,  6,  0,
    4,  1,  3,  5,  7,
    3,  2,  4,  8,  0,
    3,  3,  7,  9,  0,
    4,  4,  6,  8, 10,
    3,  5,  7, 11,  0,
    3,  6, 10, 12,  0,
    4,  7,  9, 11, 13,
    3,  8, 10, 14,  0,
    2,  9, 13,  0,  0,
    3, 10, 12, 14,  0,
    2, 11, 13,  0,  0,
};

#define kTrashText (RomText("VermilionGymTrashText"))
#define kLock1Text (RomText("_VermilionGymTrashSuccessText1"))
#define kLock2Text (RomText("_VermilionGymTrashSuccessText3"))
#define kFailText (RomText("_VermilionGymTrashFailText"))

#define kGentlemanPre (RomText("VermilionGymGentlemanBattleText"))
#define kGentlemanEnd (RomText("VermilionGymGentlemanEndBattleText"))
#define kGentlemanAfter (RomText("VermilionGymGentlemanAfterBattleText"))

#define kRockerPre (RomText("VermilionGymSuperNerdBattleText"))
#define kRockerEnd (RomText("VermilionGymSuperNerdEndBattleText"))
#define kRockerAfter (RomText("VermilionGymSuperNerdAfterBattleText"))

#define kSailorPre (RomText("VermilionGymSailorBattleText"))
#define kSailorEnd (RomText("VermilionGymSailorEndBattleText"))
#define kSailorAfter (RomText("VermilionGymSailorAfterBattleText"))

static void place_gate(const char *state) {
    if (!AmberScript_PlaceSwapBlock("vermiliongym_gate", state, 2, 2)) {
        printf("[vermiliongym] gate tile swap FAILED (vermiliongym_gate_%s_* "
               "not defined) -- regenerate with "
               "tools/romimport/emit_kanto.py --all\n", state);
        fflush(stdout);
    }
}

static void open_door(void)  { place_gate("open");   }
static void close_door(void) { place_gate("closed"); }

static void gym_trash_script(int can_index) {

    if (CheckEvent(EVENT_2ND_LOCK_OPENED)) {
        Text_ShowASCII(kTrashText);
        return;
    }

    if (!CheckEvent(EVENT_1ST_LOCK_OPENED)) {

        if ((uint8_t)can_index != wFirstLockTrashCanIndex) {
            Text_ShowASCII(kTrashText);
            return;
        }
        SetEvent(EVENT_1ST_LOCK_OPENED);

        uint8_t mask       = kGymTrashCans[can_index * 5];
        uint8_t raw        = BattleRandom();
        uint8_t swapped    = (uint8_t)(((raw & 0x0F) << 4) | ((raw >> 4) & 0x0F));
        uint8_t combined   = swapped & mask;
        uint8_t offset     = (uint8_t)(combined - 1u);
        if (combined == 0) {

            wSecondLockTrashCanIndex = 0;
        } else {
            wSecondLockTrashCanIndex = kGymTrashCans[can_index * 5 + 1 + offset] & 0x0Fu;
        }

        Text_ShowASCII(kLock1Text);
        Text_SetPendingSFX(Audio_PlaySFX_Switch);
        return;
    }

    if ((uint8_t)can_index != wSecondLockTrashCanIndex) {

        ClearEvent(EVENT_1ST_LOCK_OPENED);
        wFirstLockTrashCanIndex = BattleRandom() & 0x0Eu;
        Text_ShowASCII(kFailText);
        Text_SetPendingSFX(Audio_PlaySFX_Denied);
        return;
    }

    SetEvent(EVENT_2ND_LOCK_OPENED);
    open_door();
    Text_ShowASCII(kLock2Text);
    Text_SetPendingSFX(Audio_PlaySFX_GoInside);
}

void VermilionGymScripts_OnMapLoad(void) {

    const char *n = AmberScript_MapBank_NameForRealId(wCurMap);

    if (!n || strcasecmp(n, "VermilionGym") != 0) return;

    if (CheckEvent(EVENT_2ND_LOCK_OPENED))
        open_door();
    else
        close_door();
}

void VermilionGymScripts_Tick(void) {
    const char *n = AmberScript_MapBank_NameForRealId(wCurMap);
    if (!n || strcasecmp(n, "VermilionGym") != 0) return;
    if (CheckEvent(EVENT_2ND_LOCK_OPENED)) open_door();
    else                                   close_door();
}

void VermilionGymScripts_GentlemanInteract(void) {
    if (CheckEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0)) {
        Text_ShowASCII(kGentlemanAfter);
        return;
    }
    GymScripts_SetTrainerPending(VG_GENTLEMAN_CLASS, VG_GENTLEMAN_NO,
                                  EVENT_BEAT_VERMILION_GYM_TRAINER_0,
                                  kGentlemanEnd, kGentlemanAfter, kGentlemanPre);
}

void VermilionGymScripts_RockerInteract(void) {
    if (CheckEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1)) {
        Text_ShowASCII(kRockerAfter);
        return;
    }
    GymScripts_SetTrainerPending(VG_ROCKER_CLASS, VG_ROCKER_NO,
                                  EVENT_BEAT_VERMILION_GYM_TRAINER_1,
                                  kRockerEnd, kRockerAfter, kRockerPre);
}

void VermilionGymScripts_SailorInteract(void) {
    if (CheckEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2)) {
        Text_ShowASCII(kSailorAfter);
        return;
    }
    GymScripts_SetTrainerPending(VG_SAILOR_CLASS, VG_SAILOR_NO,
                                  EVENT_BEAT_VERMILION_GYM_TRAINER_2,
                                  kSailorEnd, kSailorAfter, kSailorPre);
}

void VermilionGymScripts_Trash0(void)  { gym_trash_script(0);  }
void VermilionGymScripts_Trash1(void)  { gym_trash_script(1);  }
void VermilionGymScripts_Trash2(void)  { gym_trash_script(2);  }
void VermilionGymScripts_Trash3(void)  { gym_trash_script(3);  }
void VermilionGymScripts_Trash4(void)  { gym_trash_script(4);  }
void VermilionGymScripts_Trash5(void)  { gym_trash_script(5);  }
void VermilionGymScripts_Trash6(void)  { gym_trash_script(6);  }
void VermilionGymScripts_Trash7(void)  { gym_trash_script(7);  }
void VermilionGymScripts_Trash8(void)  { gym_trash_script(8);  }
void VermilionGymScripts_Trash9(void)  { gym_trash_script(9);  }
void VermilionGymScripts_Trash10(void) { gym_trash_script(10); }
void VermilionGymScripts_Trash11(void) { gym_trash_script(11); }
void VermilionGymScripts_Trash12(void) { gym_trash_script(12); }
void VermilionGymScripts_Trash13(void) { gym_trash_script(13); }
void VermilionGymScripts_Trash14(void) { gym_trash_script(14); }
