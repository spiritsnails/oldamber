
#include <SDL.h>

#ifdef main
#  undef main
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define suite_chdir _chdir
#define suite_mkdir _mkdir
#else
#include <unistd.h>
#include <sys/stat.h>
#define suite_chdir chdir
#define suite_mkdir(p) mkdir((p), 0755)
#endif

#include "data/dex_data.h"
#include "platform/hardware.h"
#include "platform/exe_dir.h"
#include "platform/data_dir.h"
#include "platform/input.h"
#include "platform/display.h"
#include "platform/audio.h"
#include "platform/save.h"
#include "platform/assetpack.h"
#include "platform/game_version.h"
#include "platform/launcher.h"
#include "game/pokedex.h"
#include "game/party_menu.h"
#include "game/trainer_card.h"
#include "game/town_map.h"
#include "game/credits_scripts.h"
#include "game/battle/battle_ui.h"
#include "assetpack_bind.h"
#include "game/constants.h"
#include "game/overworld.h"
#include "game/bag_list_choice.h"
#include "game/yesno.h"
#include "game/player.h"
#include "game/speed_settings.h"
#include "game/npc.h"
#include "game/debug_overlay.h"
#include "game/player.h"
#include "platform/input.h"
#include "platform/compiler.h"
#include "game/debug_cli.h"
#include "game/debug_suite.h"
#include "game/amberscript_core.h"
#include "game/amberscript_mapbank.h"
#include "game/breakpoint.h"
#include "game/presentation_menu.h"
#include "game/suspend_menu.h"
#include "game/intro.h"
#include "game/title_screen.h"
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#include "game/amberscript_saveops.h"
#include "data/map_data.h"

extern int gNoWilds;

#define REWIND_SLOTS          1800
#define REWIND_SNAPSHOT_EVERY 1
static int s_rw_seq[REWIND_SLOTS];
static int s_rw_len = 0;
static int s_rw_cursor = -1;
static int s_rw_next_slot = 0;
static uint32_t s_rw_frame_div = 0;
static uint8_t *s_rw_data = NULL;
static size_t s_rw_state_size = 0;

static int load_state_for_runtime_mem(const uint8_t *src) {
    if (!src) return 0;
    if (Save_StateLoadFromBuffer(src, s_rw_state_size) == 0) {
        extern void Map_Load(uint8_t map_id);
        extern void NPC_Load(void);
        extern void BattleUI_Restore(void);
        static int s_loaded_map = -1;
        if (Save_StateWasBattle()) {
            BattleUI_Restore();
            return 1;
        }

        if (s_loaded_map != (int)wCurMap) {
            Map_Load(wCurMap);
            NPC_Load();
            s_loaded_map = (int)wCurMap;
        } else {

            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
        }
        return 1;
    }
    return 0;
}

static int rewind_capture_snapshot(void) {
    int slot = s_rw_next_slot;
    uint8_t *dst;

    if (s_rw_cursor >= 0 && s_rw_cursor < s_rw_len - 1) {

        s_rw_len = s_rw_cursor + 1;
    }

    dst = s_rw_data + ((size_t)slot * s_rw_state_size);
    s_rw_next_slot = (s_rw_next_slot + 1) % REWIND_SLOTS;
    if (Save_StateCaptureToBuffer(dst, s_rw_state_size) != 0) return -1;

    if (s_rw_len < REWIND_SLOTS) {
        s_rw_seq[s_rw_len++] = slot;
    } else {
        memmove(&s_rw_seq[0], &s_rw_seq[1], sizeof(int) * (REWIND_SLOTS - 1));
        s_rw_seq[REWIND_SLOTS - 1] = slot;
        if (s_rw_cursor > 0) s_rw_cursor--;
    }
    s_rw_cursor = s_rw_len - 1;
    return slot;
}

static int rewind_step_older(void) {
    if (s_rw_len < 2 || s_rw_cursor <= 0) return 0;
    s_rw_cursor--;
    return load_state_for_runtime_mem(s_rw_data + ((size_t)s_rw_seq[s_rw_cursor] * s_rw_state_size));
}

static int rewind_step_newer(void) {
    if (s_rw_len < 2 || s_rw_cursor < 0 || s_rw_cursor >= s_rw_len - 1) return 0;
    s_rw_cursor++;
    return load_state_for_runtime_mem(s_rw_data + ((size_t)s_rw_seq[s_rw_cursor] * s_rw_state_size));
}

static void breakpoint_map_label(char *out, size_t n) {
    if (AmberScript_IsEnabled() &&
        wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {
        const char *nm = AmberScript_MapBank_NameForRealId(wCurMap);
        if (nm && *nm) { snprintf(out, n, "%s", nm); return; }
    }
    snprintf(out, n, "map%d", (int)wCurMap);
}

static void breakpoint_commit_now(void) {
    char label[40], name[80] = {0};
    if (s_rw_len < 1) { printf("[breakpoint] nothing to commit (ring empty)\n"); return; }
    breakpoint_map_label(label, sizeof(label));
    Breakpoint_Commit(label, s_rw_data, s_rw_seq, s_rw_len, s_rw_state_size,
                      (uint8_t)wCurMap, (int)wXCoord, (int)wYCoord,
                      name, sizeof(name));
}

static void breakpoint_restore_now(const char *name) {
    int n = Breakpoint_LoadBundle(name, s_rw_data, s_rw_seq, &s_rw_len,
                                  s_rw_state_size, REWIND_SLOTS);
    if (n < 1) return;
    s_rw_cursor    = 0;
    s_rw_next_slot = n % REWIND_SLOTS;
    load_state_for_runtime_mem(s_rw_data + (size_t)s_rw_seq[0] * s_rw_state_size);
    DebugSuite_SetRewindPos(s_rw_cursor, s_rw_len);
}

extern void GameInit(void);
extern void GameTick(void);
extern int  Game_GetScene(void);
extern void Game_SyncGbcColor(void);
extern void Gen1Color_Tick(void);
extern int  AmberScript_MapWarp(const char *name, int x, int y);

static int  s_mwarp_pending = 0;
static char s_mwarp_map[32] = {0};
static int  s_mwarp_x = 0, s_mwarp_y = 0;

#ifndef _MSC_VER
WEAK void GameInit(void) {

    extern void WRAMClear(void);
    WRAMClear();

    if (Save_Load() == 0)
        printf("[save] Save loaded OK\n");
    else
        printf("[save] No save file, starting new game\n");
}

WEAK void GameTick(void) {

    static uint32_t frame = 0;
    for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++)
        wTileMap[i] = (uint8_t)((i + frame / 4) & 0xFF);
    frame++;
}
#endif

static void update_random(void) {
    Random_FrameTick();
}

static void anchor_cwd_to_exe_dir(int skip) {
    char dir[1024];
    if (skip) return;

    if (!DataDir_Get(dir, sizeof dir)) {
        fprintf(stderr, "warning: could not resolve a data directory; saves "
                        "and assets will use the current directory instead\n");
        return;
    }
    if (!UserData_MigrateFromInstall())
        fprintf(stderr, "warning: could not migrate all existing saves and settings\n");
    if (suite_chdir(dir) != 0) {
        fprintf(stderr, "warning: could not enter %s; saves and assets "
                        "will use the current directory instead\n", dir);
        return;
    }

    if (DataDir_IsSeparate()) {
        fprintf(stderr, "[boot] data directory: %s\n", dir);
        DataDir_SeedFromInstall();
    }
}

static void sync_speed_badge(void) {
    static int last = -1;
    int pct = DebugSuite_SpeedPct();
    if (pct == last) return;
    last = pct;
    if (pct == 100)     Display_SetSpeedBadge(NULL);
    else if (pct == 0)  Display_SetSpeedBadge("TURBO");
    else {
        char b[16];

        if (pct % 100 == 0) snprintf(b, sizeof(b), "%dX", pct / 100);
        else                snprintf(b, sizeof(b), "%d%%", pct);
        Display_SetSpeedBadge(b);
    }
}

#ifdef HAVE_ROM_LAUNCHER

static launcher_result_t boot_launcher(char *picked, size_t picked_sz) {
    struct stat st;
    const char *tools_dir =
        (stat("tools/assetpack", &st) == 0) ? "tools/assetpack" :
        (stat("../tools/assetpack", &st) == 0) ? "../tools/assetpack" : NULL;
    const char *romimport_tools_dir =
        (stat("tools/romimport", &st) == 0) ? "tools/romimport" :
        (stat("../tools/romimport", &st) == 0) ? "../tools/romimport" : NULL;
    const char *out_pak =
        (tools_dir && tools_dir[0] == '.') ? "../" ASSETPACK_DEFAULT_PATH
                                           : ASSETPACK_DEFAULT_PATH;

    launcher_result_t result = Launcher_Run(tools_dir, out_pak, romimport_tools_dir,
                                            picked, picked_sz);
    SuspendMenu_SetDebugToolingEnabled(Launcher_DebugToolingEnabled());
    return result;
}
#endif

extern int gSkipMenu;
extern int gSkipHallOfFameToCredits;

int main(int argc, char *argv[]) {
#ifndef _WIN32

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

#endif

    int fs_from_cli = -1;

    int lab_mode = 0;
    int workdir_set = 0;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--workdir=", 10) == 0 && argv[i][10]) {
            suite_mkdir(argv[i] + 10);
            if (suite_chdir(argv[i] + 10) != 0) {
                fprintf(stderr, "[suite] --workdir: chdir to '%s' failed\n",
                        argv[i] + 10);
                return 1;
            }
            workdir_set = 1;
        } else if (strcmp(argv[i], "--lab") == 0) {
            lab_mode = 1;
        }
    }

    if (workdir_set) UserData_UseCurrentDirectory();

    anchor_cwd_to_exe_dir(workdir_set);

    if (freopen("pokered_log.txt", "w", stdout) != NULL)
        setvbuf(stdout, NULL, _IOLBF, 8192);

#ifdef _WIN32
    if (_dup2(_fileno(stdout), _fileno(stderr)) == 0)
#else
    if (dup2(fileno(stdout), fileno(stderr)) != -1)
#endif
        setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "[boot] entered main\n");

    if (lab_mode) {

        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        Display_SetHiddenWindow(1);
        DebugSuite_SetLabMode(1);
        printf("[suite] lab mode: hidden window, dummy audio, turbo on\n");
        fflush(stdout);
    }

    int sfx_debug = 0;
    int debug_render = 0;
    int debug_render_typing = 0;
    int mute = 0;
    int render_fps = 60;
    int render_fps_from_cli = 0;

    char force_version[16] = "";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--skip") == 0 ||
            strcmp(argv[i], "--quickstart") == 0 ||
            strcmp(argv[i], "--skip-title") == 0) {
            gSkipMenu = 1;
                    } else if (strcmp(argv[i], "--fullscreen") == 0) {
                Display_SetFullscreen(1);
                fs_from_cli = 1;
            } else if (strcmp(argv[i], "--windowed") == 0) {
                Display_SetFullscreen(0);
                fs_from_cli = 0;
            } else if (strcmp(argv[i], "--skiphof") == 0) {
            gSkipHallOfFameToCredits = 1;
            gSkipMenu = 1;
        } else if (strncmp(argv[i], "--version=", 10) == 0 && argv[i][10]) {

            snprintf(force_version, sizeof(force_version), "%s", argv[i] + 10);
        } else if (strcmp(argv[i], "--widescreen") == 0) {

            Display_SetWidescreen(1);
        } else if (strcmp(argv[i], "--sfx-debug") == 0) {
            sfx_debug = 1;
        } else if (strcmp(argv[i], "--debug-render") == 0) {
            debug_render = 1;
        } else if (strcmp(argv[i], "--mute") == 0) {

            mute = 1;
        } else if (strncmp(argv[i], "--render-fps=", 13) == 0) {

            render_fps = atoi(argv[i] + 13);
            render_fps_from_cli = 1;
            if (render_fps < 15 || render_fps > 360) {
                fprintf(stderr, "[display] --render-fps must be 15..360\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--pson") == 0) {

            AmberScript_SetEnabled(1);
        } else if (strncmp(argv[i], "--mwarp-", 8) == 0) {

            char spec[64];
            snprintf(spec, sizeof(spec), "%s", argv[i] + 8);
            char *py = strrchr(spec, '-');
            char *px = NULL;
            if (py) { *py = '\0'; py++; px = strrchr(spec, '-'); }
            if (py && px) {
                *px = '\0'; px++;
                snprintf(s_mwarp_map, sizeof(s_mwarp_map), "%s", spec);
                s_mwarp_x = atoi(px);
                s_mwarp_y = atoi(py);
                s_mwarp_pending = 1;
                gSkipMenu = 1;
                AmberScript_SetEnabled(1);
            } else {
                fprintf(stderr, "[mwarp] bad flag '%s' (want --mwarp-<Map>-<x>-<y>)\n", argv[i]);
            }
        }
    }

    {
        char err[512], err1[512];

        const char *installed[GAMEVER_MAX];
        int n_installed = GameVersion_ScanInstalled(installed, GAMEVER_MAX);
        char picked[16] = "";

        if (force_version[0]) {
            GameVersion_Set(force_version);
        } else if (n_installed > 0) {
#ifdef HAVE_ROM_LAUNCHER
            launcher_result_t launch = boot_launcher(picked, sizeof picked);
            if (launch == LAUNCHER_RESTART) return 75;
            if (launch != LAUNCHER_GOT_PAK)
                return 0;
            if (picked[0]) GameVersion_Set(picked);
#else

            GameVersion_Set(installed[0]);
#endif
        }

        char pkg_dir[64], pkg_dir_up[72];
        snprintf(pkg_dir, sizeof pkg_dir, "packages/%s", GameVersion_Current());
        snprintf(pkg_dir_up, sizeof pkg_dir_up, "../packages/%s", GameVersion_Current());
        if (!Pkg_MountList(pkg_dir, err1, sizeof err1) &&
            !Pkg_MountList(pkg_dir_up, err1, sizeof err1)) {

            if (strcmp(GameVersion_Current(), "red") != 0) {
                fprintf(stderr, "assets: no packages/%s -- %s\n",
                        GameVersion_Current(), err1);
            } else
            if (!AssetPack_Open(ASSETPACK_DEFAULT_PATH, err, sizeof err) &&
                !AssetPack_Open("../" ASSETPACK_DEFAULT_PATH, err, sizeof err)) {
#ifdef HAVE_ROM_LAUNCHER

                launcher_result_t launch = boot_launcher(picked, sizeof picked);
                if (launch == LAUNCHER_RESTART) return 75;
                if (launch != LAUNCHER_GOT_PAK) {
                    fprintf(stderr, "%s\n%s\n", err1, err);
                    return 1;
                }
                if (picked[0]) GameVersion_Set(picked);

                snprintf(pkg_dir, sizeof pkg_dir, "packages/%s",
                         GameVersion_Current());
                snprintf(pkg_dir_up, sizeof pkg_dir_up, "../packages/%s",
                         GameVersion_Current());
                if (!Pkg_MountList(pkg_dir, err1, sizeof err1) &&
                    !Pkg_MountList(pkg_dir_up, err1, sizeof err1)) {
                    fprintf(stderr, "assets: import finished but no packages/%s -- %s\n",
                            GameVersion_Current(), err1);
                    return 1;
                }
#else
                fprintf(stderr, "%s\n%s\n", err1, err);
                return 1;
#endif
            }
        }
        AssetPack_BindAll();

        MapEvents_LoadFromPack();
        DexEntries_LoadFromPack();
    }

    if (mute) {
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        printf("[main] --mute: audio driver forced to dummy\n");
        fflush(stdout);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER |
                 SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    Display_SetDebugRenderMode(debug_render);

    PresentationMenu_PreloadRenderer();
    if (Display_Init() != 0) {
        fprintf(stderr, "Display_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    Display_SetRenderFPS(render_fps);

    if (Audio_Init() != 0) {
        fprintf(stderr, "Audio_Init failed: %s (continuing without audio)\n",
                SDL_GetError());
    }
    Audio_SetMoveSfxDebug(sfx_debug);
    if (sfx_debug) {
        printf("[debug] Move SFX tracing ON (--sfx-debug)\n");
    }

    Input_Init();

    PresentationMenu_LoadSettings();

    if (render_fps_from_cli) Display_SetRenderFPS(render_fps);
    if (Display_RenderFPS() != 60) {
        printf("[display] render cap: %d FPS "
               "(simulation remains 59.7275 Hz)\n", Display_RenderFPS());
    }

    if (fs_from_cli >= 0) Display_SetFullscreen(fs_from_cli);

    if (PresentationMenu_FastBoot()) gSkipMenu = 1;

    GameInit();
    if (debug_render) {
        DebugCLI_ConsoleSetOverlayEnabled(0);
        DebugCLI_ConsoleSetAlwaysOpen(1);
        SDL_StopTextInput();
    }
    s_rw_state_size = Save_StateSize();
    s_rw_data = (uint8_t *)malloc((size_t)REWIND_SLOTS * s_rw_state_size);
    if (!s_rw_data) {
        fprintf(stderr, "rewind buffer alloc failed\n");
        Audio_Quit();
        Input_Quit();
        Display_Quit();
        SDL_Quit();
        return 1;
    }
    DebugSuite_Init(s_rw_data, s_rw_seq, &s_rw_len, s_rw_state_size,
                    REWIND_SLOTS);
    {
        int slot = rewind_capture_snapshot();
        if (slot >= 0) DebugSuite_RecordFrame(hJoyInput, slot);
    }

    #define GB_FRAME_HZ_NUM 4194304ull
    #define GB_FRAME_HZ_DEN 70224ull
    const uint32_t FRAME_MS = 1000 / 60;

    const uint64_t video_perf_hz    = SDL_GetPerformanceFrequency();
    const uint64_t video_period_ctr = video_perf_hz * GB_FRAME_HZ_DEN / GB_FRAME_HZ_NUM;
    uint64_t video_deadline = SDL_GetPerformanceCounter();
    uint32_t frame = 0;
    int running = 1;

    uint64_t perf_hz_dbg = SDL_GetPerformanceFrequency();
    uint64_t t_frame_start = 0, t_after_tick = 0, t_after_render = 0;
    (void)t_after_render;

    uint64_t music_base_ctr = SDL_GetPerformanceCounter();
    uint64_t music_perf_hz  = SDL_GetPerformanceFrequency();
    uint64_t music_ticks    = 0;

    uint64_t sfx_base_ctr   = 0;
    uint64_t sfx_ticks      = 0;

    static const int kSpeedCycle[] = { 100, 200, 300, 400, 0 };
    int ff_held = 0, ff_prev_pct = 100;

    while (running) {
        t_frame_start = SDL_GetPerformanceCounter();
        t_after_tick  = 0;
        uint32_t frame_start = SDL_GetTicks();
        Input_SetGameplayInputBlocked(debug_render && debug_render_typing);

        if (Display_IsSteamDeck()) {
            static int focus_frames;
            if (!Display_HasInputFocus()) {
                focus_frames = 0;
                Display_SuspendFullscreenForOverlay(1);
            } else if (++focus_frames >= 45) {
                Display_SuspendFullscreenForOverlay(0);
            }
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {

            SuspendMenu_HandleEvent((const union SDL_Event *)&ev);

            int is_cli_toggle = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0) {
                SDL_Scancode sc = ev.key.keysym.scancode;
                SDL_Keycode kc = ev.key.keysym.sym;
                SDL_Keymod km = (SDL_Keymod)ev.key.keysym.mod;
                is_cli_toggle =
                    (sc == SDL_SCANCODE_GRAVE) ||
                    (kc == SDLK_BACKQUOTE)     ||
                    ((km & KMOD_SHIFT) && kc == SDLK_BACKQUOTE);
            }
            if (ev.type == SDL_QUIT) running = 0;

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                SDL_Keymod km = (SDL_Keymod)ev.key.keysym.mod;
                if (DebugCLI_ConsoleIsOpen()) {
                    DebugCLI_ConsoleClose();
                    if (!debug_render) SDL_StopTextInput();
                } else if (SuspendMenu_IsCapturing()) {

                } else if (km & KMOD_SHIFT) {
                    running = 0;
                } else if (SuspendMenu_IsOpen()) {

                } else {
                    SuspendMenu_Toggle();
                }
            }

            if (Input_PadMenuPressed() && !DebugCLI_ConsoleIsOpen() &&
                !SuspendMenu_IsOpen())
                SuspendMenu_Toggle();

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_RETURN &&
                (ev.key.keysym.mod & KMOD_ALT)) {
                Display_ToggleFullscreen();
            }

            if (is_cli_toggle) {
                if (debug_render) {
                    debug_render_typing = !debug_render_typing;
                    if (debug_render_typing) SDL_StartTextInput();
                    else SDL_StopTextInput();
                } else if (DebugCLI_ConsoleIsOpen()) {
                    DebugCLI_ConsoleClose();
                    SDL_StopTextInput();
                } else {
                    DebugCLI_ConsoleOpen();
                    SDL_StartTextInput();
                }
            }

            if (ev.type == SDL_KEYDOWN && DebugCLI_ConsoleIsOpen() && !debug_render) {
                if (ev.key.keysym.scancode == SDL_SCANCODE_RETURN ||
                    ev.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) {
                    DebugCLI_ConsoleExecute();
                    SDL_StopTextInput();
                } else if (ev.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
                    DebugCLI_ConsoleBackspace();
                }
            }
            if (ev.type == SDL_KEYDOWN && DebugCLI_ConsoleIsOpen() && debug_render) {
                if (debug_render_typing &&
                    ev.key.keysym.scancode == SDL_SCANCODE_BACKSPACE) {
                    DebugCLI_ConsoleBackspace();
                } else if (debug_render_typing &&
                           (ev.key.keysym.scancode == SDL_SCANCODE_RETURN ||
                            ev.key.keysym.scancode == SDL_SCANCODE_KP_ENTER)) {
                    DebugCLI_ConsoleExecute();
                }
            }

            if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0 &&
                !(debug_render && debug_render_typing)) {
                if (ev.key.keysym.scancode == SDL_SCANCODE_COMMA) {
                    rewind_step_older();
                } else if (ev.key.keysym.scancode == SDL_SCANCODE_PERIOD) {
                    rewind_step_newer();
                }
            }

            if (ev.type == SDL_TEXTINPUT && DebugCLI_ConsoleIsOpen()) {
                const char *t = ev.text.text;

                if ((debug_render ? debug_render_typing : 1) &&
                    t[0] != '`' && t[0] != '~' && t[1] == '\0')
                    DebugCLI_ConsoleAddChar(t[0]);
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F1)
                Debug_PrintGrid();

            if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0 &&
                ev.key.keysym.scancode == SDL_SCANCODE_F2) {
                DebugSuite_CaptureReport(NULL);
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F3) {
                if (ev.key.keysym.mod & KMOD_SHIFT) {
                    int on = !Debug_CollisionOverlayOn();
                    Debug_SetCollisionOverlay(on);
                    printf("[debug] Collision overlay %s\n", on ? "ON" : "OFF");
                } else {
                    int on = !Display_GetBlockIDOverlay();
                    Display_SetBlockIDOverlay(on);
                    printf("[debug] Block-ID overlay %s\n", on ? "ON" : "OFF");
                }
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F4) {
                gNoClip = !gNoClip;
                printf("[debug] No-clip %s\n", gNoClip ? "ON" : "OFF");
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F6) {
                gNoWilds = !gNoWilds;
                printf("[debug] Wild encounters %s\n", gNoWilds ? "OFF" : "ON");
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F7) {
                if (ev.key.keysym.mod & KMOD_SHIFT) {
                    int on = !Debug_CombatLogOn();
                    Debug_SetCombatLog(on);
                } else {
                    Debug_PrintBattleState();
                }
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F8) {
                Debug_DumpWRAM();
            }

            if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0 &&
                ev.key.keysym.scancode == SDL_SCANCODE_R &&
                !DebugCLI_ConsoleIsOpen()) {
                breakpoint_commit_now();
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F9) {
                if (Input_IsRecording()) {
                    Input_StopRecording();
                } else {
                    char path[256];
                    snprintf(path, sizeof(path),
                             "bugs/recording_%lu.bin", (unsigned long)time(NULL));
                    Input_StartRecording(path);
                }
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F10) {
                if (Input_IsPlaying()) {
                    Input_StopPlayback();
                } else {
                    Input_StartPlayback("bugs/recording_latest.bin");
                }
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F11) {
                if (Save_StateWrite("bugs/savestate.bin") == 0)
                    printf("[state] State saved.\n");
                else
                    printf("[state] State save failed.\n");
            }

            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.scancode == SDL_SCANCODE_F12) {
                if (Save_StateLoad("bugs/savestate.bin") == 0) {

                    AmberScript_ReloadAfterStateLoad();
                    Player_IgnoreInputFrames(4);
                    printf("[state] State loaded.\n");
                } else {
                    printf("[state] No save state found.\n");
                }
            }

            if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0 &&
                ev.key.keysym.scancode == SDL_SCANCODE_TAB &&
                !(ev.key.keysym.mod & KMOD_SHIFT) &&
                !(debug_render ? debug_render_typing : DebugCLI_ConsoleIsOpen())) {
                int cur = ff_held ? ff_prev_pct : DebugSuite_SpeedPct();
                int n = (int)(sizeof(kSpeedCycle) / sizeof(kSpeedCycle[0]));
                int i, next = 100;
                for (i = 0; i < n; i++)
                    if (kSpeedCycle[i] == cur) { next = kSpeedCycle[(i + 1) % n]; break; }

                ff_prev_pct = next;
                if (!ff_held) DebugSuite_SetSpeedPct(next);
                if (next == 0) printf("[speed] TURBO (uncapped)\n");
                else           printf("[speed] %d%%\n", next);
            }
        }

        {
            const uint8_t *keys = SDL_GetKeyboardState(NULL);
            int typing = debug_render ? debug_render_typing
                                      : DebugCLI_ConsoleIsOpen();
            int want = !typing && keys[SDL_SCANCODE_TAB] &&
                       (SDL_GetModState() & KMOD_SHIFT) != 0;
            if (want != ff_held) {
                ff_held = want;
                if (want) {
                    ff_prev_pct = DebugSuite_SpeedPct();
                    DebugSuite_SetSpeedPct(0);
                } else {
                    DebugSuite_SetSpeedPct(ff_prev_pct);
                }
            }
        }

        sync_speed_badge();

        {
            const uint8_t *keys = SDL_GetKeyboardState(NULL);
            int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
            int did_scrub = 0;
            if (!DebugCLI_ConsoleIsOpen() && ctrl && keys[SDL_SCANCODE_Z]) {
                did_scrub = rewind_step_older();
            } else if (!DebugCLI_ConsoleIsOpen() && ctrl && keys[SDL_SCANCODE_Y]) {
                did_scrub = rewind_step_newer();
            }
            if (did_scrub) {
                Display_RenderScrolled(gScrollPxX, gScrollPxY, gScrollTileMap, SCROLL_MAP_W);
                uint32_t elapsed = SDL_GetTicks() - frame_start;
                if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
                frame++;
                continue;
            }
        }

        {
            char bp_name[80];
            if (DebugSuite_TakeBreakpointCommit()) breakpoint_commit_now();
            if (DebugSuite_TakeBreakpointRestore(bp_name, sizeof(bp_name)))
                breakpoint_restore_now(bp_name);
        }

        {
            int rwd = DebugSuite_TakeRewindDelta();
            int scrubbed = 0;
            while (rwd > 0) { if (!rewind_step_older()) break; rwd--; scrubbed = 1; }
            while (rwd < 0) { if (!rewind_step_newer()) break; rwd++; scrubbed = 1; }
            DebugSuite_SetRewindPos(s_rw_cursor, s_rw_len);
            if (scrubbed) {

                Display_RenderScrolled(gScrollPxX, gScrollPxY, gScrollTileMap, SCROLL_MAP_W);
                if (Display_SaveScreenshot("bugs/screen_tmp.bmp") == 0) {
                    remove("bugs/screen.bmp");
                    rename("bugs/screen_tmp.bmp", "bugs/screen.bmp");
                }
            }
        }

        if (Display_BackendRestartPending() && !Display_SuspendOverlayActive())
            Display_ApplyBackendRestart();

        if (SuspendMenu_ExitToLauncherRequested()) {
            running = 0;
            continue;
        }

        if (DebugSuite_QuitRequested()) {
            running = 0;
            continue;
        }
        {
            int unfocused = !Display_HasInputFocus();
            int pause_unfocused = unfocused &&
                                  PresentationMenu_PauseWhenUnfocused();
            Audio_SetFocusMuted(unfocused &&
                (PresentationMenu_MuteWhenUnfocused() || pause_unfocused));
        }

        DebugSuite_TopOfFrame();
        if (!DebugSuite_FrameGate()) {

            DebugSuite_PausedTick();
            DebugCLI_ConsoleRender();
            Display_RenderScrolled(gScrollPxX, gScrollPxY,
                                   gScrollTileMap, SCROLL_MAP_W);
            SDL_Delay(8);
            continue;
        }

        {
            int unfocused = !Display_HasInputFocus();
            int pause_unfocused = unfocused &&
                                  PresentationMenu_PauseWhenUnfocused();
            if (pause_unfocused && !SuspendMenu_IsOpen() &&
                !PresentationMenu_IsOpen()) {

                Input_Update();
                DebugCLI_PollExternal();
                DebugCLI_PumpButtons();
                DebugCLI_ConsoleRender();
                Display_RenderScrolled(gScrollPxX, gScrollPxY,
                                       gScrollTileMap, SCROLL_MAP_W);
                music_base_ctr = SDL_GetPerformanceCounter();
                music_ticks = 0;
                sfx_base_ctr = 0;
                video_deadline = SDL_GetPerformanceCounter();
                SDL_Delay(16);
                continue;
            }
        }

        Display_SetFrameWidth(Display_WantFrameWidth(
            wIsInBattle == 0 && !BattleUI_IsActive()));

        {
            static int s_geom_w = SCREEN_WIDTH_PX;
            int geom_w = Display_FrameWidth();
            if (geom_w != s_geom_w) {

                Map_UpdateCamera();
                if (!BattleUI_IsActive() && !TitleScreen_IsOpen() &&
                    !Intro_IsActive() && !Pokedex_IsOpen() &&
                    !PartyMenu_IsOpen() && !TrainerCard_IsOpen()) {
                    Player_SyncOAM();
                    Map_BuildScrollView();
                    NPC_BuildView(gScrollPxX, gScrollPxY);
                }
            }
            s_geom_w = geom_w;
        }

        Display_SetAuthoredFrame(TitleScreen_IsOpen() || Intro_IsActive());

        Display_SetLetterboxFrame(Pokedex_IsOpen() || PartyMenu_IsOpen() ||
                                  TrainerCard_IsOpen() || TownMap_IsOpen() ||
                                  CreditsScripts_IsActive(),
                                  (TrainerCard_IsOpen() || TownMap_IsOpen())
                                      ? DISPLAY_BOX_BLACK
                                      : DISPLAY_BOX_EXTEND);

        update_random();
        DebugSuite_InjectInput();
        DebugSuite_InjectLiveHold();
        Input_Update();
        hVBlankOccurred = 1;
        hFrameCounter   = (hFrameCounter - 1) & 0xFF;

        if (SuspendMenu_IsOpen()) {

            DebugCLI_PollExternal();
            DebugCLI_PumpButtons();
            SuspendMenu_Tick();

            Game_SyncGbcColor();
        } else if (PresentationMenu_IsOpen()) {

            DebugCLI_PollExternal();
            DebugCLI_PumpButtons();
            PresentationMenu_Tick();

            Game_SyncGbcColor();
        } else {
            GameTick();
            t_after_tick = SDL_GetPerformanceCounter();
            Game_SyncGbcColor();
            Gen1Color_Tick();
        }

        if (s_mwarp_pending && Game_GetScene() == 0) {
            printf("[mwarp] warping to %s (%d,%d)\n", s_mwarp_map, s_mwarp_x, s_mwarp_y);
            fflush(stdout);
            AmberScript_MapWarp(s_mwarp_map, s_mwarp_x, s_mwarp_y);
            s_mwarp_pending = 0;
        }

        {

            int sfx_pct = DebugSuite_SpeedPct();
            if (sfx_pct == 100) sfx_pct = SpeedSettings_OverworldFramePct();
            int sfx_wall = !DebugSuite_IsLab() &&
                           (sfx_pct == 0 || sfx_pct > 100);
            if (!sfx_wall) {
                sfx_base_ctr = 0;
                Audio_UpdateSfx();
            } else {
                uint64_t now = SDL_GetPerformanceCounter();
                if (sfx_base_ctr == 0) { sfx_base_ctr = now; sfx_ticks = 0; }
                uint64_t elapsed = now - sfx_base_ctr;
                uint64_t due = (elapsed * GB_FRAME_HZ_NUM)
                             / (music_perf_hz * GB_FRAME_HZ_DEN);

                if (due > sfx_ticks + 12) {
                    sfx_base_ctr = now;
                    sfx_ticks    = 0;
                    Audio_UpdateSfx();
                } else {
                    while (sfx_ticks < due) { sfx_ticks++; Audio_UpdateSfx(); }
                }
            }
        }
        if (DebugSuite_IsLab()) {

            Audio_UpdateMusic();
        } else {

            uint64_t elapsed = SDL_GetPerformanceCounter() - music_base_ctr;
            uint64_t due = (elapsed * GB_FRAME_HZ_NUM)
                         / (music_perf_hz * GB_FRAME_HZ_DEN);

            if (due > music_ticks + 12) {

                music_base_ctr = SDL_GetPerformanceCounter();
                music_ticks    = 0;
                Audio_UpdateMusic();
            } else {
                while (music_ticks < due) { music_ticks++; Audio_UpdateMusic(); }
            }
        }
        if (Debug_CollisionOverlayOn()) Debug_UpdateOverlay();

        BagListChoice_Refresh();

        if (YesNo_IsOpen()) YesNo_PostRender();
        DebugCLI_ConsoleRender();
        Display_RenderScrolled(gScrollPxX, gScrollPxY,
                               gScrollTileMap, SCROLL_MAP_W);
        {
            extern uint64_t g_dbg_swap_ticks;

            extern uint64_t g_dbg_stream_ticks;
            extern unsigned g_dbg_stream_count;
            static uint64_t stream_prev;
            static unsigned stream_prev_n;
            static uint64_t swap_prev;
            uint64_t t_end = SDL_GetPerformanceCounter();
            double ms_swap = (double)(g_dbg_swap_ticks - swap_prev) * 1000.0
                             / (double)perf_hz_dbg;
            double ms_all  = (double)(t_end - t_frame_start) * 1000.0 / (double)perf_hz_dbg;
            swap_prev = g_dbg_swap_ticks;
            double ms_tick = t_after_tick
                           ? (double)(t_after_tick - t_frame_start) * 1000.0 / (double)perf_hz_dbg
                           : -1.0;
            double ms_rend = t_after_tick
                           ? (double)(t_end - t_after_tick) * 1000.0 / (double)perf_hz_dbg
                           : -1.0;
            double ms_stream = (double)(g_dbg_stream_ticks - stream_prev) * 1000.0
                               / (double)perf_hz_dbg;
            unsigned n_stream = g_dbg_stream_count - stream_prev_n;
            stream_prev   = g_dbg_stream_ticks;
            stream_prev_n = g_dbg_stream_count;
            if (ms_all > 20.0) {
                FILE *pf = fopen("bugs/perf.log", "a");
                if (pf) {
                    fprintf(pf, "[PERFDBG] frame=%u map=%u xy=(%d,%d) "
                                "tick=%.1fms render=%.1fms (swap %.1fms) "
                                "loop=%.1fms stream=%.1fms x%u\n",
                            (unsigned)frame, (unsigned)wCurMap,
                            (int)wXCoord, (int)wYCoord,
                            ms_tick, ms_rend, ms_swap, ms_all,
                            ms_stream, n_stream);
                    fclose(pf);
                }
            }
        }

        if (!DebugCLI_IsReplayPlaying() && !DebugSuite_ReplayActive()) {
            s_rw_frame_div++;
            if (s_rw_frame_div >= REWIND_SNAPSHOT_EVERY) {
                s_rw_frame_div = 0;
                int slot = rewind_capture_snapshot();
                if (slot >= 0) DebugSuite_RecordFrame(hJoyInput, slot);
            }
        }

        DebugSuite_EndFrame();

        hVBlankOccurred = 0;

        {
            int pct = DebugSuite_SpeedPct();

            if (pct == 100) pct = SpeedSettings_OverworldFramePct();

            Display_SetSpeedPct(pct);
            if (pct != 0) {
                const uint64_t period = video_period_ctr * 100u / (uint64_t)pct;
                uint64_t now = SDL_GetPerformanceCounter();

                video_deadline += period;

                if (video_deadline < now || video_deadline - now > period * 4) {
                    video_deadline = now + period;
                }

                for (;;) {
                    uint64_t left, wake_deadline = video_deadline;
                    uint64_t present_deadline = 0;

                    if (Display_RenderFPS() > 60) {
                        present_deadline = Display_NextPresentCounter();
                        if (present_deadline != 0 &&
                            present_deadline < wake_deadline)
                            wake_deadline = present_deadline;
                    }

                    now = SDL_GetPerformanceCounter();
                    if (now >= video_deadline) break;
                    if (present_deadline != 0 && now >= present_deadline) {
                        Display_PresentLatestIfDue();
                        continue;
                    }
                    left = (wake_deadline - now) * 1000u / video_perf_hz;

                    SDL_Delay(left > 1 ? (uint32_t)(left - 1) : 0);
                }
            } else {

                video_deadline = SDL_GetPerformanceCounter();
            }
        }

        frame++;
    }

    Audio_Quit();
    Input_Quit();
    Display_Quit();
    free(s_rw_data);
    SDL_Quit();

    if (SuspendMenu_ExitToLauncherRequested()) {
        char *newargv[2];
        newargv[0] = argv[0];
        newargv[1] = NULL;
        fflush(stdout);
        fflush(stderr);
#ifdef _WIN32

        _execv(argv[0], (const char *const *)newargv);
#else
        execv(argv[0], newargv);
#endif

        fprintf(stderr, "exit to launcher: could not restart %s\n", argv[0]);
        return 1;
    }

    return 0;
}
