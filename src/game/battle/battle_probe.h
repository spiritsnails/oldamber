
#ifndef BATTLE_PROBE_H
#define BATTLE_PROBE_H

extern void (*gBattleProbeHook)(const char *routine);

#define BPROBE(name) do {                    \
        if (gBattleProbeHook)                \
            gBattleProbeHook(name);          \
    } while (0)

#endif
