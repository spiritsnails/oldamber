#pragma once
#include <stdint.h>

#include "sfx_ids.h"

#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2
#define AUDIO_BUFFER_SIZE  512

int  Audio_Init(void);
void Audio_Quit(void);

void Audio_Update(void);
void Audio_UpdateMusic(void);
void Audio_UpdateSfx(void);

void Audio_SetLowHealthAlarm(int on);
void Audio_DisableLowHealthAlarm(void);

int  Audio_IsLowHealthAlarmOn(void);

void Audio_PlayPokeFluteInBattle(void);
int  Audio_IsPokeFluteInBattlePlaying(void);
void Audio_ResetLowHealthAlarm(void);

void Audio_WriteReg(int channel, int reg, uint8_t value);

void Audio_SetWaveInstrument(int idx);

void Audio_SetWaveRaw(const uint8_t pattern[16]);

void Audio_PlaySFX_PressAB(void);

void Audio_WriteNR50(uint8_t v);
void Audio_WriteNR51(uint8_t v);

void Audio_SetMixVolume(uint8_t level);
void Audio_SetMixVolumeImmediate(uint8_t level);
float Audio_GetMixLevel(void);

void Audio_SetMasterVolume(int level);
void Audio_SetMusicVolume(int level);
void Audio_SetSfxVolume(int level);
int  Audio_GetMasterVolume(void);
int  Audio_GetMusicVolume(void);
int  Audio_GetSfxVolume(void);

void Audio_SetOutputMono(int enabled);
int  Audio_GetOutputMono(void);
void Audio_SetFocusMuted(int muted);
int  Audio_GetFocusMuted(void);

void Audio_ApplyChannelVolumes(void);

void Audio_PlaySFX_StartMenu(void);

void Audio_PlaySFX_TurnOnPC(void);
void Audio_PlaySFX_EnterPC(void);
void Audio_PlaySFX_TurnOffPC(void);
void Audio_PlaySFX_WithdrawDeposit(void);

void Audio_PlaySFX_Ledge(void);

void Audio_PlaySFX_Collision(void);

void Audio_PlaySFX_CollisionRetrigger(void);

void Audio_PlaySFX_GoInside(void);
void Audio_PlaySFX_GoOutside(void);

void Audio_PlaySFX_BattleHit(uint8_t dmg_mult);

void Audio_PlaySFX_SilphScope(void);
int  Audio_IsSFXPlaying_SilphScope(void);

void Audio_PlaySFX_BallPoof(void);

void Audio_PlaySFX_BallToss(void);

void Audio_PlaySFX_Tink(void);

void Audio_PlaySFX_Poisoned(void);

void Audio_PlaySFX_Shrink(void);

void Audio_PlaySFX_CaughtMon(void);

void Audio_PlaySFX_Faint(void);

void Audio_PlaySFX_Run(void);

void Audio_PlaySFX_Cut(void);

void Audio_PlaySFX_PushBoulder(void);

void Audio_PlaySFX_TeleportEnter1(void);

void Audio_PlaySFX_TeleportEnter2(void);

void Audio_PlaySFX_TeleportExit1(void);

void Audio_PlaySFX_TeleportExit2(void);
void Audio_PlaySFX_Battle24(void);
void Audio_PlaySFX_Battle28(void);
void Audio_PlaySFX_Battle29(void);
void Audio_PlaySFX_Battle2A(void);
void Audio_PlaySFX_Battle0D(void);
void Audio_PlaySFX_FaintFallOnly(void);

int  Audio_PlaySfx(uint16_t sfx_index);
int  Audio_PlaySfxModified(uint16_t sfx_index, int8_t pitch_add, uint8_t tempo_mod);

static inline void Audio_PlaySFX_Fly(void) {
    Audio_PlaySfxModified(SFX_NOT_VERY_EFFECTIVE, 0x20, 0xC0);
}
void Audio_SetMoveSfxDebug(int on);
int  Audio_IsMoveSfxDebug(void);

void Audio_PlaySFX_Switch(void);

void Audio_PlaySFX_Swap(void);

void Audio_PlaySFX_ArrowTiles(void);

void Audio_PlaySFX_SafariZonePA(void);

void Audio_PlaySFX_TradeMachine(void);

void Audio_PlaySFX_SlotsNewSpin(void);
void Audio_PlaySFX_SlotsStopWheel(void);
void Audio_PlaySFX_SlotsReward(void);

void Audio_PlaySFX_Denied(void);

void Audio_UseTitleScreenBank(void);
void Audio_PlaySFX_ShootingStar(void);
void Audio_PlaySFX_IntroHip(void);
void Audio_PlaySFX_IntroHop(void);
void Audio_PlaySFX_IntroRaise(void);
void Audio_PlaySFX_IntroCrash(void);

void Audio_PlaySFX_IntroWhoosh(void);
void Audio_PlaySFX_IntroLunge(void);

void Audio_PlaySFX_LevelUp(void);

void Audio_PlaySFX_DexRating(void);

void Audio_PlaySFX_HealingMachine(void);

void Audio_PlaySFX_HealHP(void);

void Audio_PlaySFX_HealAilment(void);

void Audio_PlaySFX_Save(void);

void Audio_PlaySFX_GetItem1(void);

void Audio_PlaySFX_GetItem2(void);

void Audio_PlaySFX_GetKeyItem(void);

int  Audio_IsSFXPlaying_GetKeyItem(void);

int  Audio_IsSFXPlaying(void);

void Audio_PlaySFX_SSAnneHorn(void);
int  Audio_IsSFXPlaying_SSAnneHorn(void);

void Audio_PlaySFX_Purchase(void);

#define AUDIO_CRIES_GEN1    0
#define AUDIO_CRIES_CRYSTAL 1
void Audio_SetCryStyle(int style);
int  Audio_GetCryStyle(void);

void Audio_PlayCry(uint8_t species);
void Audio_PlayCryModified(uint8_t species, int8_t pitch_add, uint8_t tempo_add);
int  Audio_IsCryPlaying(void);

int  Audio_StillSounding(void);

int  Audio_IsMoveSFXPlaying(void);
