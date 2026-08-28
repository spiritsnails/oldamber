cmake_minimum_required(VERSION 3.16)
project(pokered C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# SDL2, support both modern targets and legacy variable style
#
# The MSVC development libraries are not carried in this tree. They are 24 MB of
# prebuilt headers and .lib files that only an MSVC build reads, so a developer
# who wants them unpacks the upstream archive into deps/SDL2-VC/ themselves.
# Globbed rather than pinned to one version, so whichever they download works.
file(GLOB SDL2_VC_DIRS "${CMAKE_SOURCE_DIR}/deps/SDL2-VC/SDL2-*")
set(SDL2_VC_ROOT "")
foreach(candidate ${SDL2_VC_DIRS})
  if(EXISTS "${candidate}/cmake/sdl2-config.cmake")
    set(SDL2_VC_ROOT "${candidate}")
  endif()
endforeach()
if(MSVC AND SDL2_VC_ROOT)
  message(STATUS "SDL2: using ${SDL2_VC_ROOT}")
  list(PREPEND CMAKE_PREFIX_PATH "${SDL2_VC_ROOT}/cmake")
elseif(MSVC)
  # Not fatal: find_package still searches, and vcpkg or a system install is a
  # perfectly good answer. But if it comes up empty the error is CMake's own
  # "Could NOT find SDL2", which does not say where to get one, so say it here.
  message(STATUS
    "SDL2: no MSVC development libraries in deps/SDL2-VC/.\n"
    "   Download SDL2-devel-<version>-VC.zip from\n"
    "   https://github.com/libsdl-org/SDL/releases and unpack it there, so you\n"
    "   have deps/SDL2-VC/SDL2-<version>/cmake/sdl2-config.cmake, or point\n"
    "   CMAKE_PREFIX_PATH at an SDL2 you already have.")
elseif(MINGW AND EXISTS "C:/msys64/mingw64")
  # A hint, not a requirement: MSYS2's default prefix when this machine has
  # it. A MinGW toolchain installed elsewhere still resolves SDL2 through the
  # normal find_package search.
  list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()
find_package(SDL2 REQUIRED)
# Modern SDL2 CMake exports SDL2::SDL2 targets; fallback to legacy variables
if(TARGET SDL2::SDL2)
  # Use SDL2 target only; SDL2main causes WinMain entry issues on MinGW.
  # The Windows link below adds -mconsole so the entry point stays main().
  set(SDL2_LINK_TARGETS SDL2::SDL2)
else()
  set(SDL2_LINK_TARGETS ${SDL2_LIBRARIES})
  include_directories(${SDL2_INCLUDE_DIRS})
endif()
# Always add include dirs (SDL2::SDL2 carries them transitively, but be safe)
if(SDL2_INCLUDE_DIRS)
  include_directories(${SDL2_INCLUDE_DIRS})
endif()

# Math (libm is implicit on Windows/MinGW, explicit on Linux)
find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
  set(MATH_LIBRARY "")
endif()

# Python, embedded in-process by src/platform/rom_import.c so the launcher can
# run tools/assetpack/build_pak.py when the player imports a ROM.
#
# Pinned to the Python311 install only when that exact path is present, so a
# machine without it still resolves through the normal find_package search.
# The pin keeps MSYS's mingw64 Python out of the way: find_package would
# otherwise reach it through the CMAKE_PREFIX_PATH hint above, and its
# libpython carries a prefix baked in at package build time, so Py_Initialize
# aborts unable to find `encodings`.
#
# Whether to embed CPython for in-process ROM import (rom_import.c), or shell
# out to a system python3 for it (rom_import_exec.c). Both run the same
# importer scripts; the SOURCES selection below keys on Python3_FOUND.
#
# ON suits Windows, where the bundle can carry a known interpreter. OFF is what
# a Linux/Steam Deck build wants: embedding puts libpython3.x.so.1.0 in the
# ELF's NEEDED list, so a binary built against one minor version will not start
# on a host with another.
option(AMBER_EMBED_PYTHON "Embed CPython for in-process ROM import" ON)
if(AMBER_EMBED_PYTHON)
    set(_amber_win_py "C:/Program Files/Python311")
    if(WIN32 AND EXISTS "${_amber_win_py}/python.exe")
        set(Python3_INCLUDE_DIR "${_amber_win_py}/include" CACHE PATH "" FORCE)
        set(Python3_LIBRARY "${_amber_win_py}/libs/python311.lib" CACHE FILEPATH "" FORCE)
        set(Python3_EXECUTABLE "${_amber_win_py}/python.exe" CACHE FILEPATH "" FORCE)
    endif()
    unset(_amber_win_py)
    find_package(Python3 COMPONENTS Development)
    if(NOT Python3_FOUND)
        message(WARNING "Python3 development libs not found -- pokered will build "
                        "without the in-process ROM launcher (src/platform/rom_import.c "
                        "and launcher.c excluded); the player must build assets.pak "
                        "via the CLI path instead.")
    endif()
else()
    # Nothing to find, and Python3_FOUND stays false, which is exactly what the
    # rom_import_exec.c branch below keys on.
    message(STATUS "AMBER_EMBED_PYTHON=OFF -- ROM import will run python3 as a "
                   "subprocess (src/platform/rom_import_exec.c)")
endif()

# RED_ONLY, build with no Crystal/Johto data.
#
# The Crystal data lives in generated/, which is gitignored because it is
# ROM-derived, so a checkout made from a Red ROM alone does not have those
# files at all.
#
# ON  -> compile stubs/crystal/crystal_stubs.c (all symbols zero) instead, with
#        stubs/crystal ahead of generated/ on the include path so the shared
#        engine files that #include "crystal_font.h" and friends still resolve.
#        The Crystal branches still compile; they are never reached, because a
#        Red-only game cannot put the player on a Johto map.
# OFF -> the full build. This is the default.
#
# Stubs are regenerated by tools/gen_crystal_stubs.py; --check reports drift.
option(RED_ONLY "Build Red standalone: no Crystal/Johto data required" OFF)

# Which base game this build is, and therefore which generated/ tree it reads.
#
# Red and Blue share every asset name and differ only in table contents. They
# still get their own directory: a shared pool would let a Blue build link
# Red's tables, and that failure looks like wrong data rather than a missing
# file.
#
# The sources include these headers by bare name, so nothing in src/ knows
# which version it is, only this include path changes.
set(GAME_VERSION "red" CACHE STRING "base game package: red or blue")
set_property(CACHE GAME_VERSION PROPERTY STRINGS red blue)
message(STATUS "GAME_VERSION: ${GAME_VERSION} (generated/${GAME_VERSION})")

# The generated Crystal data RED_ONLY replaces. Keep in step with MODULES in
# tools/gen_crystal_stubs.py: --check is what catches these drifting apart.
set(CRYSTAL_DATA_SOURCES
    ${CMAKE_SOURCE_DIR}/generated/gen2_pokedex.c
    ${CMAKE_SOURCE_DIR}/generated/gen2_evos_moves.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_palettes.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_font.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_menus.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_options.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_pack.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_icons.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_stats_screen.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_tile_anims.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_emotes.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_audio.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_sprites.c
    ${CMAKE_SOURCE_DIR}/generated/johto_trainers.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_mon_pics.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_trainer_pics.c
    ${CMAKE_SOURCE_DIR}/generated/crystal_battle_gfx.c
)
if(RED_ONLY)
    set(CRYSTAL_DATA_SOURCES ${CMAKE_SOURCE_DIR}/stubs/crystal/crystal_stubs.c)
    message(STATUS "RED_ONLY: Crystal data stubbed (no Crystal ROM required)")
endif()

# Sources
set(SOURCES
    ${CRYSTAL_DATA_SOURCES}
    src/main.c
    src/game/debug_suite.c
    src/game/breakpoint.c
    src/game/debug_trace.c
    src/game/debug_fields.c
    src/game/wram.c
    src/game/overworld.c
    src/game/player.c
    src/game/game.c
    src/game/gbc_color.c
    src/game/presentation_menu.c
    src/game/suspend_menu.c
    src/game/save_slots.c
    src/game/speed_settings.c
    src/game/glitches.c
    src/game/cycling_road_gate_scripts.c
    src/game/cycling_road_gate_scripts.c
    src/game/gen1color/gen1color_battle.c
    src/game/gen1color/gen1color_scene.c
    src/game/gen1color/crystal_pic_anim.c
    # The port's own Crystal logic, as opposed to the generated Crystal data
    # in CRYSTAL_DATA_SOURCES above. These compile either way and read
    # whatever that list resolves to, real data or stubs.
    src/game/crystal_color.c
    src/game/crystal_tile_anim.c
    src/game/crystal_grass_rustle.c
    src/game/map_music.c
    src/data/gbc_palettes.c
    src/platform/input.c
    src/platform/display.c
    # Optional OpenGL present path (SameBoy shaders). Always compiled, never
    # mandatory: DisplayGL_Init reports failure and display.c falls back to
    # SDL_Renderer, so a machine without GL loses the filters and nothing else.
    src/platform/display_gl.c
    src/platform/sgb_border.c
    src/platform/ntsc_filter.c
    # OldAmber's own CRT renderer. See
    # docs/crt-renderer-implementation-spec.md.
    src/platform/crt_renderer.c
    src/platform/gl_api.c
    src/platform/crt_renderer_gl.c
    # NTSC-CRT by EMMIR (LMP88959), vendored unmodified, see THIRD_PARTY.md.
    # Only the core and the SNES signal profile; CRT_SYSTEM below picks it.
    third_party/ntsc-crt/crt_core.c
    third_party/ntsc-crt/crt_snes.c
    src/platform/audio.c
    src/platform/gb_apu.c
    src/game/audio_engine.c
    src/platform/save.c
    # Which game this session is, runtime, not a #define. One binary plays
    # Red or Blue; see src/platform/game_version.h.
    src/platform/game_version.c
    src/data/base_stats.c
    src/data/moves_data.c
    src/data/type_chart.c
    # gTilesets and gMapTable are rebuilt at boot from assets.pak, so there is
    # no tileset_data.c or map_data.c to list.
    src/data/map_events_load.c
    src/data/town_map_data.c
    src/data/move_anim_scripts.c
    src/data/move_anim_subanims.c
    src/data/move_anim_frameblocks.c
    src/data/move_anim_basecoords.c
    # gCutTreeAnimTiles comes from assets.pak, so there is no cut_anim_tiles.c.
    src/platform/assetpack.c
    ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION}/assetpack_bind.c
    src/data/trade_gfx.c
    src/game/trade.c
    src/game/warp.c
    src/game/anim.c
    src/game/npc.c
    src/game/text.c
    src/game/naming_screen.c
    src/game/menu.c
    src/game/trainer_card.c
    src/game/diploma.c
    src/data/trainer_card_tiles.c
    src/data/sgb_border_data.c
    src/game/escape_anim.c
    src/game/inventory.c
    src/game/session_log.c
    src/game/pokemon.c
    src/game/bag_menu.c
    src/game/bicycle.c
    src/game/fly_anim.c
    src/game/party_menu.c
    src/game/pokecenter.c
    src/game/pokemart.c
    src/game/mon_pic.c
    src/game/crystal_fade.c
    src/game/crystal_icon_anim.c
    src/game/gen2_resources.c
    src/game/summary_screen.c
    src/game/battle/battle_damage.c
    src/game/battle/battle_effects.c
    src/game/battle/battle_core.c
    src/game/battle/battle_loop.c
    src/game/battle/battle_ai.c
    src/game/battle/battle_init.c
    src/game/battle/battle_exp.c
    src/game/battle/battle_driver.c
    src/game/battle/move_anim.c
    src/game/battle/battle_catch.c
    src/game/battle/battle_items.c
    src/game/battle/battle_switch.c
    src/game/battle/battle_trainer.c
    src/game/battle/battle_ui.c
    src/game/battle_transition.c
    src/game/gen2_species.c
    src/game/johto_battle.c
    src/game/trainer_sight.c
    src/game/rival_starter.c
    src/game/badge.c
    src/game/gate_scripts.c
    src/game/pokedex.c
    src/game/town_map.c
    src/game/blackboard.c
    src/game/bag_list_choice.c
    src/game/prize_list_choice.c
    src/game/money_box.c
    src/game/coin_box.c
    src/game/slot_machine.c
    src/game/fossil_popup.c
    src/game/title_screen.c
    src/game/main_menu.c
    src/game/intro.c
    src/game/pallet_scripts.c
    src/game/oakslab_scripts.c
    src/game/gym_scripts.c
    src/game/viridian_gym_spinners.c
    src/game/fishing.c
    src/game/viridian_mart_scripts.c
    src/game/route24_scripts.c
    src/game/blues_house_scripts.c
    src/game/reds_house1f_scripts.c
    src/game/bills_house_scripts.c
    src/game/bills_pokemon_list.c
    src/game/badge_house_menu.c
    src/game/route25_scripts.c
    src/game/dex_rating.c
    src/game/pc_menu.c
    src/game/players_pc.c
    src/game/route2gate_scripts.c
    src/game/ss_anne_depart.c
    src/game/vermilion_gym_scripts.c
    src/game/cinnabar_gym_scripts.c
    src/game/name_rater_scripts.c
    src/game/daycare.c
    src/game/poison.c
    src/game/yesno.c
    src/game/bikeshop_menu.c
    src/game/tmhm.c
    src/game/field_moves.c
    src/game/pokeflute.c
    src/game/rockethideout_b4f_scripts.c
    src/game/rockethideout_scripts.c
    src/game/game_corner_scripts.c
    src/game/pokemontower6f_scripts.c
    src/game/pokemontower7f_scripts.c
    src/game/mrfujis_house_scripts.c
    src/game/celadon_city_scripts.c
    src/game/route1_scripts.c
    src/game/saffron_city_scripts.c
    src/game/seafoam_scripts.c
    src/game/victory_road_scripts.c
    src/game/elite_four_scripts.c
    src/game/champions_room_scripts.c
    src/game/hall_of_fame_scripts.c
    src/game/credits_scripts.c
    src/game/safari_zone_scripts.c
    src/game/safari_zone_secret_house_scripts.c
    src/game/wardens_house_scripts.c
    src/game/mansion_scripts.c
    src/game/cinnabar_island_scripts.c
    src/game/itemfinder.c
    src/game/elevator_menu.c
    src/data/dex_data.c
    src/game/debug_overlay.c
    src/game/debug_cli.c
    src/game/amberscript_core.c
    src/game/amberscript_movement.c
    src/game/amberscript_tilemod.c
    src/game/amberscript_battle_debug.c
    src/game/amberscript_saveops.c
    src/game/amberscript_zone_resets.c
    src/game/amberscript_scene.c
    src/game/amberscript_story.c
    src/game/amberscript_party_items.c
    src/game/amberscript_misc.c
    src/game/amberscript_checkpoint.c
    src/game/amberscript_mapbank.c
src/game/static_encounter.c
    src/game/rom_text.c
    src/game/py_ai_bridge.c
    src/game/type_mod.c
    src/game/species_mod.c
    src/game/sprite_mod.c
    src/data/trainer_sprites.c
    src/data/party_icon_data.c
    src/data/font_data.c
    # gFontTiles, gBoxTiles and gHudTiles are bound at boot from assets.pak, so
    # there is no generated/gen1_battle_ui.c to list.
    src/data/pokedex_tiles.c
    src/data/slots_gfx.c
    src/data/town_map_data.c
    src/data/town_map_data.c
    # The sprite tables come from assets.pak: no sprite_data.c, player_sprite.c,
    # bike_sprite.c, seel_sprite.c, pokemon_sprites.c, fossil_sprites.c or
    # ghost_front_sprite.c.
    src/game/music.c
    src/game/johto_music.c
    # Where this executable lives. Always compiled: main.c anchors the working
    # directory to it before anything opens a file, and the import backend below
    # looks for the frozen setup binary beside it.
    src/platform/exe_dir.c
    # Where generated data lives, which is not always where the program
    # lives: a macOS bundle and a Flatpak both forbid writing beside it.
    src/platform/data_dir.c
)

# The launcher always ships: it is the ROM picker every player goes through.
# What varies is how it runs the importer.
#
#   Python3 dev libs found -> rom_import.c, CPython embedded in-process.
#   otherwise              -> rom_import_exec.c, the same scripts run as a
#                             subprocess against the system python3.
list(APPEND SOURCES
    src/platform/launcher.c
    # Own TU: <windows.h> may never meet hardware.h (launcher_native.h).
    src/platform/launcher_native.c
    src/platform/launcher_font.c
    src/platform/launcher_draw.c
    src/platform/launcher_nav.c
    src/platform/launcher_browse.c
    # Only does anything on a Deck, every entry point is gated on
    # Display_IsSteamDeck, but it compiles everywhere: the file is one
    # __linux__ branch with a stub tail, so no call site needs a platform
    # #if around it.
    src/platform/steam_shortcut.c
)
if(Python3_FOUND)
    list(APPEND SOURCES src/platform/rom_import.c)
else()
    list(APPEND SOURCES src/platform/rom_import_exec.c)
endif()
# The third import backend, and the one a release bundle uses: the frozen
# setup binary. A bundle ships no loose tools/ and no guaranteed python3, so
# neither of the two above can run there. Always compiled, and only used when
# the setup binary is actually beside the executable.
list(APPEND SOURCES src/platform/rom_import_setup.c)
# The file browser's "..." button on macOS. Compiled as Objective-C through a
# source property rather than enable_language(OBJC), so the project stays a
# C-only build.
if(APPLE)
    list(APPEND SOURCES src/platform/launcher_native_mac.m)
    set_source_files_properties(src/platform/launcher_native_mac.m
        PROPERTIES LANGUAGE C COMPILE_FLAGS "-x objective-c -fobjc-arc")
endif()
# The file chooser is launcher_browse.c: one opendir/readdir implementation
# that behaves the same on every target, drawn in the launcher's own chrome and
# driven by launcher_nav.c's intents, so it works from a gamepad, a
# touchscreen, a mouse or a keyboard. launcher_native.c adds an optional native
# dialog on top of it where one exists.

add_executable(pokered ${SOURCES})
# The launcher is always compiled, so main.c's call site is always live. Only
# the embedded interpreter's include path is conditional.
target_compile_definitions(pokered PRIVATE HAVE_ROM_LAUNCHER)
# The game id GameVersion_Current() starts at, for a session before the
# launcher picks one. See src/platform/game_version.h.
target_compile_definitions(pokered PRIVATE AMBER_GAME_VERSION="${GAME_VERSION}")

# Ad-hoc debug tracing, the bracketed [E4DBG]/[PALDBG]-style printfs. See
# src/platform/debug_log.h.
#
# Defaults on, Release included, so a developer build keeps its traces. Turning
# them off is a release step: tools/dist/make_release.sh refuses a build that
# left them on.
option(AMBER_DEBUG_PRINTS "Compile in the bracketed debug traces" ON)
if(AMBER_DEBUG_PRINTS)
    target_compile_definitions(pokered PRIVATE AMBER_DEBUG_PRINTS=1)
else()
    target_compile_definitions(pokered PRIVATE AMBER_DEBUG_PRINTS=0)
endif()

if(Python3_FOUND)
    target_include_directories(pokered PRIVATE ${Python3_INCLUDE_DIRS})
endif()

# RED_ONLY is the standalone player-facing build (tools/setup_red.sh), and it
# ships as OldAmber. The CMake target keeps the name `pokered`, which every dev
# script already uses; only the output binary is renamed.
if(RED_ONLY)
    set_target_properties(pokered PROPERTIES OUTPUT_NAME "oldamber")
endif()

# stubs/crystal goes ahead of generated/ under RED_ONLY so the mirrored
# crystal_*.h resolve there. A machine that has run the Crystal extractors must
# still take the stub headers, or it would compile against real declarations
# and link against the zeroed definitions.
target_include_directories(pokered PRIVATE src)
target_include_directories(pokered PRIVATE ${CMAKE_SOURCE_DIR}/third_party/ntsc-crt)
# CRT_SYSTEM_SNES (3). crt_core.h picks the signal profile from this, and a
# wrong value quietly compiles another machine's line timing, so it is set once
# here rather than in any source file.
target_compile_definitions(pokered PRIVATE CRT_SYSTEM=3)
if(RED_ONLY)
    target_include_directories(pokered PRIVATE ${CMAKE_SOURCE_DIR}/stubs/crystal)
endif()
target_include_directories(pokered PRIVATE ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
target_link_libraries(pokered
    ${SDL2_LINK_TARGETS}
    ${MATH_LIBRARY}
)
# display_gl.c needs the base GL library for the GL 1.1 entry points it calls
# directly (glTexImage2D, glDrawArrays, glViewport). Everything newer is
# fetched through SDL_GL_GetProcAddress at runtime, so this is the only GL link
# dependency.
if(WIN32)
    target_link_libraries(pokered opengl32)
elseif(APPLE)
    # Cocoa for launcher_native_mac.m's NSOpenPanel, the file browser's "..."
    # button.
    target_link_libraries(pokered "-framework OpenGL" "-framework Cocoa")
else()
    # GLVND splits the old libGL into libOpenGL + libGLX, and the legacy
    # OPENGL_gl_LIBRARY variable comes back empty there even though
    # find_package reports success. The imported target resolves to the right
    # set on both GLVND and legacy systems, so prefer it.
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
    if(TARGET OpenGL::GL)
        target_link_libraries(pokered OpenGL::GL)
    elseif(OPENGL_FOUND)
        target_link_libraries(pokered ${OPENGL_LIBRARIES})
    endif()
endif()
if(Python3_FOUND)
    target_link_libraries(pokered ${Python3_LIBRARIES})
    # comdlg32 for launcher_native.c's GetOpenFileNameW, the "..." beside the
    # browser's path bar. launcher_browse.c is still the chooser on every
    # device; this is one extra way for a desktop pointer to name a file, and
    # it feeds the same out_path.
    if(WIN32)
        target_link_libraries(pokered comdlg32)
    endif()
endif()

if(MSVC)
  set(SDL2_VC_DLL "${SDL2_VC_ROOT}/lib/x64/SDL2.dll")
  if(EXISTS "${SDL2_VC_DLL}")
    add_custom_command(TARGET pokered POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${SDL2_VC_DLL}"
              "$<TARGET_FILE_DIR:pokered>/SDL2.dll"
    )
  endif()
endif()

# SDL2.dll beside the binary, so the game runs however it is started. Without
# the copy it only starts from a shell whose PATH already carries the DLL. The
# release bundles ship their own, so this covers the dev tree.
#
# The DLL is taken from wherever the SDL2 being linked against lives, so the
# binary cannot end up paired with another prefix's runtime.
if(WIN32 AND MINGW)
  # find_package(SDL2) may set either the SDL2::SDL2 target or the legacy
  # variables, so hints come from both. PATHS ENV PATH is the fallback.
  set(_sdl2_hints "")
  foreach(_d ${SDL2_INCLUDE_DIRS})
    get_filename_component(_p "${_d}" DIRECTORY)    # .../include/SDL2 -> .../include
    get_filename_component(_p "${_p}" DIRECTORY)    #                  -> prefix
    list(APPEND _sdl2_hints "${_p}")
  endforeach()
  foreach(_l ${SDL2_LIBRARIES})
    if(EXISTS "${_l}")                              # skips "-lSDL2"-style entries
      get_filename_component(_d "${_l}" DIRECTORY)
      get_filename_component(_p "${_d}" DIRECTORY)
      list(APPEND _sdl2_hints "${_d}" "${_p}")
    endif()
  endforeach()
  if(TARGET SDL2::SDL2)
    get_target_property(_imp SDL2::SDL2 IMPORTED_IMPLIB)
    if(NOT _imp)
      get_target_property(_imp SDL2::SDL2 IMPORTED_LOCATION)
    endif()
    if(_imp AND EXISTS "${_imp}")
      get_filename_component(_d "${_imp}" DIRECTORY)
      get_filename_component(_p "${_d}" DIRECTORY)
      list(APPEND _sdl2_hints "${_d}" "${_p}")
    endif()
  endif()
  find_file(SDL2_RUNTIME_DLL
    NAMES SDL2.dll
    HINTS ${_sdl2_hints}
    PATH_SUFFIXES bin
    PATHS ENV PATH
  )
  if(SDL2_RUNTIME_DLL)
    # Both: POST_BUILD only fires when the target relinks, so the
    # configure-time copy is what fixes an already-built tree.
    file(COPY "${SDL2_RUNTIME_DLL}" DESTINATION "${CMAKE_BINARY_DIR}")
    add_custom_command(TARGET pokered POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${SDL2_RUNTIME_DLL}"
              "$<TARGET_FILE_DIR:pokered>/SDL2.dll"
    )
    message(STATUS "SDL2.dll will be copied beside the binary: ${SDL2_RUNTIME_DLL}")
  else()
    message(WARNING "SDL2.dll not found to copy beside the binary. The game will\n"
                    "    only start from a shell whose PATH already has it. Started any\n"
                    "    other way, Windows reports \"SDL2.dll is missing\".")
  endif()
endif()

# Console for development, no console for a release.
#
# -mconsole gives a terminal window alongside the game. That is what you want
# while working, and wrong in front of a player: a released game opening a black
# console next to itself looks broken before it has drawn a frame. Release links
# -mwindows instead, which creates no console.
#
# main() is the entry point either way. The concern in the note at the top of
# this file is SDL2main, which redirects the entry to WinMain; we do not link
# it, and MinGW's own runtime provides the WinMain wrapper that calls main()
# under -mwindows. Nothing about main() changes here.
#
# The log is unaffected: main() redirects stdout to pokered_log.txt at startup,
# so a release build still records everything it always did, with no console for
# it to have gone to.
if(WIN32 AND MINGW)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(pokered PRIVATE -mwindows)
  else()
    target_link_options(pokered PRIVATE -mconsole)
  endif()
endif()

# Warnings
target_compile_options(pokered PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
    $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
    $<$<CONFIG:Debug>:$<$<C_COMPILER_ID:MSVC>:/Zi /Od>>
    $<$<CONFIG:Debug>:$<$<NOT:$<C_COMPILER_ID:MSVC>>:-g -O0>>
    $<$<CONFIG:Release>:$<$<C_COMPILER_ID:MSVC>:/O2>>
    $<$<CONFIG:Release>:$<$<NOT:$<C_COMPILER_ID:MSVC>>:-O2>>
)

# Test suite: no SDL2, platform calls come from tests/stubs.c.
set(TEST_GAME_SOURCES
    src/game/wram.c
    src/game/debug_trace.c
    src/game/overworld.c
    src/game/gbc_color.c
    src/data/gbc_palettes.c
    src/game/amberscript_test_stubs.c
    src/game/player.c
    src/game/warp.c
    src/game/anim.c
    src/game/music.c
    src/game/johto_music.c
    src/game/py_ai_bridge.c
    src/game/type_mod.c
    src/game/species_mod.c
    src/game/sprite_mod.c
    src/game/npc.c
    src/game/text.c
    src/game/naming_screen.c
    src/game/inventory.c
    src/game/bicycle.c
    src/game/fly_anim.c
    src/game/session_log.c
    src/game/party_menu.c
    src/game/town_map.c
    src/game/blackboard.c
    src/game/bag_list_choice.c
    src/game/prize_list_choice.c
    src/game/money_box.c
    src/game/coin_box.c
    src/game/fossil_popup.c
    src/game/mon_pic.c
    src/game/crystal_fade.c
    src/game/crystal_icon_anim.c
    src/game/gen2_resources.c
    src/game/summary_screen.c
    src/data/party_icon_data.c
    src/game/pokecenter.c
    src/game/pokemart.c
    src/data/base_stats.c
    src/data/moves_data.c
    src/data/type_chart.c
    # gTilesets and gMapTable are rebuilt at boot from assets.pak, so there is
    # no tileset_data.c or map_data.c to list.
    src/data/map_events_load.c
    src/data/move_anim_scripts.c
    src/data/move_anim_subanims.c
    src/data/move_anim_frameblocks.c
    src/data/move_anim_basecoords.c
    # gCutTreeAnimTiles comes from assets.pak, so there is no cut_anim_tiles.c.
    src/platform/assetpack.c
    ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION}/assetpack_bind.c
    src/data/trade_gfx.c
    src/game/trade.c
    src/data/font_data.c
    # The battle UI tiles come from assets.pak, as in the pokered target above.
    src/data/pokedex_tiles.c
    src/data/town_map_data.c
    # The sprite tables come from assets.pak: no sprite_data.c, player_sprite.c,
    # bike_sprite.c, seel_sprite.c, pokemon_sprites.c or fossil_sprites.c.
    src/game/battle/battle_damage.c
    src/game/battle/battle_effects.c
    src/game/battle/battle_core.c
    src/game/battle/battle_loop.c
    src/game/battle/battle_ai.c
    src/game/battle/battle_exp.c
    src/game/battle/battle_init.c
    src/game/battle/move_anim.c
    src/game/battle/battle_catch.c
    src/game/battle/battle_items.c
    src/game/battle/battle_switch.c
    src/game/battle/battle_trainer.c
    src/game/trainer_sight.c
    src/game/rival_starter.c
    src/game/badge.c
    src/game/rom_text.c
    src/platform/game_version.c
    src/game/gate_scripts.c
    src/game/gym_scripts.c
    src/game/viridian_gym_spinners.c
    src/game/fishing.c
    src/game/oakslab_scripts.c
    src/game/viridian_mart_scripts.c
    src/game/route24_scripts.c
    src/game/blues_house_scripts.c
    src/game/reds_house1f_scripts.c
    src/game/bills_house_scripts.c
    src/game/bills_pokemon_list.c
    src/game/badge_house_menu.c
    src/game/route25_scripts.c
    src/game/dex_rating.c
    src/game/pc_menu.c
    src/game/players_pc.c
    src/game/route2gate_scripts.c
    src/game/ss_anne_depart.c
    src/game/vermilion_gym_scripts.c
    src/game/cinnabar_gym_scripts.c
    src/game/name_rater_scripts.c
    src/game/daycare.c
    src/game/poison.c
    src/game/yesno.c
    src/game/bikeshop_menu.c
    src/game/tmhm.c
    src/game/field_moves.c
    src/game/escape_anim.c
    src/game/pokeflute.c
    src/game/rockethideout_b4f_scripts.c
    src/game/rockethideout_scripts.c
    src/game/game_corner_scripts.c
    src/game/pokemontower6f_scripts.c
    src/game/pokemontower7f_scripts.c
    src/game/mrfujis_house_scripts.c
    src/game/celadon_city_scripts.c
    src/game/route1_scripts.c
    src/game/saffron_city_scripts.c
    src/game/seafoam_scripts.c
    src/game/victory_road_scripts.c
    src/game/elite_four_scripts.c
    src/game/champions_room_scripts.c
    src/game/hall_of_fame_scripts.c
    src/game/credits_scripts.c
    src/game/safari_zone_scripts.c
    src/game/safari_zone_secret_house_scripts.c
    src/game/wardens_house_scripts.c
    src/game/mansion_scripts.c
    src/game/cinnabar_island_scripts.c
    src/game/itemfinder.c
    src/game/elevator_menu.c
    src/game/pokedex.c
    src/game/pallet_scripts.c
    src/game/pokemon.c
    # The Gen 2 species layer. pokemon.c and battle_ui.c, both already in this
    # list, call Species_GetBaseStats and Species_Dex unconditionally, so
    # leaving this out is a link error, not a harness missing Johto mon.
    src/game/gen2_species.c
    # The generated Crystal data, swapped for zeroed stubs under RED_ONLY
    # exactly as the main SOURCES list does.
    ${CRYSTAL_DATA_SOURCES}
    src/data/dex_data.c
    src/data/trainer_sprites.c
    # The Crystal presentation and logic files the *_scripts.c and battle_ui.c
    # sources above call unconditionally: map music restart, slot machine, the
    # battle-UI ghost sprite, Gen 2 pack/pic/trainer graphics. Nothing stubs
    # these, unlike the AmberScript tile and mapbank surface, so they have to
    # be real.
    ${CRYSTAL_DATA_SOURCES}
    src/data/slots_gfx.c
    src/game/slot_machine.c
    src/game/battle/battle_ui.c
    src/game/map_music.c
    src/game/crystal_color.c
    src/game/crystal_tile_anim.c
    src/game/crystal_grass_rustle.c
    src/game/gen1color/crystal_pic_anim.c
)
set(TEST_SOURCES
    tests/main.c
    tests/test_runner.c
    tests/stubs.c
    tests/test_map.c
    tests/test_warp.c
    tests/test_player.c
    tests/test_text.c
    tests/test_wild.c
    tests/test_battle_damage.c
    tests/test_battle_effects.c
    tests/test_battle_core.c
    tests/test_battle_loop.c
    tests/test_battle_ai.c
    tests/test_battle_catch.c
    tests/test_battle_items.c
    tests/test_battle_switch.c
    tests/test_battle_trainer.c
    tests/test_battle_init.c
    tests/test_move_anim.c
    tests/test_pokemon_storage.c
)
# The developer targets below need the real Crystal data: the test source set
# pulls in gen2_pokedex.c and gen2_evos_moves.c, which Species_GetBaseStats
# resolves against. RED_ONLY does not define them at all, rather than build
# them against zeroed tables and let them report nonsense.
if(NOT RED_ONLY)

add_executable(pokered_tests ${TEST_GAME_SOURCES} ${TEST_SOURCES})
target_include_directories(pokered_tests PRIVATE src tests ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
target_compile_options(pokered_tests PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
    $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
    $<$<CONFIG:Debug>:$<$<C_COMPILER_ID:MSVC>:/Zi /Od>>
    $<$<CONFIG:Debug>:$<$<NOT:$<C_COMPILER_ID:MSVC>>:-g -O0>>
)
# anim_harness, headless move-animation driver for the debug suite.
# Reuses the test source set and stubs: the move-animation VM is a pure
# function of (animation_id, hWhoseTurn, animation_type) over WRAM, so it runs
# with no battle, no SDL and no game loop. Emits the per-frame OAM trace that
# tools/debugsuite/animdiff.py compares against the golden trace.
add_executable(anim_harness ${TEST_GAME_SOURCES} tests/stubs.c tools/anim_harness/main.c)
target_include_directories(anim_harness PRIVATE src tests ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
target_compile_options(anim_harness PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
    $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
)


# evo_harness, headless driver for the post-battle/field evolution screen
# (BUI_EVOLUTION in battle_ui.c). Diagnostic tool, tools/evo_harness/main.c.
add_executable(evo_harness ${TEST_GAME_SOURCES}
               src/game/bag_menu.c
               tests/stubs.c
               tools/battle_harness/stubs.c tools/evo_harness/main.c)
target_include_directories(evo_harness PRIVATE src tests ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
target_compile_definitions(evo_harness PRIVATE BATTLE_UI_REAL)
target_compile_options(evo_harness PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
    $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
)

endif()  # NOT RED_ONLY, developer targets that need the real Crystal data

# trainer_ai_harness sits outside the NOT RED_ONLY block on purpose: a Gen 1
# battle never reaches a Johto path, so the zeroed Crystal stubs are never
# consulted and the harness still tells the truth in the Red standalone, which
# is the checkout the battle is played in.
# trainer_ai_harness, headless verification of the trainer AI logic.
# Drives full multi-turn trainer battles through Battle_RunTurn (which runs both
# AI systems) with no SDL and no battle_ui. See src/game/battle/battle_ai.c.
#
# Guarded on its source being present, as are ai_harness and assetpack_harness
# below. tools/dist/export_release.sh leaves the development harnesses out of
# the exported tree, and a target naming a file that does not exist is a hard
# CMake configure error rather than a skipped target. The guard has to wrap the
# target_* calls too, since they name a target that would not exist.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/trainer_ai_harness/main.c")
    add_executable(trainer_ai_harness ${TEST_GAME_SOURCES} tests/stubs.c
                   src/game/speed_settings.c
                   tools/trainer_ai_harness/red_stubs.c
                   tools/trainer_ai_harness/main.c)
    target_include_directories(trainer_ai_harness PRIVATE src tests)
    if(RED_ONLY)
        # stubs/crystal ahead of generated/, same as the pokered target: those
        # are committed mirrors of the gitignored Crystal headers, so the shared
        # engine files that #include "crystal_audio.h" and friends resolve.
        target_include_directories(trainer_ai_harness PRIVATE ${CMAKE_SOURCE_DIR}/stubs/crystal)
    endif()
    target_include_directories(trainer_ai_harness PRIVATE ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
    target_compile_options(trainer_ai_harness PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
        $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
    )
endif()


if(NOT RED_ONLY)
# battle_harness, headless driver for the battle presentation layer.
# Emits the same JSONL event stream as tools/debugsuite/btrace_rom.py, which
# records the byte-exact ROM. See tools/debugsuite/BATTLE_TRACE.md.
#
# It stays inside the NOT RED_ONLY block: TEST_GAME_SOURCES contains
# overworld.c, which includes johto_music.h, which includes the generated
# crystal_audio.h. A missing header is not something the data-stub swap covers,
# so in a Red standalone this fails to compile as part of `all`, taking the
# game build with it.
#
# battle_ui.c is already in TEST_GAME_SOURCES. Do not re-list it here: CMake
# would compile it twice into the same executable and every symbol in it would
# come back as a duplicate definition.
add_executable(battle_harness ${TEST_GAME_SOURCES}
               src/game/bag_menu.c
               tests/stubs.c   # ghost_front_sprite.c comes from assets.pak
               tools/battle_harness/stubs.c tools/battle_harness/main.c)
target_include_directories(battle_harness PRIVATE src tests ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
target_compile_definitions(battle_harness PRIVATE BATTLE_UI_REAL)
target_compile_options(battle_harness PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
    $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
)
endif()  # NOT RED_ONLY, battle_harness needs generated/crystal_audio.h


# ai_harness, deterministic driver for the trainer move-choice AI.
#
# Outside the NOT RED_ONLY block on purpose: it links battle_ai.c, the asset
# pack and the type chart, none of which need Crystal data, so unlike
# battle_harness it exists in the Red standalone too. See tools/ai_harness/main.c.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/ai_harness/main.c")
    add_executable(ai_harness
                   src/game/battle/battle_ai.c
                   src/data/type_chart.c
                   src/platform/assetpack.c
                   generated/${GAME_VERSION}/assetpack_bind.c
                   tools/ai_harness/stubs.c
                   tools/ai_harness/main.c)
    target_include_directories(ai_harness PRIVATE src src/game ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
    target_compile_options(ai_harness PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
        $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
    )
endif()

# assetpack_harness, pins the asset pack's two implementations together.
# The binary layout is written by tools/assetpack/pak.py and read by
# src/platform/assetpack.c; two implementations of one format drift silently, so
# this builds a pack in Python and reads it back in C. Also self-tests SHA-1,
# without which the ROM hash gate would be decorative.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/assetpack_harness/main.c")
    add_executable(assetpack_harness
                   src/platform/assetpack.c
                   tools/assetpack_harness/main.c)
    target_include_directories(assetpack_harness PRIVATE src)
    target_compile_options(assetpack_harness PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
        $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
    )
endif()

if(NOT RED_ONLY)
add_executable(pic_harness
               src/game/gen1color/crystal_pic_anim.c
               generated/crystal_mon_pics.c
               tools/pic_harness/main.c)
target_include_directories(pic_harness PRIVATE src src/game ${CMAKE_SOURCE_DIR}/generated/${GAME_VERSION})
target_compile_options(pic_harness PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers>
    $<$<C_COMPILER_ID:GNU>:-include ${CMAKE_SOURCE_DIR}/src/platform/no_avx512.h>
)

endif()  # NOT RED_ONLY, pic_harness drives Crystal front-pic animation

# The CRT renderer's unit tests: pure CPU maths, descriptor validation,
# viewport derivation and persistence coefficients, with no Crystal data and no
# GL. They build under RED_ONLY too, unlike pokered_tests.
add_executable(crt_renderer_tests
    tests/crt_renderer_tests.c
    src/platform/crt_renderer.c
)
target_include_directories(crt_renderer_tests PRIVATE src)
target_compile_options(crt_renderer_tests PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)
if(MATH_LIBRARY)
    target_link_libraries(crt_renderer_tests PRIVATE ${MATH_LIBRARY})
endif()

# The CRT measurement harness (tests/crt_harness.c). Defined under RED_ONLY
# too: it needs no Crystal data and no game state, only a GL context, the
# shaders on disk and the renderer itself. It renders with the real shaders on
# the real GPU and reads the pixels back.
#
# Not linked against display_gl.c: the harness supplies its own
# DisplayGL_ShaderDir, so it does not drag in the rest of the presentation
# stack.
add_executable(crt_harness
    tests/crt_harness.c
    src/platform/crt_renderer.c
    src/platform/crt_renderer_gl.c
    src/platform/gl_api.c
)
target_include_directories(crt_harness PRIVATE src)
target_compile_options(crt_harness PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)
target_link_libraries(crt_harness ${SDL2_LINK_TARGETS})
if(WIN32)
    target_link_libraries(crt_harness opengl32)
elseif(APPLE)
    target_link_libraries(crt_harness "-framework OpenGL")
else()
    find_package(OpenGL)
    if(OPENGL_FOUND)
        target_link_libraries(crt_harness ${OPENGL_gl_LIBRARY})
    endif()
endif()
if(MATH_LIBRARY)
    target_link_libraries(crt_harness ${MATH_LIBRARY})
endif()
# Same reason as pokered: SDL2main wants WinMain, and this is a console tool.
if(WIN32 AND MINGW)
    target_link_options(crt_harness PRIVATE -mconsole)
endif()

# CTest integration. pokered_tests only exists in a full build, so registering
# it under RED_ONLY would create a test that cannot be run.
if(NOT RED_ONLY)
enable_testing()
add_test(NAME pokered_tests COMMAND pokered_tests)
endif()

# ui_harness, renders the settings UI to an image with no game and no window.
#
# Outside any RED_ONLY guard, and it links two source files: ui_draw.c takes no
# SDL types and launcher_font.c is a static glyph table, so this needs no
# Crystal data, no SDL, no GL context and no ROM. Every screen is inspectable
# as a picture before it reaches a device.
#
# Declared only if its source is present: the release export leaves development
# tooling out of the exported tree (tools/dist/export_release.sh), and a target
# naming a file that does not exist is a hard CMake configure error.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/ui_harness/main.c")
    add_executable(ui_harness
        src/platform/ui_draw.c
        src/platform/launcher_font.c
        tools/ui_harness/main.c)
    target_include_directories(ui_harness PRIVATE src src/platform)
    if(WIN32 AND MINGW)
        target_link_options(ui_harness PRIVATE -mconsole)
    endif()
endif()
