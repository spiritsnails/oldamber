
#include "crystal_pic_anim.h"
#include "crystal_mon_pics.h"

#define ANIM_END       0xFF
#define ANIM_SETREPEAT 0xFE
#define ANIM_DOREPEAT  0xFD

#define ST_RUN  0
#define ST_WAIT 1

static struct {
    int      dex;
    uint16_t cmd_index;
    uint16_t base;
    uint8_t  state;
    uint8_t  wait;
    uint8_t  repeat;
    uint8_t  speed;
    uint8_t  frame;
    uint8_t  running;
    uint8_t  owner;
} s;

static uint8_t anim_duration(uint8_t a, uint8_t speed) {
    uint16_t hl = (uint16_t)a * speed;
    return (uint8_t)(((hl >> 4) & 0xFF) + a);
}

void CrystalPicAnim_Stop(void) {
    s.running = 0;
    s.frame = 0;
    s.dex = 0;
    s.owner = 0;
}

void CrystalPicAnim_StopOwner(int owner) {
    if (s.running && s.owner != (uint8_t)owner) return;
    CrystalPicAnim_Stop();
}

void CrystalPicAnim_Start(int dex, int speed) {
    CrystalPicAnim_StartOwner(CRYSTAL_ANIM_OWNER_BATTLE, dex, speed);
}

void CrystalPicAnim_StartOwner(int owner, int dex, int speed) {
    CrystalPicAnim_Stop();
    if (dex <= 0 || dex >= CRYSTAL_MON_COUNT) return;
    const crystal_pic_t *p = &gCrystalMonPic[dex];

    if (p->idle_base <= p->anim_base) return;

    s.dex = dex;
    s.base = p->anim_base;
    s.cmd_index = 0;
    s.state = ST_RUN;
    s.wait = 0;
    s.repeat = 0;
    s.speed = (uint8_t)speed;
    s.frame = 0;
    s.running = 1;
    s.owner = (uint8_t)owner;
}

int CrystalPicAnim_Running(void) { return s.running; }

void CrystalPicAnim_Tick(void) {
    if (!s.running) return;

    for (int guard = 0; guard < 64; guard++) {
        if (s.state == ST_WAIT) {

            if (--s.wait != 0) return;
            s.state = ST_RUN;
            return;
        }

        uint16_t at = s.base + s.cmd_index;
        if (at >= CRYSTAL_PIC_ANIM) { CrystalPicAnim_Stop(); return; }
        uint8_t cmd = gCrystalPicAnim[at][0];
        uint8_t param = gCrystalPicAnim[at][1];
        s.cmd_index++;

        if (cmd == ANIM_END) {

            CrystalPicAnim_Stop();
            return;
        }
        if (cmd == ANIM_SETREPEAT) {
            s.repeat = param;
            continue;
        }
        if (cmd == ANIM_DOREPEAT) {

            if (s.repeat == 0) return;
            if (--s.repeat == 0) return;
            s.cmd_index = param;
            continue;
        }

        s.frame = cmd;
        s.wait = anim_duration(param, s.speed);
        s.state = ST_WAIT;

        if (--s.wait != 0) return;
        s.state = ST_RUN;
        return;
    }
    CrystalPicAnim_Stop();
}

const uint8_t *CrystalPicAnim_FrameMap(int dex) {
    if (!s.running || dex != s.dex || s.frame == 0) return 0;
    const crystal_pic_t *p = &gCrystalMonPic[dex];
    if (s.frame > p->frame_count) return 0;
    return gCrystalPicFrames[p->frame_base + s.frame - 1];
}
