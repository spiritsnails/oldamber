
#include "viridian_mart_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "npc.h"
#include "pokemart.h"
#include "inventory.h"
#include "player.h"
#include "../data/event_constants.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"

#include <stdio.h>

#define MAP_VIRIDIAN_MART  0x2a

#define kYouCameFromPallet (RomText("ViridianMartClerkYouCameFromPalletTownText"))

#define kParcelQuestText (RomText("_ViridianMartClerkParcelQuestText"))

#define kSayHiToOak (RomText("ViridianMartClerkSayHiToOakText"))

typedef enum {
    VMS_IDLE = 0,

    VMS_ENTRY_TEXT,
    VMS_ENTRY_WALK,

    VMS_PARCEL_TEXT,
    VMS_PARCEL_GIVE,
    VMS_PARCEL_DONE,
} VirtMartState;

static VirtMartState gVMState = VMS_IDLE;

#define DIR_LEFT  2
#define DIR_UP    1

static const int kWalkDirs[] = { DIR_LEFT, DIR_UP, DIR_UP };
#define WALK_LEN 3
static int gWalkStep = 0;

int ViridianMartScripts_IsActive(void) {
    return gVMState != VMS_IDLE;
}

void ViridianMartScripts_OnMapLoad(void) {
    if (wCurMap != MAP_VIRIDIAN_MART) return;
    if (CheckEvent(EVENT_GOT_OAKS_PARCEL)) return;

    gVMState = VMS_ENTRY_TEXT;
    Text_ShowASCII(kYouCameFromPallet);
    printf("[viridian_mart] parcel script triggered on entry\n");
}

void ViridianMart_ClerkCallback(void) {

    if (gVMState != VMS_IDLE) return;

    if (CheckEvent(EVENT_OAK_GOT_PARCEL)) {

        ViridianMart_Start();
        return;
    }

    Text_ShowASCII(kSayHiToOak);
}

void ViridianMartScripts_Tick(void) {
    switch (gVMState) {
    case VMS_IDLE:
        return;

    case VMS_ENTRY_TEXT:
        if (Text_IsOpen()) return;

        gWalkStep = 0;
        gVMState = VMS_ENTRY_WALK;
        return;

    case VMS_ENTRY_WALK:
        if (Player_IsMoving()) return;
        if (gWalkStep >= WALK_LEN) {

            gPlayerFacing = 2;
            NPC_FacePlayer(0);
            NPC_BuildView(gScrollPxX, gScrollPxY);
            gVMState = VMS_PARCEL_TEXT;
            Text_ShowASCII(kParcelQuestText);
            return;
        }
        Player_DoScriptedStep(kWalkDirs[gWalkStep]);
        gWalkStep++;
        return;

    case VMS_PARCEL_TEXT:
        if (Text_IsOpen()) return;

        Inventory_Add(ITEM_OAKS_PARCEL, 1);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        gVMState = VMS_PARCEL_DONE;
        return;

    case VMS_PARCEL_DONE:
        gVMState = VMS_IDLE;
        return;

    }
}
