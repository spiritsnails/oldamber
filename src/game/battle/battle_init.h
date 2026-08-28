#pragma once

#include <stdint.h>

void Battle_Start(void);

void Battle_RequestOldManType(void);

uint8_t Battle_PeekTrainerLevelForTransition(uint8_t trainer_class, uint8_t trainer_no);

void Battle_ReadTrainer(uint8_t trainer_class, uint8_t trainer_no);

void Battle_StartTrainer(uint8_t trainer_class, uint8_t trainer_no);

void Battle_StartTrainerCustomDebug(uint8_t trainer_class,
                                    const uint8_t species[6],
                                    const uint8_t level[6],
                                    const uint8_t moves[6][4],
                                    uint8_t count);
