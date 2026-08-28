
#include "tmhm.h"
#include "yesno.h"
#include "text.h"
#include "party_menu.h"
#include "overworld.h"
#include "npc.h"
#include "constants.h"
#include "../data/tmhm_data.h"
#include "../data/base_stats.h"
#include "../data/moves_data.h"
#include "../data/font_data.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "pokemon.h"
#include "inventory.h"
#include "rom_text.h"
#include "bag_menu.h"
#include "player.h"
#include "../platform/display.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "gen2_species.h"

static const uint8_t kHMMoves[5] = {
    MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH
};

static int is_hm_move(uint8_t move_id) {
    for (int i = 0; i < 5; i++)
        if (kHMMoves[i] == move_id) return 1;
    return 0;
}

typedef enum {
    TS_IDLE = 0,
    TS_BOOTED_WAIT,
    TS_CONFIRM_WAIT,
    TS_PICK_MON,
    TS_CANT_LEARN,
    TS_ALREADY_KNOWS,
    TS_TRY_FORGET_WAIT,
    TS_ABANDON_WAIT,
    TS_PICK_MOVE,
    TS_CANT_FORGET,
    TS_POOF_WAIT,
    TS_SUCCESS,
    TS_DECLINED,
} ts_t;

static ts_t   s_state       = TS_IDLE;
static uint8_t s_item_id    = 0;
static int     s_tmhm_idx   = 0;
static uint8_t s_move_id    = 0;
static int     s_party_slot = 0;
static int     s_move_cursor= 0;

static char    s_str[256];

static uint8_t s_saved_screen[SCROLL_MAP_W * SCROLL_MAP_H];
static int     s_screen_saved = 0;

static void save_screen(void) {
    memcpy(s_saved_screen, gScrollTileMap, sizeof s_saved_screen);
    s_screen_saved = 1;
}

static void restore_screen(void) {
    if (s_screen_saved)
        memcpy(gScrollTileMap, s_saved_screen, sizeof s_saved_screen);
}

static void hide_text_window(void) {
    hWY = SCREEN_HEIGHT_PX;
}

static void enter_move_picker(void) {
    s_move_cursor = 0;
    hide_text_window();
    s_state = TS_PICK_MOVE;
}

static int can_learn(int party_slot) {
    uint8_t species = wPartyMons[party_slot].base.species;
    uint8_t dex     = gSpeciesToDex[species];
    const uint8_t *bits = gBaseStats[dex].tmhm;
    return (bits[s_tmhm_idx >> 3] >> (s_tmhm_idx & 7)) & 1;
}

static int already_knows(int party_slot) {
    for (int i = 0; i < 4; i++)
        if (wPartyMons[party_slot].base.moves[i] == s_move_id) return 1;
    return 0;
}

static int find_empty_slot(int party_slot) {
    for (int i = 0; i < 4; i++)
        if (wPartyMons[party_slot].base.moves[i] == 0) return i;
    return -1;
}

static void install_move(int party_slot, int move_slot) {
    wPartyMons[party_slot].base.moves[move_slot] = s_move_id;
    wPartyMons[party_slot].base.pp[move_slot]    = gMoves[s_move_id].pp;
}

static const char *mon_name(int party_slot) {
    uint8_t species = wPartyMons[party_slot].base.species;
    return Pokemon_GetName(Species_Dex(species));
}

static const char *move_name(void) {
    if (s_move_id < NUM_MOVE_DEFS && gMoveNames[s_move_id])
        return gMoveNames[s_move_id];
    return "???";
}

static const char *move_name_of(uint8_t id) {
    if (id < NUM_MOVE_DEFS && gMoveNames[id]) return gMoveNames[id];
    return "???";
}

#define MOVE_BOX_ROW    7
#define MOVE_BOX_COL    4
#define MOVE_BOX_H      6
#define MOVE_BOX_W      16
#define MOVE_ROW0       8
#define MOVE_NAME_COL   6
#define MOVE_CURSOR_COL 5
#define MOVE_MENU_COLS  20

#define POKE_SPACE     0x7F
#define POKE_CORNER_TL 0x79
#define POKE_HORIZ     0x7A
#define POKE_CORNER_TR 0x7B
#define POKE_VERT      0x7C
#define POKE_CORNER_BL 0x7D
#define POKE_CORNER_BR 0x7E

static int poke_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    if (c == '-') return Font_CharToTile(0xE3);
    if (c == '.') return Font_CharToTile(0xE8);
    if (c == '!') return Font_CharToTile(0xE7);
    if (c == '?') return Font_CharToTile(0xE6);
    if (c == ' ') return Font_CharToTile(0x7F);
    return Font_CharToTile(0x7F);
}

#define SCROLL_MAP_STRIDE  SCROLL_MAP_W
#define TMIDX(r, c)  (((r) + 2) * SCROLL_MAP_STRIDE + ((c) + 2))

static void set_tile(int row, int col, int tile) {
    gScrollTileMap[TMIDX(row, col)] = (uint8_t)tile;
}

static void write_str(int row, int col, const char *s) {
    int c = col;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\f') { row++; c = col; continue; }
        if (c >= MOVE_MENU_COLS) continue;
        set_tile(row, c++, poke_char((unsigned char)s[i]));
    }
}

#define MSG_TEXT_ROW1 14
#define MSG_TEXT_ROW2 16
#define MSG_TEXT_COL   1

static void write_msg_box(const char *s) {
    int row = MSG_TEXT_ROW1, col = MSG_TEXT_COL;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\f') {
            row = (row == MSG_TEXT_ROW1) ? MSG_TEXT_ROW2 : MSG_TEXT_ROW1;
            col = MSG_TEXT_COL;
            continue;
        }
        if (col >= MOVE_MENU_COLS - 1) continue;
        set_tile(row, col++, poke_char((unsigned char)s[i]));
    }
}

static void draw_box_border(int row, int col, int h, int w) {
    set_tile(row, col, Font_CharToTile(POKE_CORNER_TL));
    for (int c = 1; c < w - 1; c++)
        set_tile(row, col + c, Font_CharToTile(POKE_HORIZ));
    set_tile(row, col + w - 1, Font_CharToTile(POKE_CORNER_TR));
    for (int r = 1; r < h - 1; r++) {
        set_tile(row + r, col, Font_CharToTile(POKE_VERT));
        for (int c = 1; c < w - 1; c++)
            set_tile(row + r, col + c, Font_CharToTile(POKE_SPACE));
        set_tile(row + r, col + w - 1, Font_CharToTile(POKE_VERT));
    }
    set_tile(row + h - 1, col, Font_CharToTile(POKE_CORNER_BL));
    for (int c = 1; c < w - 1; c++)
        set_tile(row + h - 1, col + c, Font_CharToTile(POKE_HORIZ));
    set_tile(row + h - 1, col + w - 1, Font_CharToTile(POKE_CORNER_BR));
}

static void draw_move_menu(void) {

    draw_box_border(12, 0, 6, SCREEN_WIDTH);
    write_msg_box(RomText("_WhichMoveToForgetText"));

    draw_box_border(MOVE_BOX_ROW, MOVE_BOX_COL, MOVE_BOX_H, MOVE_BOX_W);

    for (int i = 0; i < 4; i++) {
        int row = MOVE_ROW0 + i;
        uint8_t mid = wPartyMons[s_party_slot].base.moves[i];
        const char *mname = (mid > 0 && mid < NUM_MOVE_DEFS && gMoveNames[mid])
                            ? gMoveNames[mid] : "-";
        set_tile(row, MOVE_CURSOR_COL,
                 (i == s_move_cursor) ? Font_CharToTile(0xED)
                                      : Font_CharToTile(POKE_SPACE));
        write_str(row, MOVE_NAME_COL, mname);
    }
}

static void open_confirm(void) {

    char teach[192];
    RomTextSplice(teach, sizeof(teach), "_TeachMachineMoveText",
                  "{name}", move_name());

    snprintf(s_str, sizeof(s_str), "%s\f%s",
             RomText(s_item_id >= TM01 ? "_BootedUpTMText" : "_BootedUpHMText"),
             teach);
    YesNo_Show(s_str);
    s_state = TS_CONFIRM_WAIT;
}

static void validate_mon(int slot);

static void learned_message(int slot) {

    Audio_PlaySFX_GetItem1();
    RomTextSpliceN(s_str, sizeof(s_str), "_LearnedMove1Text",
                   "{ram:D036}", mon_name(slot),
                   "{name}",     move_name(), NULL);
    Text_ShowASCII(s_str);
    s_state = TS_SUCCESS;
}

extern void PartyMenu_DbgDumpIcons(const char *tag);

static void ask_trying_to_learn(int slot) {
    RomTextSpliceN(s_str, sizeof(s_str), "_TryingToLearnText",
                   "{ram:D036}", mon_name(slot),
                   "{name}",     move_name(), NULL);
    PartyMenu_DbgDumpIcons("pre-YesNo_Show");
    YesNo_Show(s_str);
    PartyMenu_DbgDumpIcons("post-YesNo_Show");
    s_state = TS_TRY_FORGET_WAIT;
}

static void ask_abandon(void) {
    RomTextSpliceN(s_str, sizeof(s_str), "_AbandonLearningText",
                   "{name}", move_name(), NULL);
    YesNo_Show(s_str);
    s_state = TS_ABANDON_WAIT;
}

static void reopen_party(void) {
    PartyMenu_Open(PARTY_MENU_TMHM);
    s_state = TS_PICK_MON;
}

static void party_backdrop_for_message(void) {
    PartyMenu_Open(PARTY_MENU_TMHM);

    PartyMenu_FinishOpenFade();
}

static void validate_mon(int slot) {
    s_party_slot = slot;
    if (!can_learn(slot)) {

        Audio_PlaySFX_Denied();
        RomTextSpliceN(s_str, sizeof(s_str), "_MonCannotLearnMachineMoveText",
                       "{badge}", mon_name(slot),
                       "{name}",  move_name(), NULL);

        party_backdrop_for_message();
        Text_ShowASCII(s_str);
        s_state = TS_CANT_LEARN;
        return;
    }
    if (already_knows(slot)) {

        RomTextSpliceN(s_str, sizeof(s_str), "_AlreadyKnowsText",
                       "{badge}", mon_name(slot),
                       "{name}",  move_name(), NULL);

        party_backdrop_for_message();
        Text_ShowASCII(s_str);
        s_state = TS_ALREADY_KNOWS;
        return;
    }
    int empty = find_empty_slot(slot);
    if (empty >= 0) {
        install_move(slot, empty);
        if (s_item_id >= TM01) Inventory_Remove(s_item_id, 1);
        learned_message(slot);
        return;
    }

    ask_trying_to_learn(slot);
}

void TMHM_Use(uint8_t item_id) {

    if (wIsInBattle) {

        Text_ShowASCII(RomText("_ItemUseNotTimeText"));
        s_state = TS_IDLE;
        return;
    }
    s_item_id   = item_id;
    s_tmhm_idx  = (item_id >= TM01) ? (item_id - TM01) : (item_id - HM01 + 50);
    s_move_id   = gTMHMMoves[s_tmhm_idx];
    s_move_cursor = 0;

    save_screen();

    YesNo_SetOverMenu(1);

    open_confirm();
}

void TMHM_BeginLevelUpLearn(int party_slot, uint8_t move_id) {
    if (party_slot < 0 || party_slot >= (int)wPartyCount) return;
    if (move_id == 0 || move_id >= NUM_MOVE_DEFS)         return;
    s_item_id     = 0;
    s_tmhm_idx    = 0;
    s_move_id     = move_id;
    s_party_slot  = party_slot;
    s_move_cursor = 0;
    save_screen();
    YesNo_SetOverMenu(1);
    ask_trying_to_learn(party_slot);
}

int TMHM_IsActive(void) { return s_state != TS_IDLE; }

int TMHM_ShowingMenuBackdrop(void) {
    switch (s_state) {
        case TS_BOOTED_WAIT:  case TS_CONFIRM_WAIT:
        case TS_TRY_FORGET_WAIT: case TS_ABANDON_WAIT:
        case TS_PICK_MOVE:    case TS_CANT_FORGET:
        case TS_POOF_WAIT:    case TS_SUCCESS:
        case TS_DECLINED:
            return 1;
        default:
            return 0;
    }
}

int TMHM_CanLearnActive(int party_slot) {
    if (s_state == TS_IDLE) return 0;
    if (party_slot < 0 || party_slot >= (int)wPartyCount) return 0;
    return can_learn(party_slot);
}

void TMHM_PostRender(void) {

    switch (s_state) {

        case TS_BOOTED_WAIT:
        case TS_CONFIRM_WAIT:
        case TS_TRY_FORGET_WAIT:
        case TS_ABANDON_WAIT:
        case TS_PICK_MOVE:
        case TS_CANT_FORGET:
        case TS_POOF_WAIT:
        case TS_SUCCESS:
        case TS_DECLINED:
            restore_screen();
            PartyMenu_DbgDumpIcons("postrender");
            break;
        default:
            break;
    }

    switch (s_state) {
        case TS_CONFIRM_WAIT:
        case TS_TRY_FORGET_WAIT:

        case TS_ABANDON_WAIT:
            YesNo_PostRender();
            break;
        case TS_PICK_MOVE:
        case TS_CANT_FORGET:
            draw_move_menu();
            break;
        default:
            break;
    }
}

void TMHM_Tick(void) {
    switch (s_state) {
        case TS_IDLE:
            break;

        case TS_BOOTED_WAIT:
            open_confirm();
            break;

        case TS_CONFIRM_WAIT:
            YesNo_Tick();
            if (!YesNo_IsOpen()) {
                if (YesNo_GetResult()) {
                    reopen_party();
                } else {

                    Text_CancelKeepTilesOnClose();
                    Text_Close();
                    YesNo_SetOverMenu(0);
                    s_state = TS_IDLE;
                    BagMenu_ReopenAfterUse();
                }
            }
            break;

        case TS_PICK_MON:
            PartyMenu_Tick();
            if (!PartyMenu_IsOpen()) {
                int slot = PartyMenu_GetSelected();
                if (slot < 0) {

                    YesNo_SetOverMenu(0);
                    s_state = TS_IDLE;
                    BagMenu_ReopenAfterUse();
                } else {

                    save_screen();

                    PartyMenu_RestoreIcons();
                    validate_mon(slot);
                }
            }
            break;

        case TS_CANT_LEARN:
        case TS_ALREADY_KNOWS:

            s_state = TS_PICK_MON;
            break;

        case TS_TRY_FORGET_WAIT:
            YesNo_Tick();
            if (!YesNo_IsOpen()) {
                if (YesNo_GetResult()) {
                    enter_move_picker();
                } else {
                    ask_abandon();
                }
            }
            break;

        case TS_ABANDON_WAIT:
            YesNo_Tick();
            if (!YesNo_IsOpen()) {
                if (YesNo_GetResult()) {

                    RomTextSpliceN(s_str, sizeof(s_str), "_DidNotLearnText",
                                   "{ram:D036}", mon_name(s_party_slot),
                                   "{name}",     move_name(), NULL);
                    Text_ShowASCII(s_str);
                    s_state = TS_DECLINED;
                } else {

                    ask_trying_to_learn(s_party_slot);
                }
            }
            break;

        case TS_PICK_MOVE:
            if (hJoyPressed & PAD_UP) {
                if (s_move_cursor > 0) s_move_cursor--;
            } else if (hJoyPressed & PAD_DOWN) {

                if (s_move_cursor < 3 &&
                    wPartyMons[s_party_slot].base.moves[s_move_cursor + 1] != 0)
                    s_move_cursor++;
            } else if (hJoyPressed & PAD_B) {

                ask_abandon();
            } else if (hJoyPressed & PAD_A) {
                uint8_t old_move = wPartyMons[s_party_slot].base.moves[s_move_cursor];
                if (is_hm_move(old_move)) {

                    Text_ShowASCII(RomText("_HMCantDeleteText"));
                    s_state = TS_CANT_FORGET;
                } else {
                    install_move(s_party_slot, s_move_cursor);
                    if (s_item_id >= TM01) Inventory_Remove(s_item_id, 1);

                    char poof[192];

                    Audio_PlaySFX_Swap();
                    RomTextSpliceN(poof, sizeof(poof), "PoofText",
                                   "{ram:D036}", mon_name(s_party_slot),
                                   "{badge}",    move_name_of(old_move), NULL);
                    snprintf(s_str, sizeof(s_str), "%s\f%s",
                             RomText("_OneTwoAndText"), poof);
                    Text_ShowASCII(s_str);
                    s_state = TS_POOF_WAIT;
                }
            }
            break;

        case TS_CANT_FORGET:

            enter_move_picker();
            break;

        case TS_POOF_WAIT:
            learned_message(s_party_slot);
            break;

        case TS_SUCCESS:
        case TS_DECLINED:

            PartyMenu_ClearIcons();
            YesNo_SetOverMenu(0);
            s_state = TS_IDLE;
            BagMenu_ReopenAfterUse();
            break;
    }
}
