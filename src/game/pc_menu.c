#include "pc_menu.h"
#include "players_pc.h"

#include "constants.h"
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "pokemon.h"
#include "summary_screen.h"
#include "text.h"
#include "rom_text.h"
#include "dex_rating.h"
#include "hall_of_fame_scripts.h"
#include "yesno.h"
#include "../data/base_stats.h"
#include "../data/event_constants.h"
#include "../data/font_data.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "../platform/save.h"
#include "assetpack_bind.h"

#include <stdio.h>
#include <string.h>
#include "gen2_species.h"

typedef enum {
    PCM_CLOSED = 0,
    PCM_WAIT_BOOT_TEXT,
    PCM_TOP_MENU,
    PCM_WAIT_BRANCH_TEXT,
    PCM_BILLS_MENU,
    PCM_PARTY_LIST,
    PCM_BOX_LIST,
    PCM_LIST_SUBMENU,
    PCM_WAIT_RELEASE_TEXT,
    PCM_CHANGEBOX_CONFIRM,
    PCM_CHANGEBOX_INTRO,
    PCM_CHANGEBOX_PICKER,
    PCM_WAIT_RETURN_TOP,
    PCM_PLAYER_PC,

    PCM_OAK_CONFIRM,
    PCM_OAK_RATING,
    PCM_OAK_CLOSING,
    PCM_LEAGUE_VIEWER,
} pc_mode_t;

typedef enum {
    BRANCH_NONE = 0,
    BRANCH_BILLS,
    BRANCH_PLAYER,
    BRANCH_OAK,
    BRANCH_LEAGUE,
} pc_branch_t;

typedef enum {
    LIST_NONE = 0,
    LIST_DEPOSIT,
    LIST_WITHDRAW,
    LIST_RELEASE,
} pc_list_mode_t;

static pc_mode_t s_mode = PCM_CLOSED;
static pc_branch_t s_branch_after_text = BRANCH_NONE;
static pc_list_mode_t s_list_mode = LIST_NONE;
static int s_top_cursor = 0;
static int s_bills_cursor = 0;
static int s_list_cursor = 0;
static int s_list_scroll = 0;
static int s_sub_cursor = 0;
static int s_release_target = -1;
static int s_in_summary = 0;
static int s_box_picker_cursor = 0;
#define CHAR_SPACE 0x7F
#define CHAR_ARROW 0xED
#define CHAR_TERM  0x50
#define CHAR_PK    0xE1
#define CHAR_MN    0xE2
#define CHAR_POSSESSIVE_S 0xBD

static void pc_put(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile;
}

static int pc_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == ' ') return BLANK_TILE_SLOT;
    if (c == '.') return Font_CharToTile(0xE8);
    if (c == '!') return Font_CharToTile(0xE7);
    if (c == '?') return Font_CharToTile(0xE6);
    if (c == '\'') return Font_CharToTile(0xE0);
    if (c == '-') return Font_CharToTile(0xE3);
    if (c == '/') return Font_CharToTile(0xF3);
    if (c == ':') return Font_CharToTile(0x9C);
    return BLANK_TILE_SLOT;
}

static void pc_str(int col, int row, const char *s) {
    for (; *s; s++, col++)
        pc_put(col, row, (uint8_t)pc_ascii_tile((unsigned char)*s));
}

static void pc_str_encoded(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++)
        pc_put(col, row, (uint8_t)Font_CharToTile(*s));
}

static void pc_show_text(const char *s) {

    static char buf[256];
    snprintf(buf, sizeof buf, "%s", s ? s : "");
    Text_KeepTilesOnClose();
    Text_ShowASCII(buf);
    hWY = 0;
}

static void pc_show_yesno(const char *s) {
    YesNo_Show(s);
    hWY = 0;
}

static void pc_fill(int col, int row, int len, uint8_t tile) {
    for (int i = 0; i < len; i++)
        pc_put(col + i, row, tile);
}

static void pc_clear_overlay(void) {
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            gWindowTileMap[r][c] = 0;
}

static void pc_box(int l, int t, int r, int b) {
    pc_put(l, t, (uint8_t)Font_CharToTile(0x79));
    for (int c = l + 1; c < r; c++) pc_put(c, t, (uint8_t)Font_CharToTile(0x7A));
    pc_put(r, t, (uint8_t)Font_CharToTile(0x7B));
    for (int y = t + 1; y < b; y++) {
        pc_put(l, y, (uint8_t)Font_CharToTile(0x7C));
        for (int c = l + 1; c < r; c++) pc_put(c, y, BLANK_TILE_SLOT);
        pc_put(r, y, (uint8_t)Font_CharToTile(0x7C));
    }
    pc_put(l, b, (uint8_t)Font_CharToTile(0x7D));
    for (int c = l + 1; c < r; c++) pc_put(c, b, (uint8_t)Font_CharToTile(0x7A));
    pc_put(r, b, (uint8_t)Font_CharToTile(0x7E));
}

static void pc_textboxborder_from_asm(int hl_x, int hl_y, int b, int c) {

    pc_box(hl_x, hl_y, hl_x + c + 1, hl_y + b + 1);
}

static int pc_print_poke_name(int col, int row, const uint8_t *name) {
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t b = name[i];
        if (b == 0x00 || b == 0x50) return col;
        pc_put(col++, row, (uint8_t)Font_CharToTile(b));
    }
    return col;
}

static uint8_t s_pc_lv_tile_slot;
static uint8_t s_pc_ball_tile_slot;

static void pc_load_ui_tiles(void) {
    int base = Display_LoadedTileCount();
    if (base < 1) base = 1;
    if (base > BOX_TILE_BASE - 2) base = BOX_TILE_BASE - 2;
    s_pc_lv_tile_slot   = (uint8_t)base;
    s_pc_ball_tile_slot = (uint8_t)(base + 1);
    Display_LoadTile(s_pc_lv_tile_slot,   gHudTiles[0x6E - 0x62]);
    Display_LoadTile(s_pc_ball_tile_slot, gPokedexOwnedBallTile);
}

static void pc_restore_overworld(void) {
    s_mode = PCM_CLOSED;
    s_list_mode = LIST_NONE;
    s_in_summary = 0;
    s_release_target = -1;

    pc_clear_overlay();
    Display_LoadMapPalette();
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
    hWY = SCREEN_HEIGHT_PX;
}

static void pc_restore_background_under_menu(void) {
    Display_LoadMapPalette();
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
    hWY = 0;
}

static int pc_top_count(void) {
    if (wNumHoFTeams != 0) return 5;
    return CheckEvent(EVENT_GOT_POKEDEX) ? 4 : 3;
}

static void pc_draw_top_menu(void) {
    const int count = pc_top_count();
    static const uint8_t kBillsPc[] = {
        0x81,0x88,0x8B,0x8B, CHAR_POSSESSIVE_S, CHAR_SPACE, 0x8F,0x82, CHAR_TERM
    };
    static const uint8_t kSomeonesPc[] = {
        0x92,0x8E,0x8C,0x84,0x8E,0x8D,0x84, CHAR_POSSESSIVE_S, CHAR_SPACE, 0x8F,0x82, CHAR_TERM
    };
    static const uint8_t kPlayersPcSuffix[] = {
        CHAR_POSSESSIVE_S, CHAR_SPACE, 0x8F,0x82, CHAR_TERM
    };
    static const uint8_t kOaksPc[] = {
        0x8F,0x91,0x8E,0x85,0xE8,0x8E,0x80,0x8A, CHAR_POSSESSIVE_S, CHAR_SPACE, 0x8F,0x82, CHAR_TERM
    };
    static const uint8_t kLeaguePc[] = {
        CHAR_PK, CHAR_MN, 0x8B,0x84,0x80,0x86,0x94,0x84, CHAR_TERM
    };

    pc_clear_overlay();
    hWY = 0;
    pc_textboxborder_from_asm(0, 0, count == 5 ? 10 : (count == 4 ? 8 : 6), 14);
    if (CheckEvent(EVENT_MET_BILL))
        pc_str_encoded(2, 2, kBillsPc);
    else
        pc_str_encoded(2, 2, kSomeonesPc);
    pc_str_encoded(pc_print_poke_name(2, 4, wPlayerName), 4, kPlayersPcSuffix);
    if (CheckEvent(EVENT_GOT_POKEDEX)) {
        pc_str_encoded(2, 6, kOaksPc);
        if (wNumHoFTeams != 0) {
            pc_str_encoded(2, 8, kLeaguePc);
            pc_str(2, 10, "LOG OFF");
        } else {
            pc_str(2, 8, "LOG OFF");
        }
    } else {
        pc_str(2, 6, "LOG OFF");
    }

    for (int i = 0; i < count; i++) {
        int row = 2 + i * 2;
        pc_put(1, row, (uint8_t)Font_CharToTile(i == s_top_cursor ? CHAR_ARROW : CHAR_SPACE));
    }
}

static void pc_draw_bills_menu(void) {

    char box_num[12];
    int box_no = (wCurrentBoxNum % NUM_BOXES) + 1;
    static const uint8_t kWithdrawPkmn[] = {
        0x96,0x88,0x93,0x87,0x83,0x91,0x80,0x96, CHAR_SPACE, CHAR_PK, CHAR_MN, CHAR_TERM
    };
    static const uint8_t kDepositPkmn[] = {
        0x83,0x84,0x8F,0x8E,0x92,0x88,0x93, CHAR_SPACE, CHAR_PK, CHAR_MN, CHAR_TERM
    };
    static const uint8_t kReleasePkmn[] = {
        0x91,0x84,0x8B,0x84,0x80,0x92,0x84, CHAR_SPACE, CHAR_PK, CHAR_MN, CHAR_TERM
    };

    pc_clear_overlay();
    hWY = 0;
    pc_load_ui_tiles();
    pc_textboxborder_from_asm(0, 0, 10, 12);
    pc_str_encoded(2, 2, kWithdrawPkmn);
    pc_str_encoded(2, 4, kDepositPkmn);
    pc_str_encoded(2, 6, kReleasePkmn);
    pc_str(2, 8, "CHANGE BOX");
    pc_str(2, 10, "SEE YA!");
    for (int i = 0; i < 5; i++)
        pc_put(1, 2 + i * 2, (uint8_t)Font_CharToTile(i == s_bills_cursor ? CHAR_ARROW : CHAR_SPACE));

    pc_textboxborder_from_asm(0, 12, 4, 18);
    pc_str(1, 14, "What?");

    pc_textboxborder_from_asm(9, 14, 2, 9);
    snprintf(box_num, sizeof(box_num), "BOX No.%d", box_no);
    pc_str(10, 16, box_num);
}

static int pc_list_count(void) {
    switch (s_list_mode) {
    case LIST_DEPOSIT:  return wPartyCount;
    case LIST_WITHDRAW: return wBoxCount[wCurrentBoxNum % NUM_BOXES];
    case LIST_RELEASE:  return wBoxCount[wCurrentBoxNum % NUM_BOXES];
    default:            return 0;
    }
}

static const uint8_t *pc_list_name_ptr(int idx) {
    uint8_t box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (s_list_mode == LIST_DEPOSIT)
        return wPartyMonNicks[idx];
    return wBoxMonNicks[box][idx];
}

static uint8_t pc_list_species(int idx) {
    uint8_t box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (s_list_mode == LIST_DEPOSIT)
        return wPartyMons[idx].base.species;
    return wBoxMons[box][idx].species;
}

static uint8_t pc_list_level(int idx) {
    uint8_t box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (s_list_mode == LIST_DEPOSIT)
        return wPartyMons[idx].level;
    return wBoxMons[box][idx].box_level;
}

static int pc_list_total_entries(void) {
    return pc_list_count() + 1;
}

static void pc_draw_level(int col, int row, uint8_t level) {

    const uint8_t lv = s_pc_lv_tile_slot;

    if (level >= 100) {
        pc_put(col,   row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (level / 100))));
        pc_put(col+1, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + ((level / 10) % 10))));
        pc_put(col+2, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (level % 10))));
    } else if (level >= 10) {
        pc_put(col,   row, lv);
        pc_put(col+1, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (level / 10))));
        pc_put(col+2, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (level % 10))));
    } else {
        pc_put(col,   row, lv);
        pc_put(col+1, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + level)));
        pc_put(col+2, row, BLANK_TILE_SLOT);
    }
}

static void pc_clamp_list_scroll(void) {
    int total = pc_list_total_entries();
    if (total <= 0) {
        s_list_cursor = 0;
        s_list_scroll = 0;
        return;
    }
    if (s_list_cursor < 0) s_list_cursor = 0;
    if (s_list_cursor >= total) s_list_cursor = total - 1;
    if (s_list_scroll > s_list_cursor) s_list_scroll = s_list_cursor;
    if (s_list_cursor >= s_list_scroll + 4) s_list_scroll = s_list_cursor - 3;
    if (s_list_scroll < 0) s_list_scroll = 0;
}

static void pc_draw_list_screen(void) {
    int count = pc_list_count();

    pc_draw_bills_menu();
    pc_box(4, 2, 19, 12);
    for (int r = 3; r <= 11; r++)
        pc_fill(5, r, 14, BLANK_TILE_SLOT);

    for (int i = 0; i < 4; i++) {
        int idx = s_list_scroll + i;
        int row = 4 + i * 2;
        pc_fill(5, row, 14, BLANK_TILE_SLOT);
        pc_fill(5, row + 1, 14, BLANK_TILE_SLOT);
        if (idx > count) continue;
        pc_put(5, row, (uint8_t)Font_CharToTile(idx == s_list_cursor ? CHAR_ARROW : CHAR_SPACE));
        if (idx == count) {
            pc_str(6, row, "CANCEL");
            continue;
        }
        const uint8_t *nick = pc_list_name_ptr(idx);
        int name_end = pc_print_poke_name(6, row, nick);
        if (name_end == 6) {

            const char *species_name = Pokemon_GetName(Species_Dex(pc_list_species(idx)));
            if (species_name && *species_name)
                pc_str(6, row, species_name);
        }

        pc_draw_level(14, row + 1, pc_list_level(idx));
    }
}

static void pc_draw_list_submenu(void) {
    const char *action = (s_list_mode == LIST_DEPOSIT) ? "DEPOSIT" : "WITHDRAW";

    pc_textboxborder_from_asm(9, 10, 6, 9);
    pc_str(11, 12, action);
    pc_str(11, 14, "STATS");
    pc_str(11, 16, "CANCEL");
    pc_put(10, 12, (uint8_t)Font_CharToTile(s_sub_cursor == 0 ? CHAR_ARROW : CHAR_SPACE));
    pc_put(10, 14, (uint8_t)Font_CharToTile(s_sub_cursor == 1 ? CHAR_ARROW : CHAR_SPACE));
    pc_put(10, 16, (uint8_t)Font_CharToTile(s_sub_cursor == 2 ? CHAR_ARROW : CHAR_SPACE));
}

static void pc_draw_box_picker(void) {
    hWY = 0;
    pc_load_ui_tiles();

    pc_textboxborder_from_asm(0, 0, 2, 9);

    {
        char box_num[12];
        pc_fill(1, 1, 8, BLANK_TILE_SLOT);
        pc_fill(1, 2, 8, BLANK_TILE_SLOT);
        snprintf(box_num, sizeof(box_num), "BOX No.%d", (wCurrentBoxNum % NUM_BOXES) + 1);
        pc_str(1, 2, box_num);
    }

    pc_textboxborder_from_asm(11, 0, 12, 7);
    for (int i = 0; i < NUM_BOXES; i++) {
        int row = 1 + i;
        char label[8];
        snprintf(label, sizeof(label), "BOX%2d", i + 1);
        pc_put(12, row, (uint8_t)Font_CharToTile(i == s_box_picker_cursor ? CHAR_ARROW : CHAR_SPACE));
        pc_str(13, row, label);

        if (wBoxCount[i] > 0)
            pc_put(18, row, s_pc_ball_tile_slot);
    }
}

static void pc_open_branch_text(pc_branch_t branch) {
    s_branch_after_text = branch;
    if (branch == BRANCH_BILLS) {

        pc_show_text(CheckEvent(EVENT_MET_BILL)
                           ? RomText("_AccessedBillsPCText")
                           : RomText("_AccessedSomeonesPCText"));
    } else if (branch == BRANCH_PLAYER) {
        pc_show_text(RomText("_AccessedMyPCText"));
    } else if (branch == BRANCH_OAK) {

        pc_show_text(RomText("_AccessedOaksPCText"));
    } else if (branch == BRANCH_LEAGUE) {
        pc_show_text(RomText("_AccessedHoFPCText"));
    }
    s_mode = PCM_WAIT_BRANCH_TEXT;
}

static void pc_open_message_and_return(const char *text, pc_mode_t next_mode) {
    pc_show_text(text);
    s_mode = next_mode;
}

static void pc_finish_branch_text(void) {
    if (s_branch_after_text == BRANCH_BILLS) {
        s_mode = PCM_BILLS_MENU;
        pc_draw_bills_menu();
    } else if (s_branch_after_text == BRANCH_PLAYER) {

        s_mode = PCM_PLAYER_PC;
        PlayersPC_ActivateFromPCMenu();
    } else if (s_branch_after_text == BRANCH_OAK) {

        pc_show_yesno(RomText("_GetDexRatedText"));
        s_mode = PCM_OAK_CONFIRM;
    } else if (s_branch_after_text == BRANCH_LEAGUE) {
        HallOfFameViewer_Open();
        s_mode = PCM_LEAGUE_VIEWER;
    } else {
        s_mode = PCM_TOP_MENU;
        pc_draw_top_menu();
    }
}

static void pc_handle_deposit(void) {
    if (wPartyCount <= 1) {
        pc_open_message_and_return(RomText("_CantDepositLastMonText"), PCM_WAIT_RETURN_TOP);
        return;
    }
    if (wBoxCount[wCurrentBoxNum % NUM_BOXES] >= BOX_CAPACITY) {
        pc_open_message_and_return(RomText("_BoxFullText"), PCM_WAIT_RETURN_TOP);
        return;
    }
    s_list_mode = LIST_DEPOSIT;
    s_list_cursor = 0;
    s_list_scroll = 0;
    s_mode = PCM_PARTY_LIST;
    pc_draw_list_screen();
}

static void pc_handle_withdraw(void) {
    if (wBoxCount[wCurrentBoxNum % NUM_BOXES] == 0) {
        pc_open_message_and_return(RomText("_NoMonText"), PCM_WAIT_RETURN_TOP);
        return;
    }
    if (wPartyCount >= PARTY_LENGTH) {
        pc_open_message_and_return(RomText("_CantTakeMonText"), PCM_WAIT_RETURN_TOP);
        return;
    }
    s_list_mode = LIST_WITHDRAW;
    s_list_cursor = 0;
    s_list_scroll = 0;
    s_mode = PCM_BOX_LIST;
    pc_draw_list_screen();
}

static void pc_handle_release(void) {
    if (wBoxCount[wCurrentBoxNum % NUM_BOXES] == 0) {
        pc_open_message_and_return(RomText("_NoMonText"), PCM_WAIT_RETURN_TOP);
        return;
    }
    s_list_mode = LIST_RELEASE;
    s_list_cursor = 0;
    s_list_scroll = 0;
    s_mode = PCM_BOX_LIST;
    pc_draw_list_screen();
}

static void pc_handle_change_box(void) {

    pc_show_yesno(RomText("_WhenYouChangeBoxText"));
    s_mode = PCM_CHANGEBOX_CONFIRM;
}

void PCMenu_Activate(void) {
    if (s_mode != PCM_CLOSED) return;
    pc_clear_overlay();
    hWY = SCREEN_HEIGHT_PX;

    Audio_PlaySFX_TurnOnPC();
    pc_show_text(RomText("_TurnedOnPC1Text"));
    s_top_cursor = 0;
    s_bills_cursor = 0;
    s_mode = PCM_WAIT_BOOT_TEXT;
}

int PCMenu_IsOpen(void) {
    return s_mode != PCM_CLOSED;
}

void PCMenu_Tick(void) {
    int count;

    if (s_mode == PCM_CLOSED)
        return;

    if (s_mode == PCM_PLAYER_PC) {
        PlayersPC_Tick();
        if (!PlayersPC_IsOpen()) {
            s_mode = PCM_TOP_MENU;
            pc_draw_top_menu();
        }
        return;
    }

    if (s_mode == PCM_LEAGUE_VIEWER) {
        HallOfFameViewer_Tick();
        if (!HallOfFameViewer_IsOpen()) {
            pc_restore_background_under_menu();
            s_mode = PCM_TOP_MENU;
            pc_draw_top_menu();
        }
        return;
    }

    if (s_in_summary) {
        SummaryScreen_Tick();
        if (!SummaryScreen_IsOpen()) {
            s_in_summary = 0;
            NPC_BuildView(gScrollPxX, gScrollPxY);
            if (s_mode == PCM_PARTY_LIST || s_mode == PCM_BOX_LIST)
                pc_draw_list_screen();
            if (s_mode == PCM_LIST_SUBMENU) {
                pc_draw_list_screen();
                pc_draw_list_submenu();
            }
        }
        return;
    }

    if (YesNo_IsOpen()) {
        YesNo_Tick();
        if (!YesNo_IsOpen()) {
            if (s_mode == PCM_CHANGEBOX_CONFIRM) {
                if (YesNo_GetResult()) {
                    s_box_picker_cursor = wCurrentBoxNum % NUM_BOXES;

                    wDoNotWaitForButtonPress = 1;
                    pc_show_text(RomText("_ChooseABoxText"));
                    s_mode = PCM_CHANGEBOX_INTRO;
                } else {
                    s_mode = PCM_BILLS_MENU;
                    pc_draw_bills_menu();
                }
                return;
            }
            if (s_mode == PCM_OAK_CONFIRM) {
                if (YesNo_GetResult()) {

                    Text_SetPendingSFX(DexRating_PlaySfx);
                    pc_show_text(DexRating_Text());
                    s_mode = PCM_OAK_RATING;
                } else {

                    pc_show_text(RomText("_ClosedOaksPCText"));
                    s_mode = PCM_OAK_CLOSING;
                }
                return;
            }
            if (YesNo_GetResult() && s_release_target >= 0) {
                Audio_PlayCry(pc_list_species(s_release_target));

                Text_SetMonName(pc_list_species(s_release_target));
                Pokemon_ReleaseBoxMon(s_release_target);
                s_release_target = -1;
                pc_show_text(RomText("_MonWasReleasedText"));
                s_mode = PCM_WAIT_RELEASE_TEXT;
            } else {
                s_release_target = -1;
                s_mode = PCM_BOX_LIST;
                pc_clamp_list_scroll();
                pc_draw_list_screen();
            }
        }
        return;
    }

    switch (s_mode) {
    case PCM_WAIT_BOOT_TEXT:
        s_mode = PCM_TOP_MENU;
        pc_draw_top_menu();
        return;

    case PCM_WAIT_BRANCH_TEXT:
        pc_finish_branch_text();
        return;

    case PCM_OAK_RATING:

        pc_show_text(RomText("_ClosedOaksPCText"));
        s_mode = PCM_OAK_CLOSING;
        return;

    case PCM_OAK_CLOSING:

        s_mode = PCM_TOP_MENU;
        pc_draw_top_menu();
        return;

    case PCM_WAIT_RELEASE_TEXT:
    case PCM_WAIT_RETURN_TOP:
        s_mode = PCM_BILLS_MENU;
        pc_draw_bills_menu();
        return;

    case PCM_CHANGEBOX_INTRO:

        s_mode = PCM_CHANGEBOX_PICKER;
        pc_draw_box_picker();
        return;

    case PCM_CHANGEBOX_PICKER:
        if (hJoyPressed & PAD_UP) {
            s_box_picker_cursor = (s_box_picker_cursor + NUM_BOXES - 1) % NUM_BOXES;
            pc_draw_box_picker();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            s_box_picker_cursor = (s_box_picker_cursor + 1) % NUM_BOXES;
            pc_draw_box_picker();
            return;
        }
        if (hJoyPressed & PAD_B) {
            s_mode = PCM_BILLS_MENU;
            pc_draw_bills_menu();
            return;
        }
        if (hJoyPressed & PAD_A) {

            wCurrentBoxNum = (uint8_t)s_box_picker_cursor;
            Audio_PlaySFX_Save();
            Save_Write();
            s_mode = PCM_BILLS_MENU;
            pc_draw_bills_menu();
            return;
        }
        return;

    case PCM_TOP_MENU:
        count = pc_top_count();
        if (hJoyPressed & PAD_UP) {
            s_top_cursor = (s_top_cursor + count - 1) % count;
            pc_draw_top_menu();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            s_top_cursor = (s_top_cursor + 1) % count;
            pc_draw_top_menu();
            return;
        }
        if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_TurnOffPC();
            pc_restore_overworld();
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (s_top_cursor == 0) {
                Audio_PlaySFX_EnterPC();
                pc_open_branch_text(BRANCH_BILLS);
            }
            else if (s_top_cursor == 1) {
                Audio_PlaySFX_EnterPC();

                pc_open_branch_text(BRANCH_PLAYER);
            } else if (CheckEvent(EVENT_GOT_POKEDEX) && s_top_cursor == 2) {
                Audio_PlaySFX_EnterPC();

                pc_open_branch_text(BRANCH_OAK);
            } else if (wNumHoFTeams != 0 && s_top_cursor == 3) {
                Audio_PlaySFX_EnterPC();
                pc_open_branch_text(BRANCH_LEAGUE);
            }
            else {
                Audio_PlaySFX_TurnOffPC();
                pc_restore_overworld();
            }
            return;
        }
        return;

    case PCM_BILLS_MENU:
        if (hJoyPressed & PAD_UP) {
            s_bills_cursor = (s_bills_cursor + 4) % 5;
            pc_draw_bills_menu();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            s_bills_cursor = (s_bills_cursor + 1) % 5;
            pc_draw_bills_menu();
            return;
        }
        if (hJoyPressed & PAD_B) {
            s_mode = PCM_TOP_MENU;
            pc_draw_top_menu();
            return;
        }
        if (hJoyPressed & PAD_A) {
            switch (s_bills_cursor) {
            case 0: pc_handle_withdraw(); break;
            case 1: pc_handle_deposit(); break;
            case 2: pc_handle_release(); break;
            case 3: pc_handle_change_box(); break;
            default: s_mode = PCM_TOP_MENU; pc_draw_top_menu(); break;
            }
            return;
        }
        return;

    case PCM_PARTY_LIST:
    case PCM_BOX_LIST:
        count = pc_list_total_entries();
        if (count <= 0) {
            s_mode = PCM_BILLS_MENU;
            pc_draw_bills_menu();
            return;
        }
        if (hJoyPressed & PAD_UP) {
            s_list_cursor = (s_list_cursor + count - 1) % count;
            pc_clamp_list_scroll();
            pc_draw_list_screen();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            s_list_cursor = (s_list_cursor + 1) % count;
            pc_clamp_list_scroll();
            pc_draw_list_screen();
            return;
        }
        if (hJoyPressed & PAD_B) {
            s_mode = PCM_BILLS_MENU;
            pc_draw_bills_menu();
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (s_list_cursor >= pc_list_count()) {
                s_mode = PCM_BILLS_MENU;
                pc_draw_bills_menu();
                return;
            }
            if (s_list_mode == LIST_DEPOSIT || s_list_mode == LIST_WITHDRAW) {
                s_sub_cursor = 0;
                s_mode = PCM_LIST_SUBMENU;
                pc_draw_list_screen();
                pc_draw_list_submenu();
                return;
            }
            if (s_list_mode == LIST_RELEASE) {
                s_release_target = s_list_cursor;
                Text_SetMonName(pc_list_species(s_list_cursor));
                pc_show_yesno(RomText("_OnceReleasedText"));
                return;
            }
        }
        return;

    case PCM_LIST_SUBMENU:
        if (hJoyPressed & PAD_UP) {
            s_sub_cursor = (s_sub_cursor + 2) % 3;
            pc_draw_list_screen();
            pc_draw_list_submenu();
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            s_sub_cursor = (s_sub_cursor + 1) % 3;
            pc_draw_list_screen();
            pc_draw_list_submenu();
            return;
        }
        if (hJoyPressed & PAD_B) {
            s_mode = (s_list_mode == LIST_DEPOSIT) ? PCM_PARTY_LIST : PCM_BOX_LIST;
            pc_draw_list_screen();
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (s_sub_cursor == 0) {
                if (s_list_mode == LIST_DEPOSIT) {

                    uint8_t sp = pc_list_species(s_list_cursor);
                    if (Pokemon_DepositPartyMonToBox(s_list_cursor)) {
                        char msg[64];
                        char box_str[4];
                        Text_SetMonName(sp);
                        snprintf(box_str, sizeof(box_str), "%d", (wCurrentBoxNum % NUM_BOXES) + 1);
                        RomTextSplice(msg, sizeof(msg), "_MonWasStoredText", "{ram:CD3D}", box_str);
                        pc_show_text(msg);
                    } else
                        pc_show_text(RomText("_BoxFullText"));
                } else {
                    uint8_t sp = pc_list_species(s_list_cursor);
                    if (Pokemon_WithdrawBoxMonToParty(s_list_cursor)) {
                        Text_SetMonName(sp);
                        pc_show_text(RomText("_MonIsTakenOutText"));
                    } else
                        pc_show_text(RomText("_CantTakeMonText"));
                }
                s_mode = PCM_WAIT_RETURN_TOP;
                return;
            }
            if (s_sub_cursor == 1) {
                if (s_list_mode == LIST_DEPOSIT) {
                    s_in_summary = 1;
                    SummaryScreen_Open(s_list_cursor);
                } else {
                    pc_show_text("BOX STATS view not\nyet implemented.");
                    s_mode = PCM_WAIT_RETURN_TOP;
                }
                return;
            }
            s_mode = (s_list_mode == LIST_DEPOSIT) ? PCM_PARTY_LIST : PCM_BOX_LIST;
            pc_draw_list_screen();
            return;
        }
        return;

    default:
        return;
    }
}
