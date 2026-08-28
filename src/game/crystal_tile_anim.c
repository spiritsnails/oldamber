
#include "crystal_tile_anim.h"
#include "amberscript_tilemod.h"
#include "crystal_tile_anims.h"
#include <stdio.h>
#include <string.h>

#define TILE_BYTES 16

#define VRAM_BANK0_TILES 96
#define VRAM_GAP_TILES   32

static int   s_script = -1;
static char  s_slug[32];
static int   s_cursor = 0;
static uint8_t s_timer = 0;
static uint8_t s_buffer[TILE_BYTES];

static uint32_t s_warned = 0;

void CrystalTileAnim_Reset(void) {
    s_cursor = 0;
    s_timer = 0;
    s_warned = 0;
    memset(s_buffer, 0, sizeof(s_buffer));
}

void CrystalTileAnim_SetTileset(int tileset_id, const char *slug) {
    int script;
    if (tileset_id < 0 || tileset_id >= CRYSTAL_NUM_TILESETS || !slug || !*slug) {
        s_script = -1;
        s_slug[0] = '\0';
        CrystalTileAnim_Reset();
        return;
    }
    script = gCrystalTilesetAnim[tileset_id];
    if (script < 0 || script >= CRYSTAL_ANIM_NUM_SCRIPTS) {
        s_script = -1;
        s_slug[0] = '\0';
        CrystalTileAnim_Reset();
        return;
    }
    if (s_script == script && strcmp(s_slug, slug) == 0) return;
    s_script = script;
    snprintf(s_slug, sizeof(s_slug), "%s", slug);
    CrystalTileAnim_Reset();
}

static int subtile_name(uint16_t vram_tile, char *out, size_t cap) {
    int gfx;
    if (vram_tile < VRAM_BANK0_TILES) {
        gfx = vram_tile;
    } else if (vram_tile >= VRAM_BANK0_TILES + VRAM_GAP_TILES) {
        gfx = vram_tile - VRAM_GAP_TILES;
    } else {
        return 0;
    }
    snprintf(out, cap, "%s_t%03d", s_slug, gfx);
    return 1;
}

static void write_tile(uint16_t vram_tile, const uint8_t *pixels) {
    char name[48];
    if (!subtile_name(vram_tile, name, sizeof(name))) return;

    AmberScript_SubtileBlitPixels(name, pixels);
}

static void run_frame(const crystal_anim_frame_t *f) {
    switch (f->routine) {

    case CRYSTAL_ANIM_WAIT_TILE_ANIMATION:
        break;

    case CRYSTAL_ANIM_DONE_TILE_ANIMATION:

        s_cursor = -1;
        break;

    case CRYSTAL_ANIM_STANDING_TILE_FRAME:
    case CRYSTAL_ANIM_STANDING_TILE_FRAME8:
        s_timer = (uint8_t)((s_timer + 1) & 7);
        break;

    case CRYSTAL_ANIM_ANIMATE_WATER_TILE:
        write_tile(f->arg, gCrystalAnimWater[(s_timer & 6) >> 1]);
        break;

    case CRYSTAL_ANIM_ANIMATE_FLOWER_TILE:

        write_tile(0x03, gCrystalAnimFlower[(s_timer & 2) + 1]);
        break;

    case CRYSTAL_ANIM_ANIMATE_LAVA_BUBBLE_TILE1:
        write_tile(0x5B, gCrystalAnimLava[(((s_timer & 6) >> 1) + 2) & 3]);
        break;
    case CRYSTAL_ANIM_ANIMATE_LAVA_BUBBLE_TILE2:
        write_tile(0x38, gCrystalAnimLava[(s_timer & 6) >> 1]);
        break;

    case CRYSTAL_ANIM_ANIMATE_FOUNTAIN_TILE:
        write_tile(f->arg, gCrystalAnimFountain[s_timer & 7]);
        break;

    case CRYSTAL_ANIM_ANIMATE_WHIRLPOOL_TILE: {

        const crystal_anim_descriptor_t *d;
        if (f->arg_kind != 3 || f->arg >= CRYSTAL_ANIM_NUM_DESCRIPTORS) break;
        d = &gCrystalAnimDescriptors[f->arg];
        if (!d->num_frames) break;
        write_tile(d->dest_tile, d->frames[s_timer & 3]);
        break;
    }

    case CRYSTAL_ANIM_ANIMATE_TOWER_PILLAR_TILE: {

        const crystal_anim_descriptor_t *d;
        uint8_t frame;
        if (f->arg_kind != 3 || f->arg >= CRYSTAL_ANIM_NUM_DESCRIPTORS) break;
        d = &gCrystalAnimDescriptors[f->arg];
        frame = gCrystalAnimPillarSeq[s_timer & 7];
        if (frame >= d->num_frames) break;
        write_tile(d->dest_tile, d->frames[frame]);
        break;
    }

    case CRYSTAL_ANIM_FOREST_TREE_LEFT_ANIMATION:
    case CRYSTAL_ANIM_FOREST_TREE_LEFT_ANIMATION2:
        write_tile(0x0C, gCrystalAnimForestLeft[0]);
        break;
    case CRYSTAL_ANIM_FOREST_TREE_RIGHT_ANIMATION:
    case CRYSTAL_ANIM_FOREST_TREE_RIGHT_ANIMATION2:
        write_tile(0x0F, gCrystalAnimForestRight[0]);
        break;

    case CRYSTAL_ANIM_READ_TILE_TO_ANIM_BUFFER: {
        char name[48];
        if (subtile_name(f->arg, name, sizeof(name)))
            AmberScript_SubtileReadPixels(name, s_buffer);
        break;
    }
    case CRYSTAL_ANIM_WRITE_TILE_FROM_ANIM_BUFFER:
        write_tile(f->arg, s_buffer);
        break;
    case CRYSTAL_ANIM_SCROLL_TILE_DOWN: {

        uint8_t last[2] = { s_buffer[14], s_buffer[15] };
        memmove(s_buffer + 2, s_buffer, TILE_BYTES - 2);
        s_buffer[0] = last[0];
        s_buffer[1] = last[1];
        break;
    }
    case CRYSTAL_ANIM_SCROLL_TILE_RIGHT_LEFT: {

        int right;
        s_timer = (uint8_t)((s_timer + 1) & 7);
        right = !(s_timer & 4);
        for (int i = 0; i < TILE_BYTES; i++) {
            uint8_t v = s_buffer[i];
            s_buffer[i] = right ? (uint8_t)((v >> 1) | (v << 7))
                                : (uint8_t)((v << 1) | (v >> 7));
        }
        break;
    }

    case CRYSTAL_ANIM_ANIMATE_WATER_PALETTE:
    case CRYSTAL_ANIM_FLICKERING_CAVE_ENTRANCE_PALETTE:
        break;

    default:
        if (f->routine < 32 && !(s_warned & (1u << f->routine))) {
            s_warned |= 1u << f->routine;
            printf("[crystal_anim] routine %d has no implementation yet "
                   "(script %d, %s)\n", f->routine, s_script, s_slug);
        }
        break;
    }
}

void CrystalTileAnim_Tick(void) {
    const crystal_anim_script_t *sc;
    if (s_script < 0) return;
    sc = &gCrystalAnimScripts[s_script];
    if (!sc->count) return;
    if (s_cursor < 0 || s_cursor >= sc->count) s_cursor = 0;
    run_frame(&gCrystalAnimFrames[sc->first + s_cursor]);

    s_cursor = (s_cursor < 0) ? 0 : (s_cursor + 1) % sc->count;
}
