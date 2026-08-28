
#pragma once
#include <stdint.h>

#define JOHTO_TRAINER_COUNT 541

typedef struct {
    uint8_t species;
    uint8_t level;
    uint8_t moves[4];
} johto_mon_t;

typedef struct {
    const char *class_name;
    uint8_t     trainer_no;
    const char *name;
    uint8_t     count;
    uint8_t     class_id;
    johto_mon_t party[6];
} johto_trainer_t;

extern const johto_trainer_t gJohtoTrainers[JOHTO_TRAINER_COUNT];

int JohtoTrainer_Find(const char *class_name, int trainer_no);
