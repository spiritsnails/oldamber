#pragma once
#include <stdint.h>

void GymScripts_Tick(void);
void GymScripts_OnMapLoad(void);

int  GymScripts_IsActive(void);

int  GymScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out);

void GymScripts_OnVictory(void);

void GymScripts_OnGymTrainerVictory(void);

void GymScripts_SetTrainerPending(uint8_t cls, uint8_t no, uint32_t flag,
                                   const char *end_text, const char *after_text,
                                   const char *pre_text);

int  GymScripts_ConsumeGymTrainer(void);

void GymScripts_BrockInteract(void);
void GymScripts_GymTrainerInteract(void);

void GymScripts_MistyInteract(void);
void GymScripts_CeruleanTrainer0Interact(void);
void GymScripts_CeruleanTrainer1Interact(void);
void GymScripts_CeruleanGuideInteract(void);

void GymScripts_SurgeInteract(void);

void GymScripts_ErikaInteract(void);

void GymScripts_KogaInteract(void);
void GymScripts_FuchsiaTrainer1Interact(void);
void GymScripts_FuchsiaTrainer2Interact(void);
void GymScripts_FuchsiaTrainer3Interact(void);
void GymScripts_FuchsiaTrainer4Interact(void);
void GymScripts_FuchsiaTrainer5Interact(void);
void GymScripts_FuchsiaTrainer6Interact(void);
void GymScripts_FuchsiaGuideInteract(void);

void GymScripts_BlaineInteract(void);

void GymScripts_GiovanniInteract(void);
void GymScripts_ViridianTrainer0Interact(void);
void GymScripts_ViridianTrainer1Interact(void);
void GymScripts_ViridianTrainer2Interact(void);
void GymScripts_ViridianTrainer3Interact(void);
void GymScripts_ViridianTrainer4Interact(void);
void GymScripts_ViridianTrainer5Interact(void);
void GymScripts_ViridianTrainer6Interact(void);
void GymScripts_ViridianTrainer7Interact(void);
void GymScripts_ViridianGuideInteract(void);

void GymScripts_SabrinaInteract(void);
void GymScripts_SaffronTrainer0Interact(void);
void GymScripts_SaffronTrainer1Interact(void);
void GymScripts_SaffronTrainer2Interact(void);
void GymScripts_SaffronTrainer3Interact(void);
void GymScripts_SaffronTrainer4Interact(void);
void GymScripts_SaffronTrainer5Interact(void);
void GymScripts_SaffronTrainer6Interact(void);
void GymScripts_SaffronGuideInteract(void);
