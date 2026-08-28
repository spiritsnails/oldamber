
#ifndef GB_APU_H
#define GB_APU_H

#include <stdint.h>

#define GB_APU_CLOCK 4194304

void GbApu_Reset(void);

void GbApu_WriteReg(uint8_t lo, uint8_t value);
uint8_t GbApu_ReadReg(uint8_t lo);

void GbApu_StepT(uint32_t t_cycles);

void GbApu_ChannelSamples(uint8_t out[4], uint8_t active[4]);

void GbApu_Mix(float *left, float *right);

void GbApu_SetOutputRate(uint32_t hz);

void GbApu_SetSeqPhase(int32_t t_until_next_step);

void GbApu_SetSeqStep(uint8_t step);

uint16_t GbApu_DebugCh1Freq(void);

void GbApu_RenderSamples(float *out, int n);

void GbApu_SetChannelGain(int ch, float gain);

#endif
