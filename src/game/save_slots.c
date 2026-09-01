
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include "save_slots.h"
#include "overworld.h"
#include "amberscript_mapbank.h"
#include "amberscript_saveops.h"
#include "types.h"
#include "../platform/save.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "../platform/game_version.h"
#include "../platform/data_dir.h"
#include "../platform/launcher_draw.h"

static const char *slots_dir(void) {
    static char dir[1200];
    char relative[64];
    snprintf(relative, sizeof relative, "states/%s", GameVersion_Current());
    if (!UserDataPath(relative, dir, sizeof dir))
        snprintf(dir, sizeof dir, "%s", relative);
    return dir;
}

static void slot_path(int slot, const char *ext, char *out, size_t n) {
    snprintf(out, n, "%s/slot%d.%s", slots_dir(), slot + 1, ext);
}

#define META_MAGIC   0x4D534B50u
#define META_VERSION 1u

typedef struct PACKED {
    uint32_t magic;
    uint32_t version;
    int64_t  when;
    char     map[24];
    uint8_t  badges;
    uint8_t  party_count;
    uint8_t  party_level[6];
    uint16_t thumb_w, thumb_h;

} slot_meta_t;

typedef struct {
    save_slot_info_t info;
    uint32_t         thumb[SAVE_SLOT_THUMB_MAX];
    int              valid;
    int64_t          stamp;
} slot_cache_t;

static slot_cache_t s_cache[SAVE_SLOT_COUNT];

static int64_t file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (int64_t)st.st_mtime;
}

static void load_meta(int slot, save_slot_info_t *info, uint32_t *thumb) {
    char path[1400];
    slot_meta_t m;
    FILE *f;
    size_t px;

    slot_path(slot, "meta", path, sizeof path);
    f = fopen(path, "rb");
    if (!f) return;
    if (fread(&m, 1, sizeof m, f) != sizeof m ||
        m.magic != META_MAGIC || m.version != META_VERSION) {
        fclose(f);
        return;
    }
    info->when        = m.when;
    info->badges      = m.badges;
    info->party_count = m.party_count;
    memcpy(info->party_level, m.party_level, sizeof info->party_level);
    snprintf(info->map, sizeof info->map, "%s", m.map);

    px = (size_t)m.thumb_w * m.thumb_h;
    if (px > 0 && px <= SAVE_SLOT_THUMB_MAX &&
        fread(thumb, sizeof(uint32_t), px, f) == px) {
        info->thumb_w = m.thumb_w;
        info->thumb_h = m.thumb_h;
        info->thumb   = thumb;
    }
    fclose(f);
}

const save_slot_info_t *SaveSlots_Info(int slot) {
    char spath[1400];
    slot_cache_t *c;
    int64_t mt;

    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return NULL;
    c = &s_cache[slot];
    slot_path(slot, "state", spath, sizeof spath);
    mt = file_mtime(spath);

    if (c->valid && c->stamp == mt) return &c->info;

    memset(&c->info, 0, sizeof c->info);
    c->stamp = mt;
    c->valid = 1;

    if (mt == 0) {
        c->info.err = SAVE_STATE_ERR_EMPTY;
        return &c->info;
    }
    c->info.occupied = 1;
    c->info.err      = Save_StatePeek(spath, NULL);
    c->info.readable = (c->info.err == SAVE_STATE_ERR_NONE);
    load_meta(slot, &c->info, c->thumb);
    return &c->info;
}

static int popcount8(uint8_t v) {
    int n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
}

int SaveSlots_Write(int slot) {
    extern int Game_SceneHasMap(void);
    char spath[1400], mpath[1400], tmp[1420];
    slot_meta_t m;
    static uint32_t thumb[SAVE_SLOT_THUMB_MAX];
    int fw, th, i;
    FILE *f;

    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return -1;

    if (!Game_SceneHasMap()) return -1;

    {
        char states[1200];
        if (!UserDataPath("states", states, sizeof states))
            snprintf(states, sizeof states, "states");
        LauncherDraw_EnsureDir(states);
    }
    LauncherDraw_EnsureDir(slots_dir());

    slot_path(slot, "state", spath, sizeof spath);
    if (Save_StateWrite(spath) != 0) return -1;

    memset(&m, 0, sizeof m);
    m.magic   = META_MAGIC;
    m.version = META_VERSION;
    m.when    = (int64_t)time(NULL);
    m.badges  = (uint8_t)popcount8(wObtainedBadges);

    {
        const char *name = AmberScript_MapBank_NameForRealId(Map_CurrentRealId());
        snprintf(m.map, sizeof m.map, "%s", name ? name : "");
    }

    m.party_count = wPartyCount > 6 ? 6 : wPartyCount;
    for (i = 0; i < m.party_count; i++) m.party_level[i] = wPartyMons[i].level;

    fw = Display_FrameWidth();
    if (fw < 1) fw = 160;
    if (fw > SAVE_SLOT_THUMB_W) fw = SAVE_SLOT_THUMB_W;
    th = SAVE_SLOT_THUMB_H;
    memset(thumb, 0, (size_t)fw * th * sizeof(uint32_t));
    Display_BlitGameFrameTo(thumb, fw, th, 0, 0, fw, th);
    m.thumb_w = (uint16_t)fw;
    m.thumb_h = (uint16_t)th;

    slot_path(slot, "meta", mpath, sizeof mpath);
    snprintf(tmp, sizeof tmp, "%s.tmp", mpath);
    f = fopen(tmp, "wb");
    if (f) {
        size_t px = (size_t)m.thumb_w * m.thumb_h;
        int ok = (fwrite(&m, 1, sizeof m, f) == sizeof m) &&
                 (fwrite(thumb, sizeof(uint32_t), px, f) == px);
        if (fclose(f) != 0) ok = 0;
        if (ok) { remove(mpath); rename(tmp, mpath); }
        else    { remove(tmp); }
    }

    s_cache[slot].valid = 0;
    return 0;
}

int SaveSlots_Read(int slot) {
    char spath[1400];
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return -1;
    slot_path(slot, "state", spath, sizeof spath);
    if (Save_StateLoad(spath) != 0) return -1;

    AmberScript_ReloadAfterStateLoad();
    return 0;
}

int SaveSlots_Delete(int slot) {
    char spath[1400], mpath[1400];
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return -1;
    slot_path(slot, "state", spath, sizeof spath);
    slot_path(slot, "meta",  mpath, sizeof mpath);
    remove(spath);
    remove(mpath);
    s_cache[slot].valid = 0;
    return 0;
}

const char *SaveSlots_When(int slot) {
    static char buf[32];
    const save_slot_info_t *in = SaveSlots_Info(slot);
    time_t now, t;
    long mins;

    if (!in || !in->occupied) return "EMPTY";
    if (in->when == 0) return "";

    now = time(NULL);
    t   = (time_t)in->when;
    mins = (long)((now - t) / 60);

    if (mins < 1)      snprintf(buf, sizeof buf, "JUST NOW");
    else if (mins < 60) snprintf(buf, sizeof buf, "%ld MIN AGO", mins);
    else if (mins < 24 * 60) snprintf(buf, sizeof buf, "%ld HR AGO", mins / 60);
    else {
        struct tm *tm = localtime(&t);
        if (tm) strftime(buf, sizeof buf, "%d %b %H:%M", tm);
        else    snprintf(buf, sizeof buf, "OLD");
    }
    return buf;
}
