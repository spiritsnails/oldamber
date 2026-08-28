
#include "text.h"
#include "speed_settings.h"
#include "inventory.h"
#include "pokemon.h"
#include "../platform/hardware.h"
#include "overworld.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include "../game/constants.h"
#include "rival_starter.h"

#include <string.h>
#include <stdio.h>

extern int Game_GetScene(void) __attribute__((weak));

#define CHAR_TERM   0x50
#define CHAR_LINE   0x4E
#define CHAR_PARA   0x4F
#define CHAR_PAGE   0x51
#define CHAR_CONT   0x55

#define BOX_ROW     12
#define BOX_ROWS     6
#define TEXT_ROW1   14
#define TEXT_ROW2   16
#define TEXT_COL0    1
#define TEXT_COLS   18

#define CURSOR_CHAR 0xEE
#define BLANK_CHAR  0x7F

#define TEXT_LETTER_DELAY  3

int gTextLetterDelay = TEXT_LETTER_DELAY;

static const uint8_t *text_ptr   = NULL;
static const char    *ascii_ptr  = NULL;
static int            ascii_mode = 0;
static int            text_open  = 0;
static int            wait_input = 0;
static int            s_instant_next = 0;
static int            s_instant_active = 0;
static int            s_suppress_cursor_next = 0;
static int            s_suppress_cursor_active = 0;

int wDoNotWaitForButtonPress = 0;
static void (*s_pending_sfx)(void) = NULL;

static void (*s_pending_sfx_on_print)(void) = NULL;

static int s_hold_for_print_sfx = 0;

static int s_close_after_print_sfx = 0;

static int s_hold_after_prompt = 0;

void Text_SetPendingSFX(void (*fn)(void)) { s_pending_sfx = fn; }
void Text_SetPendingSFXOnPrint(void (*fn)(void)) { s_pending_sfx_on_print = fn; }
void Text_SetCloseAfterPrintSFX(void) { s_close_after_print_sfx = 1; }
void Text_HoldAfterPrompt(void) { s_hold_after_prompt = 1; }
int  Text_IsHeldAfterPrompt(void) { return text_open && wait_input == 7; }

static int s_print_sfx_due = 0;

static void fire_pending_sfx_on_print(void) {
    if (s_pending_sfx_on_print) {
        s_hold_for_print_sfx = 1;
        if (Audio_IsSFXPlaying()) {

            s_print_sfx_due = 1;
            return;
        }

        void (*fn)(void) = s_pending_sfx_on_print;
        s_pending_sfx_on_print = NULL;
        fn();
    }
}
static int            letter_timer = 0;

static int            cur_col    = TEXT_COL0;
static int            cur_row    = TEXT_ROW1;

static int            blink_timer1  = 0;
static int            blink_timer2  = 0;
static int            blink_on      = 0;
static int            scroll_timer  = 0;
static int            scroll_steps  = 0;
static int            s_keep_tiles_on_close = 0;

int Text_GetCurrentStr(char *buf, int size) {
    if (!text_open || !ascii_mode || !ascii_ptr || size <= 0) return 0;
    const char *p = ascii_ptr;
    int i = 0;
    while (*p && i < size - 1) {
        char c = *p++;
        if (c == '@') break;
        if (c == '\n' || c == '\f' || c == TEXT_ASCII_CONT) { if (i > 0 && buf[i-1] != ' ') buf[i++] = ' '; }
        else if ((unsigned char)c >= 0x20) buf[i++] = c;
    }
    buf[i] = '\0';
    return i > 0;
}

static int ascii_to_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c == ' ')              return BLANK_TILE_SLOT;
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == '!')              return Font_CharToTile(0xE7);
    if (c == '?')              return Font_CharToTile(0xE6);
    if (c == '.')              return Font_CharToTile(0xE8);
    if (c == ',')              return Font_CharToTile(0xF4);
    if (c == '-')              return Font_CharToTile(0xE3);
    if (c == '\'')             return Font_CharToTile(0xE0);
    if (c == ':')              return Font_CharToTile(0x9C);
    if (c == ';')              return Font_CharToTile(0x9D);
    if (c == '(')              return Font_CharToTile(0x9A);
    if (c == ')')              return Font_CharToTile(0x9B);
    if (c == '/')              return Font_CharToTile(0xF3);
    if (c == '!')              return Font_CharToTile(0xE7);
    if (c == 0xA5)             return Font_CharToTile(0xF0);
    if (c == 0xE9)             return Font_CharToTile(0xBA);

    if (c == 0xEF)             return Font_CharToTile(0xEF);
    if (c == 0xF5)             return Font_CharToTile(0xF5);
    return BLANK_TILE_SLOT;
}

static void set_tile(int col, int row, uint8_t tile_id) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile_id;
}

static uint8_t get_tile(int col, int row) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return 0;
    return gWindowTileMap[row][col];
}

static void show_cursor(void);

static int s_ends_with_prompt = 0;

void Text_SetEndsWithPrompt(void) { s_ends_with_prompt = 1; }

static void show_cursor_end(void) {

    if (s_ends_with_prompt && !wDoNotWaitForButtonPress) show_cursor();
}

static void show_cursor(void) {
    if (s_suppress_cursor_active) return;
    set_tile(SCREEN_WIDTH - 2, TEXT_ROW2, (uint8_t)Font_CharToTile(CURSOR_CHAR));

    blink_on     = 1;
    blink_timer1 = 8;
    blink_timer2 = 4;
}

static void hide_cursor(void) {
    set_tile(SCREEN_WIDTH - 2, TEXT_ROW2, (uint8_t)Font_CharToTile(BLANK_CHAR));
}

static void draw_box_border(void) {

    set_tile(0,              BOX_ROW, (uint8_t)Font_CharToTile(0x79));
    for (int c = 1; c < SCREEN_WIDTH - 1; c++)
        set_tile(c,          BOX_ROW, (uint8_t)Font_CharToTile(0x7A));
    set_tile(SCREEN_WIDTH-1, BOX_ROW, (uint8_t)Font_CharToTile(0x7B));

    for (int r = BOX_ROW + 1; r <= BOX_ROW + BOX_ROWS - 2; r++) {
        set_tile(0,               r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = 1; c < SCREEN_WIDTH - 1; c++)
            set_tile(c,           r, (uint8_t)Font_CharToTile(BLANK_CHAR));
        set_tile(SCREEN_WIDTH-1,  r, (uint8_t)Font_CharToTile(0x7C));
    }

    int bot = BOX_ROW + BOX_ROWS - 1;
    set_tile(0,              bot, (uint8_t)Font_CharToTile(0x7D));
    for (int c = 1; c < SCREEN_WIDTH - 1; c++)
        set_tile(c,          bot, (uint8_t)Font_CharToTile(0x7A));
    set_tile(SCREEN_WIDTH-1, bot, (uint8_t)Font_CharToTile(0x7E));
}

void Text_DrawEmptyBox(void) {
    draw_box_border();
}

static void clear_text_rows(void) {

    uint8_t blank = (uint8_t)Font_CharToTile(BLANK_CHAR);
    for (int c = TEXT_COL0; c < TEXT_COL0 + TEXT_COLS; c++) {
        set_tile(c, TEXT_ROW1, blank);
        set_tile(c, TEXT_ROW2, blank);
    }
}

static void scroll_up_once(void) {
    hide_cursor();
    for (int c = 0; c < SCREEN_WIDTH; c++)
        set_tile(c, TEXT_ROW1 - 1, get_tile(c, TEXT_ROW1));
    for (int c = 0; c < SCREEN_WIDTH; c++)
        set_tile(c, TEXT_ROW1, get_tile(c, TEXT_ROW1 + 1));
    for (int c = 0; c < SCREEN_WIDTH; c++)
        set_tile(c, TEXT_ROW1 + 1, get_tile(c, TEXT_ROW2));
    uint8_t blank = (uint8_t)Font_CharToTile(BLANK_CHAR);
    for (int c = TEXT_COL0; c < TEXT_COL0 + TEXT_COLS; c++)
        set_tile(c, TEXT_ROW2, blank);
}

static int print_poke_name(const uint8_t *name, int max_len, int col, int row) {
    for (int i = 0; i < max_len; i++) {
        uint8_t b = name[i];
        if (b == 0x00 || b == CHAR_TERM) break;
        if (col < TEXT_COL0 + TEXT_COLS) {
            set_tile(col++, row, (uint8_t)Font_CharToTile(b));
        }
    }
    return col;
}

static int print_ascii_literal(const char *s, int col, int row) {
    for (; *s; s++) {
        if (col < TEXT_COL0 + TEXT_COLS)
            set_tile(col++, row, (uint8_t)ascii_to_tile((unsigned char)*s));
    }
    return col;
}

static uint8_t resolve_player_starter_species(void) {
    uint8_t rival = RivalStarter_Get();
    if (rival == STARTER2) return STARTER1;
    if (rival == STARTER3) return STARTER2;
    return STARTER3;
}

static uint8_t s_name_enc[16] = { CHAR_TERM };

static char    s_mon_name[16];
static char    s_name_ascii[16];
static int     s_name_is_ascii;

void Text_SetItemName(uint8_t item_id) {
    const uint8_t *n = Inventory_GetName(item_id);
    size_t i = 0;
    s_name_is_ascii = 0;
    if (!n) { s_name_enc[0] = CHAR_TERM; return; }
    for (; i + 1 < sizeof s_name_enc && n[i] != CHAR_TERM; i++)
        s_name_enc[i] = n[i];
    s_name_enc[i] = CHAR_TERM;
}

void Text_SetMonName(uint8_t species) {
    const char *n = Pokemon_GetNameBySpecies(species);
    s_name_is_ascii = 1;
    snprintf(s_name_ascii, sizeof s_name_ascii, "%s", n ? n : "");
    snprintf(s_mon_name, sizeof s_mon_name, "%s", n ? n : "");
}

void Text_SetBoxNumber(int box) {
    s_name_is_ascii = 1;
    snprintf(s_name_ascii, sizeof s_name_ascii, "%d", box);
}

static uint8_t s_name2_enc[16] = { CHAR_TERM };
static char    s_name2_ascii[16];
static int     s_name2_is_ascii;

void Text_SetNameBufferItem(uint8_t item_id) {
    const uint8_t *n = Inventory_GetName(item_id);
    size_t i = 0;
    s_name2_is_ascii = 0;
    if (!n) { s_name2_enc[0] = CHAR_TERM; return; }
    for (; i + 1 < sizeof s_name2_enc && n[i] != CHAR_TERM; i++)
        s_name2_enc[i] = n[i];
    s_name2_enc[i] = CHAR_TERM;
}

void Text_SetNameBufferMon(uint8_t species) {
    const char *n = Pokemon_GetNameBySpecies(species);
    s_name2_is_ascii = 1;
    snprintf(s_name2_ascii, sizeof s_name2_ascii, "%s", n ? n : "");
}

void Text_SetNameBufferString(const char *s) {
    s_name2_is_ascii = 1;
    snprintf(s_name2_ascii, sizeof s_name2_ascii, "%s", s ? s : "");
}

static int print_one_char(void) {
    if (ascii_mode) {
        if (!ascii_ptr || !*ascii_ptr) {
            show_cursor_end(); wait_input = 2; fire_pending_sfx_on_print(); return 0;
        }
        char ac = *ascii_ptr;

        if (ac == '@') { show_cursor_end(); wait_input = 2; fire_pending_sfx_on_print(); return 0; }

        if (ac == '\n') {
            ascii_ptr++;
            if (cur_row == TEXT_ROW2) { show_cursor(); wait_input = 3; return 0; }
            cur_row = TEXT_ROW2; cur_col = TEXT_COL0;
            return 0;
        }
        if (ac == '\f') { ascii_ptr++; show_cursor(); wait_input = 1; return 0; }

        if (ac == TEXT_ASCII_CONT) {
            ascii_ptr++; show_cursor(); wait_input = 3; return 0;
        }

        if (ac == '#') {
            ascii_ptr++;
            if (cur_col < TEXT_COL0 + TEXT_COLS) set_tile(cur_col++, cur_row, (uint8_t)Font_CharToTile(0x8F));
            if (cur_col < TEXT_COL0 + TEXT_COLS) set_tile(cur_col++, cur_row, (uint8_t)Font_CharToTile(0x8E));
            if (cur_col < TEXT_COL0 + TEXT_COLS) set_tile(cur_col++, cur_row, (uint8_t)Font_CharToTile(0x8A));
            if (cur_col < TEXT_COL0 + TEXT_COLS) set_tile(cur_col++, cur_row, (uint8_t)Font_CharToTile(0xBA));
            return 1;
        }

        if (ac == '{') {
            if (strncmp(ascii_ptr, "{PLAYER}", 8) == 0) {
                ascii_ptr += 8;
                if (wPlayerName[0] && wPlayerName[0] != CHAR_TERM)
                    cur_col = print_poke_name(wPlayerName, NAME_LENGTH, cur_col, cur_row);
                else
                    cur_col = print_ascii_literal("RED", cur_col, cur_row);
                return 1;
            }
            if (strncmp(ascii_ptr, "{mon}", 5) == 0) {
                ascii_ptr += 5;
                if (s_mon_name[0])
                    cur_col = print_ascii_literal(s_mon_name, cur_col, cur_row);
                return 1;
            }
            if (strncmp(ascii_ptr, "{name}", 6) == 0) {
                ascii_ptr += 6;
                if (s_name_is_ascii) {
                    if (s_name_ascii[0])
                        cur_col = print_ascii_literal(s_name_ascii, cur_col, cur_row);
                } else if (s_name_enc[0] && s_name_enc[0] != CHAR_TERM) {
                    cur_col = print_poke_name(s_name_enc, (int)sizeof s_name_enc, cur_col, cur_row);
                }
                return 1;
            }

            if (strncmp(ascii_ptr, "{badge}", 7) == 0) {
                ascii_ptr += 7;
                if (s_name2_is_ascii) {
                    if (s_name2_ascii[0])
                        cur_col = print_ascii_literal(s_name2_ascii, cur_col, cur_row);
                } else if (s_name2_enc[0] && s_name2_enc[0] != CHAR_TERM) {
                    cur_col = print_poke_name(s_name2_enc, (int)sizeof s_name2_enc, cur_col, cur_row);
                }
                return 1;
            }
            if (strncmp(ascii_ptr, "{RIVAL}", 7) == 0) {
                ascii_ptr += 7;
                if (wRivalName[0] && wRivalName[0] != CHAR_TERM)
                    cur_col = print_poke_name(wRivalName, NAME_LENGTH, cur_col, cur_row);
                else
                    cur_col = print_ascii_literal("GARY", cur_col, cur_row);
                return 1;
            }
            if (strncmp(ascii_ptr, "{STARTER}", 9) == 0) {
                uint8_t starter = resolve_player_starter_species();
                ascii_ptr += 9;
                if (starter == STARTER1) cur_col = print_ascii_literal("CHARMANDER", cur_col, cur_row);
                else if (starter == STARTER2) cur_col = print_ascii_literal("SQUIRTLE", cur_col, cur_row);
                else cur_col = print_ascii_literal("BULBASAUR", cur_col, cur_row);
                return 1;
            }

            while (*ascii_ptr && *ascii_ptr != '}') ascii_ptr++;
            if (*ascii_ptr == '}') ascii_ptr++;
            return 0;
        }

        if (cur_col < TEXT_COL0 + TEXT_COLS)
            set_tile(cur_col++, cur_row, (uint8_t)ascii_to_tile((unsigned char)ac));
        ascii_ptr++;
        return 1;
    }

    if (!text_ptr || !*text_ptr) {
        show_cursor_end(); wait_input = 2; return 0;
    }
    uint8_t c = *text_ptr;

    if (c == CHAR_TERM) { show_cursor_end(); wait_input = 2; return 0; }

    if (c == CHAR_LINE) {
        text_ptr++;
        if (cur_row == TEXT_ROW2) { show_cursor(); wait_input = 3; return 0; }
        cur_row = TEXT_ROW2; cur_col = TEXT_COL0;
        return 0;
    }
    if (c == CHAR_PARA || c == CHAR_PAGE) {
        text_ptr++; show_cursor(); wait_input = 1; return 0;
    }
    if (c == CHAR_CONT) {
        text_ptr++; show_cursor(); wait_input = 3; return 0;
    }

    if (cur_col < TEXT_COL0 + TEXT_COLS)
        set_tile(cur_col++, cur_row, (uint8_t)Font_CharToTile(c));
    text_ptr++;
    return 1;
}

void Text_ShowBox(const uint8_t *str) {
    text_ptr     = str;
    ascii_ptr    = NULL;
    ascii_mode   = 0;
    text_open    = 1;
    wait_input   = 0;
    letter_timer = 0;
    s_instant_active = s_instant_next;
    s_instant_next = 0;
    s_suppress_cursor_active = s_suppress_cursor_next;
    s_suppress_cursor_next = 0;
    scroll_timer = 0;
    scroll_steps = 0;

    hWY = BOX_ROW * TILE_PX;
    draw_box_border();
    clear_text_rows();

    cur_col = TEXT_COL0; cur_row = TEXT_ROW1;
}

void Text_ShowASCII(const char *str) {

    while (str && *str == '\f') str++;
    ascii_ptr    = str;
    text_ptr     = NULL;
    ascii_mode   = 1;
    text_open    = 1;
    wait_input   = 0;
    letter_timer = 0;
    s_instant_active = s_instant_next;
    s_instant_next = 0;
    s_suppress_cursor_active = s_suppress_cursor_next;
    s_suppress_cursor_next = 0;
    scroll_timer = 0;
    scroll_steps = 0;

    hWY = BOX_ROW * TILE_PX;
    draw_box_border();
    clear_text_rows();

    cur_col = TEXT_COL0; cur_row = TEXT_ROW1;
}

int Text_IsOpen(void) {
    return text_open;
}

void Text_InstantNext(void) {
    s_instant_next = 1;
}

void Text_SuppressCursorNext(void) {
    s_suppress_cursor_next = 1;
}

void Text_Update(void) {
    if (!text_open) return;

    if (wait_input != 0) {

        if (wait_input == 7) return;

        if ((wait_input == 1 || wait_input == 3) && --blink_timer1 <= 0) {
            blink_timer1 = blink_on ? 8 : 4;
            if (--blink_timer2 <= 0) {
                blink_on = !blink_on;
                if (blink_on) { show_cursor(); blink_timer1 = 8; blink_timer2 = 4; }
                else          { hide_cursor(); blink_timer1 = 4; blink_timer2 = 2; }
            }
        }

        if (wait_input == 4) {
            if (!(hJoyHeld & PAD_A)) {

                if (s_hold_after_prompt) {
                    s_hold_after_prompt = 0;
                    hide_cursor();
                    wait_input = 7;
                    return;
                }
                Text_Close();
            }
            return;
        }

        if (wait_input == 5) {
            if (!(hJoyHeld & PAD_A)) {
                wait_input = 0;
                text_open  = 0;
                wDoNotWaitForButtonPress = 0;

                s_keep_tiles_on_close = 0;
            }
            return;
        }

        if (wait_input == 6) {

            if (SpeedSettings_RomTextScroll()) {
                if (scroll_timer > 0) {
                    scroll_timer--;
                    return;
                }
                if (scroll_steps > 0) {
                    scroll_up_once();
                    scroll_steps--;
                    scroll_timer = 5;
                    return;
                }
                wait_input   = 0;
                letter_timer = 0;
                cur_col = TEXT_COL0;
                cur_row = TEXT_ROW2;
                return;
            }

            if (scroll_timer > 0) {
                scroll_timer--;
                return;
            }
            scroll_up_once();
            if (--scroll_steps > 0) {
                scroll_timer = 5;
                return;
            }
            wait_input   = 0;
            letter_timer = 0;
            cur_col = TEXT_COL0;
            cur_row = TEXT_ROW2;
            return;
        }

        if (wait_input == 2 && s_hold_for_print_sfx) {

            if (s_print_sfx_due) {
                if (Audio_IsSFXPlaying()) return;
                s_print_sfx_due = 0;
                void (*fn) (void) = s_pending_sfx_on_print;
                s_pending_sfx_on_print = NULL;
                if (fn) fn();
                return;
            }
            if (Audio_IsSFXPlaying()) return;
            s_hold_for_print_sfx = 0;

            if (s_close_after_print_sfx) {
                s_close_after_print_sfx = 0;
                Text_Close();
                return;
            }
        }

        if (wait_input == 2 && wDoNotWaitForButtonPress) {
            wait_input = 5;
            return;
        }

        if (!(hJoyPressed & (PAD_A | PAD_B))) return;

        if (wait_input != 2 || s_ends_with_prompt) Audio_PlaySFX_PressAB();

        if (wait_input == 2) {

            if (s_ends_with_prompt) hide_cursor();
            wait_input = 4;
            return;
        }

        if (wait_input == 3) {

            scroll_steps = 2;

            scroll_timer = SpeedSettings_RomTextScroll() ? 0 : 5;
            wait_input = 6;
            return;
        }

        wait_input   = 0;
        letter_timer = 0;
        clear_text_rows();
        cur_col = TEXT_COL0; cur_row = TEXT_ROW1;
        return;
    }

    const int text_steps = SpeedSettings_Text();
    const int instant    = s_instant_active || (text_steps == SPEED_UNCAPPED);

    if (instant) {

        for (int guard = 0; guard < 512 && wait_input == 0; guard++)
            (void)print_one_char();
        letter_timer = 0;
        return;
    }

    for (int i = 0; i < text_steps; i++) {
        if (letter_timer > 0) {
            if (hJoyHeld & (PAD_A | PAD_B))
                letter_timer = 0;
            else
                { letter_timer--; continue; }
        }

        if (print_one_char())
            letter_timer = gTextLetterDelay;

        if (wait_input != 0)
            break;
    }

}

int Text_SnapshotBox(char *out, size_t outsz) {
    static char inv[256];
    static int  built = 0;
    if (!built) {
        for (unsigned c = 32; c < 127; c++) {
            int t = ascii_to_tile((unsigned char)c);
            if (t >= 0 && t < 256 && !inv[t]) inv[t] = (char)c;
        }
        built = 1;
    }
    if (!out || outsz == 0) return 0;

    size_t pos = 0;
    out[0] = '\0';
    for (int row = BOX_ROW; row < BOX_ROW + BOX_ROWS; row++) {
        char line[SCREEN_WIDTH + 1];
        int  len = 0;
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            uint8_t t = gWindowTileMap[row][col];
            char ch = inv[t];
            line[len++] = ch ? ch : ' ';
        }
        while (len > 0 && line[len - 1] == ' ') len--;
        int start = 0;
        while (start < len && line[start] == ' ') start++;
        if (start >= len) continue;
        if (pos && pos + 1 < outsz) out[pos++] = '|';
        for (int i = start; i < len && pos + 1 < outsz; i++) out[pos++] = line[i];
        out[pos] = '\0';
    }
    return (int)pos;
}

void Text_OverwriteTopLine(const char *s) {
    for (int c = TEXT_COL0; c < TEXT_COL0 + TEXT_COLS; c++)
        set_tile(c, TEXT_ROW1, (uint8_t)ascii_to_tile(' '));
    int col = TEXT_COL0;
    while (s && *s && col < TEXT_COL0 + TEXT_COLS)
        set_tile(col++, TEXT_ROW1, (uint8_t)ascii_to_tile((unsigned char)*s++));
}

void Text_KeepTilesOnClose(void) {
    s_keep_tiles_on_close = 1;
}

void Text_CancelKeepTilesOnClose(void) {
    s_keep_tiles_on_close = 0;
}

void Text_Close(void) {
    int keep_tiles = s_keep_tiles_on_close;

    s_hold_for_print_sfx = 0;
    s_print_sfx_due = 0;
    s_close_after_print_sfx = 0;
    s_hold_after_prompt = 0;
    text_open    = 0;
    wait_input   = 0;
    letter_timer = 0;
    scroll_timer = 0;
    scroll_steps = 0;
    text_ptr     = NULL;
    ascii_ptr    = NULL;
    ascii_mode   = 0;
    s_suppress_cursor_active = 0;
    s_ends_with_prompt = 0;

    if (!keep_tiles) {
        for (int r = BOX_ROW; r < BOX_ROW + BOX_ROWS; r++)
            for (int c = 0; c < SCREEN_WIDTH; c++)
                set_tile(c, r, 0);
        hWY = SCREEN_HEIGHT_PX;
    }

    s_keep_tiles_on_close = 0;

    if (s_pending_sfx) {
        void (*fn)(void) = s_pending_sfx;
        s_pending_sfx = NULL;
        fn();
    }
}

void Text_BlitBoxToBGAndHideWindow(void) {

    for (int r = BOX_ROW; r < BOX_ROW + BOX_ROWS; r++) {
        for (int c = 0; c < SCREEN_WIDTH; c++) {
            gScrollTileMap[(r + 2) * SCROLL_MAP_W + (c + 2) + Map_UiColOfs()] = gWindowTileMap[r][c];
        }
    }

    hWY = SCREEN_HEIGHT_PX;
}
