
#include "gen1color_scene.h"
#include "gen1color_battle.h"
#include "../battle/battle_ui.h"
#include "../text.h"
#include "../gbc_color.h"
#include "../constants.h"
#include "../overworld.h"
#include "../pokemon.h"
#include "../party_menu.h"
#include "../../platform/hardware.h"
#include "../../platform/display.h"
#include "../../data/base_stats.h"
#include "../../data/font_data.h"
#include <stdio.h>
#include "../gen2_species.h"

#define G1C_EXP_ROW        11
#define G1C_EXP_RIGHT_COL  17
#define G1C_EXP_TILES       8

static uint8_t T(unsigned ch) {
    return (uint8_t)Font_CharToTile((unsigned char)ch);
}

static void g1c_set_tile(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
    wTileMap[row * SCREEN_WIDTH + col] = tile;
}

static void g1c_clear(int col, int row, int w, int h) {
    for (int r = row; r < row + h; r++)
        for (int c = col; c < col + w; c++)
            g1c_set_tile(c, r, T(0x7F));
}

static uint8_t g1c_char_tile(char ch) {
    unsigned code;
    if      (ch >= 'A' && ch <= 'Z') code = 0x80u + (unsigned)(ch - 'A');
    else if (ch >= 'a' && ch <= 'z') code = 0xA0u + (unsigned)(ch - 'a');
    else if (ch >= '0' && ch <= '9') code = 0xF6u + (unsigned)(ch - '0');
    else if (ch == '.')              code = 0xE8u;
    else if (ch == '/')              code = 0xF3u;
    else                             code = 0x7Fu;
    return T(code);
}

static void g1c_put_str(int col, int row, const char *s, int max) {
    for (int i = 0; s && s[i] && i < max; i++)
        g1c_set_tile(col + i, row, g1c_char_tile(s[i]));
}

static int g1c_strlen(const char *s, int max) {
    int n = 0;
    while (s && s[n] && n < max) n++;
    return n;
}

static void g1c_put_level(int col, int row, int level) {
    char buf[8];
    g1c_set_tile(col, row, T(0x6E));
    snprintf(buf, sizeof(buf), "%-3d", level);
    g1c_put_str(col + 1, row, buf, 3);
}

static const char *g1c_status_name(uint8_t st) {
    if (st & STATUS_PSN)      return "PSN";
    if (st & STATUS_BRN)      return "BRN";
    if (st & STATUS_FRZ)      return "FRZ";
    if (st & STATUS_PAR)      return "PAR";
    if (st & STATUS_SLP_MASK) return "SLP";
    return NULL;
}

int G1CScene_HpPixels(int hp, int max_hp) {
    int px = 0;
    if (max_hp > 0) {
        long prod = (long)hp * 48;
        int div = max_hp;
        if (max_hp > 255) { prod >>= 2; div >>= 2; }
        px = (div > 0) ? (int)(prod / div) : 0;
        if (px < 0) px = 0;
        if (px > 48) px = 48;
        if (hp > 0 && px == 0) px = 1;
    }
    return px;
}

static void g1c_draw_hp_bar_px(int col, int row, int e, int bar_type) {

    if (col == 2 && row == 2) {
        static int last = -999;
        if (e != last) { last = e;
            printf("[DRAINDBG] g1c  enemy px=%d live=%u\n", e, (unsigned)wEnemyMon.hp);
            fflush(stdout); }
    }
    const int d = 6;

    g1c_set_tile(col,     row, T(0x71));
    g1c_set_tile(col + 1, row, T(0x62));
    for (int i = 0; i < d; i++)
        g1c_set_tile(col + 2 + i, row, T(0x63));
    g1c_set_tile(col + 2 + d, row, T(bar_type == 1 ? 0x6D : 0x6C));

    for (int i = 0; i < d && e > 0; i++) {
        int seg = (e >= 8) ? 8 : e;
        e -= seg;
        g1c_set_tile(col + 2 + i, row,
                     T(seg >= 8 ? 0x6B : (unsigned)(0x63 + seg)));
    }
}

static void g1c_draw_hp_bar(int col, int row, int side, int hp, int max_hp,
                            int bar_type) {
    int px, anim_hp, anim_max;
    if (BattleUI_HpBarAnim(side, &px, &anim_hp, &anim_max)) {
        (void)anim_hp; (void)anim_max;
        g1c_draw_hp_bar_px(col, row, px, bar_type);
        return;
    }
    g1c_draw_hp_bar_px(col, row, G1CScene_HpPixels(hp, max_hp), bar_type);
}

static void g1c_place_hud_tiles(int col, int row, int step,
                                unsigned first, unsigned corner, unsigned triangle) {
    g1c_set_tile(col, row, T(first));
    row += 1;
    g1c_set_tile(col, row, T(corner));
    for (int i = 0; i < 8; i++) {
        col += step;
        g1c_set_tile(col, row, T(0x76));
    }
    col += step;
    g1c_set_tile(col, row, T(triangle));
}

static void g1c_draw_exp_bar(void) {

    int b = Gen1Color_ExpBarPixelsShown();
    for (int i = 0; i < G1C_EXP_TILES; i++) {
        int c = (b >= 8) ? 8 : b;
        b -= c;
        g1c_set_tile(G1C_EXP_RIGHT_COL - i, G1C_EXP_ROW,
                     (uint8_t)(G1C_EXPBAR_SLOT_BASE + c));
    }
}

static int g1c_center_offset(int len) {
    return BattleUI_CenterMonNameOffset(len);
}

static void g1c_draw_enemy_hud(void) {
    g1c_clear(0, 0, 12, 4);

    g1c_set_tile(1, 2, T(0x72));
    if (wIsInBattle == 1) {
        int dex = gSpeciesToDex[wEnemyMon.species];
        if (dex > 0) {
            int bit = dex - 1;
            if ((wPokedexOwned[bit >> 3] >> (bit & 7)) & 1)
                g1c_set_tile(1, 1, T(0xE9));
        }
    }
    g1c_place_hud_tiles(1, 2, +1, 0x72, 0x74, 0x78);

    {
        const char *nm = Pokemon_GetName(Species_Dex(wEnemyMon.species));
        g1c_put_str(1 + g1c_center_offset(g1c_strlen(nm, 10)), 0, nm, 10);
    }
    {
        const char *st = g1c_status_name(wEnemyMon.status);
        if (st) g1c_put_str(7, 1, st, 3);
        else    g1c_put_level(6, 1, wEnemyMon.level);
    }

    g1c_draw_hp_bar(2, 2, 0, wEnemyMon.hp, wEnemyMon.max_hp, 0);
}

static void g1c_draw_player_hud(void) {
    g1c_clear(9, 7, 11, 5);
    g1c_place_hud_tiles(18, 10, -1, 0x73, 0x77, 0x6F);
    g1c_set_tile(18, 9, T(0x73));

    g1c_put_str(10, 7, PartyMenu_MonName(wPlayerMonNumber), 10);

    {
        const char *st = g1c_status_name(wBattleMon.status);
        if (st) g1c_put_str(15, 8, st, 3);
        else    g1c_put_level(14, 8, wBattleMon.level);
    }

    g1c_draw_hp_bar(10, 9, 1, wBattleMon.hp, wBattleMon.max_hp, 1);

    {
        int hp = wBattleMon.hp, max_hp = wBattleMon.max_hp;
        char buf[12];
        BattleUI_HpBarAnim(1, NULL, &hp, &max_hp);
        snprintf(buf, sizeof(buf), "%3d/%3d", hp, max_hp);
        g1c_put_str(11, 10, buf, 7);
    }

    g1c_draw_exp_bar();
}

static void g1c_draw_enemy_hud_gen1(void) {
    g1c_clear(0, 0, 12, 4);
    g1c_place_hud_tiles(1, 2, +1, 0x73, 0x74, 0x78);

    {
        const char *nm = Pokemon_GetName(Species_Dex(wEnemyMon.species));
        g1c_put_str(1 + g1c_center_offset(g1c_strlen(nm, 10)), 0, nm, 10);
    }
    {
        const char *st = g1c_status_name(wEnemyMon.status);
        if (st) g1c_put_str(5, 1, st, 3);
        else    g1c_put_level(4, 1, wEnemyMon.level);
    }
    g1c_draw_hp_bar(2, 2, 0, wEnemyMon.hp, wEnemyMon.max_hp, 0);
}

static void g1c_draw_player_hud_gen1(void) {
    g1c_clear(9, 7, 11, 5);
    g1c_place_hud_tiles(18, 10, -1, 0x73, 0x77, 0x6F);
    g1c_set_tile(18, 9, T(0x73));

    {
        const char *nm = PartyMenu_MonName(wPlayerMonNumber);
        g1c_put_str(10 + g1c_center_offset(g1c_strlen(nm, 10)), 7, nm, 10);
    }
    {
        const char *st = g1c_status_name(wBattleMon.status);
        if (st) g1c_put_str(15, 8, st, 3);
        else    g1c_put_level(14, 8, wBattleMon.level);
    }

    g1c_draw_hp_bar(10, 9, 1, wBattleMon.hp, wBattleMon.max_hp, 1);

    {
        int hp = wBattleMon.hp, max_hp = wBattleMon.max_hp;
        char buf[12];
        BattleUI_HpBarAnim(1, NULL, &hp, &max_hp);
        snprintf(buf, sizeof(buf), "%3d/%3d", hp, max_hp);
        g1c_put_str(11, 10, buf, 7);
    }

}

static void g1c_draw_pokeball_frames_gen1(void) {
    g1c_place_hud_tiles(18, 10, -1, 0x73, 0x77, 0x6F);
    if (wIsInBattle == 2)
        g1c_place_hud_tiles(1, 2, +1, 0x73, 0x74, 0x78);
}

static int s_enemy_hud_live;
static int s_player_hud_live;
static int s_text_seen;
static int s_main_loop_seen;

static int s_pal_black;

void G1CScene_ResetIntro(void) {
    s_enemy_hud_live = 0;
    s_player_hud_live = 0;
    s_text_seen = 0;
    s_main_loop_seen = 0;
    s_pal_black = 1;
}

int G1CScene_PaletteBlack(void) {
    return s_pal_black;
}

static int g1c_player_pic_on_screen(void) {
    for (int ty = 0; ty < 7; ty++)
        for (int tx = 0; tx < 7; tx++) {
            uint8_t t = wTileMap[(5 + ty) * SCREEN_WIDTH + (1 + tx)];
            if (t >= 53 && t < 53 + 49) return 1;
        }
    return 0;
}

int G1CScene_RedOnScreen(void) {
    const int last = 4 + 48;
    return wShadowOAM[last].y != 0 &&
           wShadowOAM[last].tile == (uint8_t)(53 + 48);
}

static int g1c_player_data_ready(void) {
    const party_mon_t *p = &wPartyMons[wPlayerMonNumber];
    if (wBattleMon.max_hp == 0) return 0;
    if (wBattleMon.hp > wBattleMon.max_hp) return 0;
    return wBattleMon.max_hp == p->max_hp;
}

#define G1C_BALL_OAM_BASE   102
#define G1C_BALL_OAM_COUNT   12

static void g1c_draw_pokeball_frames(void) {
    g1c_place_hud_tiles(18, 10, -1, 0x73, 0x75, 0x6F);
    if (wIsInBattle == 2)
        g1c_place_hud_tiles(1, 2, +1, 0x72, 0x74, 0x78);
}

static int g1c_pokeballs_up(void) {
    for (int i = 0; i < G1C_BALL_OAM_COUNT; i++) {
        int e = G1C_BALL_OAM_BASE + i;
        if (e < MAX_SPRITES && wShadowOAM[e].y != 0) return 1;
    }
    return 0;
}

static void g1c_update_pokeballs(void) {
    if (!s_main_loop_seen && BattleUI_IsAtActionMenu()) s_main_loop_seen = 1;

    for (int i = 0; i < G1C_BALL_OAM_COUNT; i++) {
        int e = G1C_BALL_OAM_BASE + i;
        if (e >= MAX_SPRITES) break;
        if (s_main_loop_seen) { wShadowOAM[e].y = 0; continue; }
        if (wShadowOAM[e].y == 0) continue;

        wShadowOAM[e].flags = (uint8_t)((wShadowOAM[e].flags & ~GBC_OBJ_PAL_MASK)
                                        | GBC_OBJ_PAL_POKEBALL);
    }
}

static void g1c_update_intro_latches(void) {
    if (Text_IsOpen()) s_text_seen = 1;

    if (s_pal_black && s_text_seen) s_pal_black = 0;

    if (s_enemy_hud_live && !BattleUI_EnemyMonOnField())
        s_enemy_hud_live = 0;

    if (!s_enemy_hud_live && BattleUI_EnemyMonOnField()) {
        if (wIsInBattle == 1) {

            if (s_text_seen) s_enemy_hud_live = 1;
        } else {

            s_enemy_hud_live = 1;
        }
    }

    if (!s_player_hud_live && BattleUI_PlayerMonIsOut() &&
        g1c_player_pic_on_screen() && g1c_player_data_ready())
        s_player_hud_live = 1;
}

int G1CScene_EnemyMonIsOut(void) {

    return (wIsInBattle == 1) ? 1 : s_enemy_hud_live;
}

void G1CScene_Draw(void) {
    g1c_update_intro_latches();
    g1c_update_pokeballs();

    if (BattleUI_HudOverlayActive()) return;

    if (!s_main_loop_seen && g1c_pokeballs_up()) g1c_draw_pokeball_frames();

    if (s_enemy_hud_live)  g1c_draw_enemy_hud();
    if (s_player_hud_live && g1c_player_data_ready()) g1c_draw_player_hud();
}

void G1CScene_DrawGen1(void) {
    g1c_update_intro_latches();
    g1c_update_pokeballs();

    if (BattleUI_HudOverlayActive()) return;

    if (!s_main_loop_seen && g1c_pokeballs_up()) g1c_draw_pokeball_frames_gen1();

    if (s_enemy_hud_live)  g1c_draw_enemy_hud_gen1();
    if (s_player_hud_live && g1c_player_data_ready()) g1c_draw_player_hud_gen1();
}
