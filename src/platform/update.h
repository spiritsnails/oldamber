#pragma once

#include <stddef.h>

typedef enum {
    UPDATE_DISABLED,
    UPDATE_IDLE,
    UPDATE_CHECKING,
    UPDATE_CURRENT,
    UPDATE_AVAILABLE,
    UPDATE_DOWNLOADING,
    UPDATE_READY,
    UPDATE_ERROR
} update_state_t;

typedef struct {
    update_state_t state;
    char version[32];
    char message[160];
} update_snapshot_t;

void Update_Init(void);
void Update_Shutdown(void);
void Update_GetSnapshot(update_snapshot_t *out);
void Update_Check(void);
void Update_Install(void);
