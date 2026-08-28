#pragma once
#include <stdint.h>

void SessionLog_Eventf(const char *fmt, ...);
void SessionLog_NpcSpoke(int npc_idx, int tx, int ty);
void SessionLog_ItemPickup(uint8_t item_id, int success);
void SessionLog_ItemAcquired(uint8_t item_id, uint8_t qty, int is_key_item, const char *reason);
void SessionLog_CapturedMon(uint8_t species, uint8_t level, int sent_to_box, int new_dex);
void SessionLog_EventFlagChange(uint16_t flag, int new_value, const char *name);
void SessionLog_BattleStart(int is_trainer, uint8_t trainer_class, uint8_t trainer_no, uint8_t wild_species, uint8_t enemy_level);
void SessionLog_BattleEnd(uint8_t result, int is_trainer, uint8_t trainer_class, uint8_t trainer_no, uint8_t wild_species, uint8_t enemy_level);
