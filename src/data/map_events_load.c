
#include <stdint.h>
#include <stdio.h>
#include "event_data.h"
#include "map_data.h"
#include "../platform/assetpack.h"

#define MAPEV_MAX_WARPS    1024
#define MAPEV_MAX_SIGNS     384
#define MAPEV_MAX_NPCS     1024
#define MAPEV_MAX_ITEMS     256
#define MAPEV_MAX_TRAINERS  512

static map_warp_t    s_warps[MAPEV_MAX_WARPS];
static sign_event_t  s_signs[MAPEV_MAX_SIGNS];
static npc_event_t   s_npcs[MAPEV_MAX_NPCS];
static item_event_t  s_items[MAPEV_MAX_ITEMS];
static map_trainer_t s_trainers[MAPEV_MAX_TRAINERS];

map_events_t gMapEvents[NUM_MAPS];

#define TEXT_NONE 0xFFFFFFFFu

static const char *s_text_base;

static const char *pack_text(uint32_t off)
{
    return (off == TEXT_NONE) ? NULL : s_text_base + off;
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void MapEvents_LoadFromPack(void)
{
    uint32_t n_off = 0, n_blob = 0;
    const uint8_t *blob = (const uint8_t *)AssetPack_Require("gMapEventBlob",
                                                             &n_blob);
    const uint8_t *offs = (const uint8_t *)AssetPack_Require("gMapEventOffsets",
                                                             &n_off);
    s_text_base = (const char *)AssetPack_Require("gMapEventText", NULL);

    int nw = 0, ns = 0, nn = 0, ni = 0, nt = 0;

    for (int mid = 0; mid < NUM_MAPS; mid++) {
        map_events_t *ev = &gMapEvents[mid];
        uint16_t o;

        if ((uint32_t)(mid * 2 + 1) >= n_off) break;
        o = rd16(offs + mid * 2);
        if (o == 0xFFFFu) continue;

        const uint8_t *p = blob + o;
        ev->border_block = *p++;

        int n = *p++;
        ev->warps = &s_warps[nw];
        ev->num_warps = (uint8_t)n;
        for (int i = 0; i < n; i++, p += 4) {
            if (nw >= MAPEV_MAX_WARPS) { printf("[mapev] warp overflow\n"); return; }
            s_warps[nw].x = p[0];
            s_warps[nw].y = p[1];
            s_warps[nw].dest_map = p[2];
            s_warps[nw].dest_warp_idx = p[3];
            nw++;
        }

        n = *p++;
        ev->signs = &s_signs[ns];
        ev->num_signs = (uint8_t)n;
        for (int i = 0; i < n; i++, p += 6) {
            if (ns >= MAPEV_MAX_SIGNS) { printf("[mapev] sign overflow\n"); return; }
            s_signs[ns].x = p[0];
            s_signs[ns].y = p[1];
            s_signs[ns].text = pack_text(rd32(p + 2));
            ns++;
        }

        n = *p++;
        ev->npcs = &s_npcs[nn];
        ev->num_npcs = (uint8_t)n;
        for (int i = 0; i < n; i++, p += 9) {
            if (nn >= MAPEV_MAX_NPCS) { printf("[mapev] npc overflow\n"); return; }
            npc_event_t *e = &s_npcs[nn];
            e->x = p[0];
            e->y = p[1];
            e->sprite_id = p[2];
            e->movement = p[3];
            e->text = pack_text(rd32(p + 5));

            e->script = NULL;
            e->facing = 0;
            e->src_idx = (uint8_t)i;
            e->crystal_pal = 0;
            nn++;
        }

        n = *p++;
        ev->items = &s_items[ni];
        ev->num_items = (uint8_t)n;
        for (int i = 0; i < n; i++, p += 3) {
            if (ni >= MAPEV_MAX_ITEMS) { printf("[mapev] item overflow\n"); return; }
            s_items[ni].x = p[0];
            s_items[ni].y = p[1];
            s_items[ni].item_id = p[2];
            s_items[ni].src_idx = (uint8_t)i;
            ni++;
        }

        n = *p++;
        ev->trainers = &s_trainers[nt];
        ev->num_trainers = (uint8_t)n;
        for (int i = 0; i < n; i++, p += 15) {
            if (nt >= MAPEV_MAX_TRAINERS) { printf("[mapev] trainer overflow\n"); return; }
            s_trainers[nt].npc_idx = p[0];
            s_trainers[nt].trainer_class = p[1];
            s_trainers[nt].trainer_no = p[2];
            s_trainers[nt].before_text = pack_text(rd32(p + 3));
            s_trainers[nt].after_text  = pack_text(rd32(p + 7));
            s_trainers[nt].end_text    = pack_text(rd32(p + 11));
            nt++;
        }

        ev->hidden_events = NULL;
        ev->num_hidden_events = 0;
    }

    printf("[mapev] %d warps, %d signs, %d npcs, %d items, %d trainers "
           "from the pack\n", nw, ns, nn, ni, nt);
    fflush(stdout);
}
