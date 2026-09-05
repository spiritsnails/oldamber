#include "hall_of_fame_scripts.h"
#include "rom_text.h"
#include "credits_scripts.h"
#include "elite_four_scripts.h"
#include "constants.h"
#include "music.h"
#include "npc.h"
#include "player.h"
#include "pokemon.h"
#include "text.h"
#include "overworld.h"
#include "../data/base_stats.h"
#include "../data/font_data.h"
#include "../data/hof_player_sprites.h"
#include "../data/pokemon_sprites.h"
#include "mon_pic.h"
#include "gbc_color.h"
#include "anim.h"
#include "gen2_species.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include <stdio.h>
#include <string.h>
#include "../platform/debug_log.h"

#define MAP_HALL_OF_FAME 0x76

static uint8_t hof_cur_map(void) {
    int id = Map_CurrentRealId();
    return (id < 0) ? 0xFFu : (uint8_t)id;
}

typedef enum {
    HOF_IDLE = 0,
    HOF_ENTRY_WALK_WAIT,
    HOF_OAK_TEXT_WAIT,
    HOF_FADE_TO_WHITE_WAIT,
    HOF_POST_FADE_HOLD_WAIT,
    HOF_MON_PREP,
    HOF_MON_BACK_SCROLL,
    HOF_MON_FRONT_SCROLL,
    HOF_MON_INFO_WAIT,
    HOF_MON_HOFBOX_WAIT,
    HOF_MON_FADE_WAIT,
    HOF_PLAYER_PREP,
    HOF_PLAYER_BACK_SCROLL,
    HOF_PLAYER_FRONT_SCROLL,
    HOF_PLAYER_INFO_WAIT,
    HOF_PLAYER_RATING_TITLE_WAIT,
    HOF_PLAYER_RATING_WAIT,
    HOF_FINAL_FADE_WAIT,
    HOF_FINAL_WHITE_HOLD_WAIT,
} hof_state_t;

static hof_state_t g_state = HOF_IDLE;
static int g_started = 0;
static int g_fade_step = 0;
static int g_fade_timer = 0;
static int g_post_fade_hold_timer = 0;
static int g_hof_slot = -1;
static int g_hof_info_timer = 0;
static int g_hof_hofbox_timer = 0;
static uint8_t g_hof_species = 0;
static int g_hof_player_timer = 0;

#define HOF_TEXT_DELAY_TICKS 120
static int g_hof_text_delay = 0;

static int s_viewer_open = 0;
static int s_viewer_team_index = 0;
static int s_viewer_team_count = 0;
static int s_viewer_mon_index = 0;
static unsigned s_viewer_team_number = 0;

#define HOF_BG_PIC_BASE  0x20
#define HOF_PIC_COL      12
#define HOF_PIC_ROW      5

static const uint8_t kFadeOutToWhite[4][3] = {
    {0xE4, 0xD0, 0xE0},
    {0x28, 0x04, 0x20},
    {0x0A, 0x01, 0x08},
    {0x00, 0x00, 0x00},
};

static void win_put(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile;
}
static void bg_put(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static int ascii_to_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == ' ') return BLANK_TILE_SLOT;
    if (c == '/') return Font_CharToTile(0xF3);
    if (c == '-') return Font_CharToTile(0xE3);
    if (c == ':') return Font_CharToTile(0x9C);
    return BLANK_TILE_SLOT;
}

static void win_ascii(int col, int row, const char *s) {
    while (*s) {
        win_put(col++, row, (uint8_t)ascii_to_tile((unsigned char)*s));
        s++;
    }
}
static void bg_ascii(int col, int row, const char *s) {
    while (*s) {
        bg_put(col++, row, (uint8_t)ascii_to_tile((unsigned char)*s));
        s++;
    }
}
static void bg_poke(int col, int row, const uint8_t *s) {
    while (*s != 0x50) {
        bg_put(col++, row, (uint8_t)Font_CharToTile(*s));
        s++;
    }
}
static void bg_num(int col, int row, unsigned v, int digits) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", v);
    if (digits <= 0) {
        bg_ascii(col, row, buf);
        return;
    }
    {
        int len = (int)strlen(buf);
        int pad = digits - len;
        for (int i = 0; i < pad; i++) bg_put(col + i, row, BLANK_TILE_SLOT);
        bg_ascii(col + (pad > 0 ? pad : 0), row, buf);
    }
}
static unsigned count_bits_19(const uint8_t *bits) {
    unsigned n = 0;
    for (int i = 0; i < 19; i++) {
        uint8_t b = bits[i];
        for (int j = 0; j < 8; j++) n += (b >> j) & 1u;
    }
    if (n > 151) n = 151;
    return n;
}
static unsigned bcd_money_to_u32(const uint8_t bcd[3]) {
    return ((bcd[0] >> 4) & 0xF) * 100000u +
           (bcd[0] & 0xF) * 10000u +
           ((bcd[1] >> 4) & 0xF) * 1000u +
           (bcd[1] & 0xF) * 100u +
           ((bcd[2] >> 4) & 0xF) * 10u +
           (bcd[2] & 0xF);
}

static const char *dex_rating_text_for_owned(unsigned owned) {
    if (owned < 10)  return RomText("DexRatingText_Own0To9");
    if (owned < 20)  return RomText("DexRatingText_Own10To19");
    if (owned < 30)  return RomText("DexRatingText_Own20To29");
    if (owned < 40)  return RomText("DexRatingText_Own30To39");
    if (owned < 50)  return RomText("DexRatingText_Own40To49");
    if (owned < 60)  return RomText("DexRatingText_Own50To59");
    if (owned < 70)  return RomText("DexRatingText_Own60To69");
    if (owned < 80)  return RomText("DexRatingText_Own70To79");
    if (owned < 90)  return RomText("DexRatingText_Own80To89");
    if (owned < 100) return RomText("DexRatingText_Own90To99");
    if (owned < 110) return RomText("DexRatingText_Own100To109");
    if (owned < 120) return RomText("DexRatingText_Own110To119");
    if (owned < 130) return RomText("DexRatingText_Own120To129");
    if (owned < 140) return RomText("DexRatingText_Own130To139");
    if (owned < 150) return RomText("DexRatingText_Own140To149");
    return RomText("DexRatingText_Own150To151");
}

static void win_box(int x, int y, int b, int c) {
    win_put(x, y, Font_CharToTile(0x79));
    for (int i = 1; i <= c; i++) win_put(x + i, y, Font_CharToTile(0x7A));
    win_put(x + c + 1, y, Font_CharToTile(0x7B));
    for (int r = 1; r <= b; r++) {
        win_put(x, y + r, Font_CharToTile(0x7C));
        for (int i = 1; i <= c; i++) win_put(x + i, y + r, BLANK_TILE_SLOT);
        win_put(x + c + 1, y + r, Font_CharToTile(0x7C));
    }
    win_put(x, y + b + 1, Font_CharToTile(0x7D));
    for (int i = 1; i <= c; i++) win_put(x + i, y + b + 1, Font_CharToTile(0x7A));
    win_put(x + c + 1, y + b + 1, Font_CharToTile(0x7E));
}
static void bg_box(int x, int y, int b, int c) {
    bg_put(x, y, Font_CharToTile(0x79));
    for (int i = 1; i <= c; i++) bg_put(x + i, y, Font_CharToTile(0x7A));
    bg_put(x + c + 1, y, Font_CharToTile(0x7B));
    for (int r = 1; r <= b; r++) {
        bg_put(x, y + r, Font_CharToTile(0x7C));
        for (int i = 1; i <= c; i++) bg_put(x + i, y + r, BLANK_TILE_SLOT);
        bg_put(x + c + 1, y + r, Font_CharToTile(0x7C));
    }
    bg_put(x, y + b + 1, Font_CharToTile(0x7D));
    for (int i = 1; i <= c; i++) bg_put(x + i, y + b + 1, Font_CharToTile(0x7A));
    bg_put(x + c + 1, y + b + 1, Font_CharToTile(0x7E));
}

static const char *type_name(uint8_t type_id) {
    static const char *kNames[27] = {
        "NORMAL","FIGHTING","FLYING","POISON","GROUND","ROCK","BIRD","BUG","GHOST",
        "","","","","","","","","","","",
        "FIRE","WATER","GRASS","ELECTRIC","PSYCHIC","ICE","DRAGON"
    };
    if (type_id < 27) return kNames[type_id];
    return "";
}

static void win_nickname_or_species(int col, int row, int slot) {
    const uint8_t *nick = wPartyMonNicks[slot];
    if (nick[0] != 0 && nick[0] != 0x50) {
        for (int i = 0; i < 10; i++) {
            uint8_t ch = nick[i];
            if (ch == 0 || ch == 0x50) break;
            win_put(col + i, row, Font_CharToTile(ch));
        }
        return;
    }
    {
        uint8_t dex = Species_Dex(wPartyMons[slot].base.species);
        const char *name = Pokemon_GetName(dex);
        win_ascii(col, row, name);
    }
}
static void bg_nickname_or_species(int col, int row, int slot) {
    const uint8_t *nick = wPartyMonNicks[slot];
    if (nick[0] != 0 && nick[0] != 0x50) {
        for (int i = 0; i < 10; i++) {
            uint8_t ch = nick[i];
            if (ch == 0 || ch == 0x50) break;
            bg_put(col + i, row, Font_CharToTile(ch));
        }
        return;
    }
    {
        uint8_t dex = Species_Dex(wPartyMons[slot].base.species);
        const char *name = Pokemon_GetName(dex);
        bg_ascii(col, row, name);
    }
}

static void bg_hof_nickname_or_species(int col, int row,
                                       const hall_of_fame_mon_t *mon) {
    if (mon->nickname[0] != 0 && mon->nickname[0] != 0x50) {
        for (int i = 0; i < NAME_LENGTH; i++) {
            uint8_t ch = mon->nickname[i];
            if (ch == 0 || ch == 0x50) break;
            bg_put(col + i, row, Font_CharToTile(ch));
        }
        return;
    }
    bg_ascii(col, row, Pokemon_GetName(Species_Dex(mon->species)));
}

static void clear_bg_screen(void) {
    for (int r = 0; r < SCROLL_MAP_H; r++) {
        for (int c = 0; c < SCROLL_MAP_W; c++) {
            gScrollTileMap[r * SCROLL_MAP_W + c] = BLANK_TILE_SLOT;
        }
    }
}

static void hide_all_sprites(void) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        wShadowOAM[i].y = 0;
        wShadowOAM[i].x = 0;
        wShadowOAM[i].tile = 0;
        wShadowOAM[i].flags = 0;
    }
}

static void hof_apply_mon_palette(int dex) {

    GbcColor_SetPalPokemonWholeScreen(dex);
}

static void load_bg_pic_tiles(uint8_t dex, int front) {
    for (int i = 0; i < 49; i++) {
        Display_LoadTile((uint8_t)(HOF_BG_PIC_BASE + i),
                         front ? MonPic_FrontTile(dex, i) : MonPic_BackTile(dex, i));
    }
}

static void load_bg_player_pic_tiles(int front) {
    for (int i = 0; i < 49; i++) {
        if (front) Display_LoadTile((uint8_t)(HOF_BG_PIC_BASE + i), kHofRedFrontSprite[i]);
        else Display_LoadTile((uint8_t)(HOF_BG_PIC_BASE + i), kHofRedBackSprite[i]);
    }
}

static void stamp_bg_pic(void) {
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            gScrollTileMap[(HOF_PIC_ROW + ty + 2) * SCROLL_MAP_W + (HOF_PIC_COL + tx + 2) + Map_UiColOfs()] =
                (uint8_t)(HOF_BG_PIC_BASE + ty * 7 + tx);
        }
    }
}

static void draw_mon_info_box(int slot) {
    char lvl[4];
    const char *t1;
    const char *t2;
    snprintf(lvl, sizeof(lvl), "%02u", (unsigned)wPartyMons[slot].level);
    bg_box(0, 2, 9, 10);
    bg_nickname_or_species(1, 4, slot);
    bg_ascii(2, 6, "LEVEL/");
    bg_ascii(2, 7, "TYPE1/");
    bg_ascii(2, 9, "TYPE2/");
    bg_ascii(8, 6, lvl);
    t1 = type_name(wPartyMons[slot].base.type1);
    t2 = type_name(wPartyMons[slot].base.type2);

    bg_ascii(3, 8, t1);
    if (wPartyMons[slot].base.type2 != wPartyMons[slot].base.type1) {
        bg_ascii(3, 10, t2);
    } else {
        bg_ascii(3, 10, t1);
    }
}

static void draw_hof_record_info(const hall_of_fame_mon_t *mon,
                                 unsigned team_number) {
    uint8_t dex = Species_Dex(mon->species);
    const base_stats_t *stats = (dex && dex < gBaseStats_count)
                              ? &gBaseStats[dex] : NULL;
    char lvl[4];

    snprintf(lvl, sizeof(lvl), "%02u", (unsigned)mon->level);
    bg_box(0, 2, 9, 10);
    bg_hof_nickname_or_species(1, 4, mon);
    bg_ascii(2, 6, "LEVEL/");
    bg_ascii(2, 7, "TYPE1/");
    bg_ascii(2, 9, "TYPE2/");
    bg_ascii(8, 6, lvl);
    if (stats) {
        bg_ascii(3, 8, type_name(stats->type1));
        bg_ascii(3, 10, type_name(stats->type2));
    }

    bg_box(0, 13, 2, 18);
    bg_ascii(1, 15, "HALL OF FAME No");
    bg_num(16, 15, team_number, 3);
}

void HallOfFame_RecordParty(void) {
    hall_of_fame_team_t team;
    unsigned target;
    unsigned count = wPartyCount;

    if (count > PARTY_LENGTH) count = PARTY_LENGTH;
    memset(&team, 0, sizeof(team));
    for (unsigned i = 0; i < count; i++) {
        team.mons[i].species = wPartyMons[i].base.species;
        team.mons[i].level = wPartyMons[i].level;
        memcpy(team.mons[i].nickname, wPartyMonNicks[i], NAME_LENGTH);
    }
    if (count < PARTY_LENGTH)
        team.mons[count].species = 0xFF;

    if (wNumHoFTeams != 0xFF)
        wNumHoFTeams++;

    if ((unsigned)(wNumHoFTeams - 1u) < HOF_TEAM_CAPACITY) {
        target = (unsigned)(wNumHoFTeams - 1u);
    } else {
        memmove(&wHallOfFameTeams[0], &wHallOfFameTeams[1],
                sizeof(wHallOfFameTeams[0]) * (HOF_TEAM_CAPACITY - 1));
        target = HOF_TEAM_CAPACITY - 1;
    }
    wHallOfFameTeams[target] = team;
}

static void hall_of_fame_viewer_draw(void) {
    const hall_of_fame_mon_t *mon =
        &wHallOfFameTeams[s_viewer_team_index].mons[s_viewer_mon_index];
    uint8_t dex = Species_Dex(mon->species);

    Display_SetPalette(0xE4, 0xE4, 0xE4);
    hof_apply_mon_palette(dex);
    hWY = SCREEN_HEIGHT_PX;
    hide_all_sprites();
    clear_bg_screen();
    load_bg_pic_tiles(dex, 1);
    stamp_bg_pic();
    draw_hof_record_info(mon, s_viewer_team_number);
    Audio_PlayCry(mon->species);
}

void HallOfFameViewer_Open(void) {
    if (wNumHoFTeams == 0) return;
    s_viewer_team_count = wNumHoFTeams;
    if (s_viewer_team_count > HOF_TEAM_CAPACITY)
        s_viewer_team_count = HOF_TEAM_CAPACITY;
    s_viewer_team_index = 0;
    s_viewer_mon_index = 0;
    s_viewer_team_number = (wNumHoFTeams > HOF_TEAM_CAPACITY)
                         ? (unsigned)(wNumHoFTeams - HOF_TEAM_CAPACITY + 1u)
                         : 1u;
    s_viewer_open = 1;
    hall_of_fame_viewer_draw();
}

void HallOfFameViewer_Tick(void) {
    const hall_of_fame_team_t *team;

    if (!s_viewer_open) return;
    if (hJoyPressed & PAD_B) {
        s_viewer_open = 0;
        return;
    }
    if (!(hJoyPressed & PAD_A)) return;

    team = &wHallOfFameTeams[s_viewer_team_index];
    s_viewer_mon_index++;
    if (s_viewer_mon_index >= PARTY_LENGTH ||
        team->mons[s_viewer_mon_index].species == 0xFF) {
        s_viewer_team_index++;
        s_viewer_team_number++;
        s_viewer_mon_index = 0;
        if (s_viewer_team_index >= s_viewer_team_count) {
            s_viewer_open = 0;
            return;
        }
    }
    hall_of_fame_viewer_draw();
}

int HallOfFameViewer_IsOpen(void) {
    return s_viewer_open;
}

static void draw_hof_box(void) {
    bg_box(2, 13, 3, 14);
    bg_ascii(4, 15, "HALL OF FAME");
}

static void draw_player_stats_layout(void) {
    unsigned seen = count_bits_19(wPokedexSeen);
    unsigned owned = count_bits_19(wPokedexOwned);
    unsigned money = bcd_money_to_u32(wPlayerMoney);

    bg_box(5, 0, 2, 9);

    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t ch = wPlayerName[i];
        if (ch == 0 || ch == 0x50) break;
        bg_put(7 + i, 2, Font_CharToTile(ch));
    }

    bg_box(0, 4, 6, 10);
    bg_ascii(1, 6, "PLAY TIME");

    {
        extern unsigned long gPlayTimeFrames;
        unsigned long secs = gPlayTimeFrames / 60;
        int hrs  = (int)(secs / 3600); if (hrs > 999) hrs = 999;
        int mins = (int)((secs / 60) % 60);
        bg_num(5, 7, (unsigned)hrs, 3);
        bg_ascii(8, 7, ":");
        bg_put(9,  7, Font_CharToTile(0xF6 + (mins / 10)));
        bg_put(10, 7, Font_CharToTile(0xF6 + (mins % 10)));
    }
    bg_ascii(1, 9, "MONEY");
    bg_put(4, 10, Font_CharToTile(0xF0));
    bg_num(5, 10, money, 6);
    bg_box(0, 12, 4, 18);

    bg_ascii(1, 14, "POK");
    bg_put(4, 14, Font_CharToTile(0xBA));
    bg_ascii(5, 14, "DEX   Seen:");
    bg_num(16, 14, seen, 0);
    bg_ascii(10, 16, "Owned:");
    bg_num(16, 16, owned, 0);
}

static int find_first_party_slot(void) {
    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++) {
        if (wPartyMons[i].base.species != 0) return i;
    }
    return -1;
}

static int find_next_party_slot(int cur) {
    for (int i = cur + 1; i < wPartyCount && i < PARTY_LENGTH; i++) {
        if (wPartyMons[i].base.species != 0) return i;
    }
    return -1;
}

#define kHallOfFameOakText (RomText("HallOfFameOakText"))

static const int8_t kEntryMove[] = { 1, 1, 1, 1, 1 };

void HallOfFameScripts_OnMapLoad(void) {
    if (hof_cur_map() != MAP_HALL_OF_FAME) return;
    g_state = HOF_IDLE;
    g_started = 0;
    g_fade_step = 0;
    g_fade_timer = 0;
    g_post_fade_hold_timer = 0;
    g_hof_slot = -1;
    g_hof_info_timer = 0;
    g_hof_hofbox_timer = 0;
    g_hof_species = 0;
    g_hof_player_timer = 0;
    g_hof_text_delay = 0;
    gScrollPxX = 0;
    gScrollPxY = 0;
    hWY = SCREEN_HEIGHT_PX;
    hide_all_sprites();
    NPC_HideOAM();

    NPC_ReloadTiles();
}

void HallOfFameScripts_Tick(void) {

    {
        static int last_m = -99, last_s = -99;
        int m = (int)hof_cur_map();
        if (m != last_m || (int)g_state != last_s) {
            last_m = m; last_s = (int)g_state;
            DBG_PRINTF("[HOFDBG] tick curmap=%u real=%d want=%d started=%d state=%d\n",
                   (unsigned)wCurMap, m, MAP_HALL_OF_FAME, g_started, (int)g_state);
            fflush(stdout);
        }
    }
    if (hof_cur_map() != MAP_HALL_OF_FAME) return;

    if (!g_started && g_state == HOF_IDLE) {
        g_started = 1;

        Music_PlayCities1AlternateTempo();

        Anim_SetTileset(TILEANIM_NONE);
        hJoyPressed = 0;
        hJoyHeld = 0;
        Player_StartSimulatedMovement(kEntryMove, 4);
        g_state = HOF_ENTRY_WALK_WAIT;
        return;
    }

    switch (g_state) {
    case HOF_ENTRY_WALK_WAIT:
        if (!Player_IsSimulatingMovement() && !Player_IsMoving()) {
            gPlayerFacing = 3;
            NPC_SetFacing(0, 2);
            NPC_BuildView(gScrollPxX, gScrollPxY);
            wDoNotWaitForButtonPress = 1;
            Text_KeepTilesOnClose();
            Text_ShowASCII(kHallOfFameOakText);
            g_hof_text_delay = HOF_TEXT_DELAY_TICKS;
            g_state = HOF_OAK_TEXT_WAIT;
        }
        break;
    case HOF_OAK_TEXT_WAIT:
        if (!Text_IsOpen()) {
            if (g_hof_text_delay > 0) { g_hof_text_delay--; break; }
            g_fade_step = 0;
            g_fade_timer = 4;
            g_state = HOF_FADE_TO_WHITE_WAIT;
        }
        break;
    case HOF_FADE_TO_WHITE_WAIT:
        if (g_fade_step < 4) {
            Display_SetPalette(kFadeOutToWhite[g_fade_step][0],
                               kFadeOutToWhite[g_fade_step][1],
                               kFadeOutToWhite[g_fade_step][2]);
            if (--g_fade_timer <= 0) {
                g_fade_timer = 4;
                g_fade_step++;
            }
        } else {

            g_post_fade_hold_timer = 105;
            g_state = HOF_POST_FADE_HOLD_WAIT;
        }
        break;
    case HOF_POST_FADE_HOLD_WAIT:
        if (g_post_fade_hold_timer > 0) {
            g_post_fade_hold_timer--;
        } else {
            Music_Play(MUSIC_HALL_OF_FAME);
            g_hof_slot = find_first_party_slot();
            g_state = HOF_MON_PREP;
        }
        break;
    case HOF_MON_PREP:
        if (g_hof_slot < 0) {
            gScrollPxX = 0;
            gScrollPxY = 0;
            g_state = HOF_IDLE;
            break;
        }

        g_hof_species = Species_Dex(wPartyMons[g_hof_slot].base.species);
        Display_SetPalette(0xE4, 0xE4, 0xE4);
        hof_apply_mon_palette(g_hof_species);
        hWY = SCREEN_HEIGHT_PX;
        hide_all_sprites();
        clear_bg_screen();
        load_bg_pic_tiles(g_hof_species, 0);
        stamp_bg_pic();
        gScrollPxY = 48;
        gScrollPxX = 64;
        g_state = HOF_MON_BACK_SCROLL;
        break;
    case HOF_MON_BACK_SCROLL:
        hide_all_sprites();
        gScrollPxX -= 4;
        if (gScrollPxX <= -152) {
            clear_bg_screen();
            load_bg_pic_tiles(g_hof_species, 1);
            stamp_bg_pic();
            gScrollPxY = 0;
            gScrollPxX = -152;
            g_state = HOF_MON_FRONT_SCROLL;
        }
        break;
    case HOF_MON_FRONT_SCROLL:
        hide_all_sprites();
        gScrollPxX += 4;
        if (gScrollPxX >= 0) {
            gScrollPxX = 0;
            draw_mon_info_box(g_hof_slot);
            Audio_PlayCry(wPartyMons[g_hof_slot].base.species);
            g_hof_info_timer = 80;
            g_state = HOF_MON_INFO_WAIT;
        }
        break;
    case HOF_MON_INFO_WAIT:
        if (g_hof_info_timer > 0) g_hof_info_timer--;
        if (g_hof_info_timer <= 0) {
            draw_hof_box();
            g_hof_hofbox_timer = 180;
            g_state = HOF_MON_HOFBOX_WAIT;
        }
        break;
    case HOF_MON_HOFBOX_WAIT:
        if (g_hof_hofbox_timer > 0) g_hof_hofbox_timer--;
        if (g_hof_hofbox_timer <= 0) {
            g_fade_step = 0;
            g_fade_timer = 4;
            g_state = HOF_MON_FADE_WAIT;
        }
        break;
    case HOF_MON_FADE_WAIT:
        if (g_fade_step < 4) {
            Display_SetPalette(kFadeOutToWhite[g_fade_step][0],
                               kFadeOutToWhite[g_fade_step][1],
                               kFadeOutToWhite[g_fade_step][2]);
            if (--g_fade_timer <= 0) {
                g_fade_timer = 4;
                g_fade_step++;
            }
        } else {
            gScrollPxX = 0;
            gScrollPxY = 0;
            g_hof_slot = find_next_party_slot(g_hof_slot);
            if (g_hof_slot >= 0) {
                g_state = HOF_MON_PREP;
            } else {

                HallOfFame_RecordParty();
                g_state = HOF_PLAYER_PREP;
            }
        }
        break;
    case HOF_PLAYER_PREP:
        Display_SetPalette(0xE4, 0xE4, 0xE4);
        hWY = SCREEN_HEIGHT_PX;
        hide_all_sprites();
        clear_bg_screen();
        load_bg_player_pic_tiles(0);
        stamp_bg_pic();
        gScrollPxY = 48;
        gScrollPxX = 64;
        g_state = HOF_PLAYER_BACK_SCROLL;
        break;
    case HOF_PLAYER_BACK_SCROLL:
        hide_all_sprites();
        gScrollPxX -= 4;
        if (gScrollPxX <= -152) {
            clear_bg_screen();
            load_bg_player_pic_tiles(1);
            stamp_bg_pic();
            gScrollPxY = 0;
            gScrollPxX = -152;
            g_state = HOF_PLAYER_FRONT_SCROLL;
        }
        break;
    case HOF_PLAYER_FRONT_SCROLL:
        hide_all_sprites();
        gScrollPxX += 4;
        if (gScrollPxX >= 0) {
            gScrollPxX = 0;
            draw_player_stats_layout();
            g_hof_player_timer = 240;
            g_state = HOF_PLAYER_INFO_WAIT;
        }
        break;
    case HOF_PLAYER_INFO_WAIT:
        if (g_hof_player_timer > 0) g_hof_player_timer--;
        if (g_hof_player_timer <= 0) {

            {
                static char buf[24];
                RomTextSplice(buf, sizeof(buf), "_DexRatingText", "<COLON>", ":");
                wDoNotWaitForButtonPress = 1;
                Text_KeepTilesOnClose();
                Text_ShowASCII(buf);
            }
            g_hof_text_delay = HOF_TEXT_DELAY_TICKS;
            g_state = HOF_PLAYER_RATING_TITLE_WAIT;
        }
        break;
    case HOF_PLAYER_RATING_TITLE_WAIT:
        if (!Text_IsOpen()) {
            if (g_hof_text_delay > 0) { g_hof_text_delay--; break; }
            unsigned owned = count_bits_19(wPokedexOwned);
            wDoNotWaitForButtonPress = 1;
            Text_KeepTilesOnClose();
            Text_ShowASCII(dex_rating_text_for_owned(owned));
            g_hof_text_delay = HOF_TEXT_DELAY_TICKS;
            g_state = HOF_PLAYER_RATING_WAIT;
        }
        break;
    case HOF_PLAYER_RATING_WAIT:
        if (!Text_IsOpen()) {
            if (g_hof_text_delay > 0) { g_hof_text_delay--; break; }
            g_fade_step = 0;
            g_fade_timer = 4;
            g_state = HOF_FINAL_FADE_WAIT;
        }
        break;
    case HOF_FINAL_FADE_WAIT:
        if (g_fade_step < 4) {
            Display_SetPalette(kFadeOutToWhite[g_fade_step][0],
                               kFadeOutToWhite[g_fade_step][1],
                               kFadeOutToWhite[g_fade_step][2]);
            if (--g_fade_timer <= 0) {
                g_fade_timer = 4;
                g_fade_step++;
            }
        } else {

            g_post_fade_hold_timer = 105;
            g_state = HOF_FINAL_WHITE_HOLD_WAIT;
        }
        break;
    case HOF_FINAL_WHITE_HOLD_WAIT:
        if (g_post_fade_hold_timer > 0) {
            g_post_fade_hold_timer--;
        } else {

            EliteFourScripts_ResetRunFull();
            CreditsScripts_Start();
            g_state = HOF_IDLE;
        }
        break;
    default:
        break;
    }
}

int HallOfFameScripts_IsActive(void) {
    return (hof_cur_map() == MAP_HALL_OF_FAME) && (g_state != HOF_IDLE);
}

int HallOfFameScripts_ShouldUpdateOverworld(void) {
    if (hof_cur_map() != MAP_HALL_OF_FAME) return 0;
    return g_state <= HOF_OAK_TEXT_WAIT;
}

void HallOfFameScripts_OakInteract(void) {
    if (hof_cur_map() != MAP_HALL_OF_FAME) return;
    if (g_state == HOF_IDLE) {
        Text_ShowASCII(kHallOfFameOakText);
    }
}
