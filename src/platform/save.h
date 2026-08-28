#pragma once
#include <stdint.h>
#include <stddef.h>

#define SAVE_FILE "pokered.sav"

int  Save_Load(void);

int  Save_LoadFrom(const char *path);

typedef struct {
    uint8_t player_name[11];
    uint8_t badges;
    uint8_t pokedex_owned[19];
    int     valid;
} save_peek_t;

int  Save_PeekFrom(const char *path, save_peek_t *out);

int  Save_Write(void);

int  Save_ValidateChecksum(void);

int  Save_StateWrite(const char *path);
int  Save_StateLoad(const char *path);
size_t Save_StateSize(void);
int  Save_StateCaptureToBuffer(void *dst, size_t dst_size);
int  Save_StateLoadFromBuffer(const void *src, size_t src_size);

int  Save_StateWasBattle(void);

enum {
    SAVE_STATE_ERR_NONE = 0,
    SAVE_STATE_ERR_EMPTY,
    SAVE_STATE_ERR_CORRUPT,
    SAVE_STATE_ERR_VERSION,
};
int  Save_StateLastError(void);

int  Save_StatePeek(const char *path, int *out_version);
