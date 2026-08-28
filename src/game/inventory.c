
#include "inventory.h"
#include "session_log.h"
#include "../platform/hardware.h"
#include "../data/item_data_gen.h"

#define MAX_QTY          99
#define ITEM_TERM        0xFF

int Inventory_AddTo(uint8_t *num, uint8_t *items, int cap, uint8_t item_id, uint8_t qty) {
    for (int i = 0; i < (int)*num; i++) {
        if (items[i * 2] == item_id) {
            int new_qty = (int)items[i * 2 + 1] + qty;
            if (new_qty > MAX_QTY) new_qty = MAX_QTY;
            items[i * 2 + 1] = (uint8_t)new_qty;
            return 0;
        }
    }
    if ((int)*num >= cap) return -1;
    int slot = *num;
    items[slot * 2]     = item_id;
    items[slot * 2 + 1] = qty > MAX_QTY ? MAX_QTY : qty;
    (*num)++;
    items[*num * 2] = ITEM_TERM;
    return 0;
}

int Inventory_RemoveFrom(uint8_t *num, uint8_t *items, uint8_t item_id, uint8_t qty) {
    for (int i = 0; i < (int)*num; i++) {
        if (items[i * 2] != item_id) continue;
        int new_qty = (int)items[i * 2 + 1] - qty;
        if (new_qty > 0) {
            items[i * 2 + 1] = (uint8_t)new_qty;
        } else {
            for (int j = i; j < (int)*num - 1; j++) {
                items[j * 2]     = items[(j + 1) * 2];
                items[j * 2 + 1] = items[(j + 1) * 2 + 1];
            }
            (*num)--;
            items[*num * 2] = ITEM_TERM;
        }
        return 0;
    }
    return -1;
}

int Inventory_Add(uint8_t item_id, uint8_t qty) {
    int r = Inventory_AddTo(&wNumBagItems, wBagItems, BAG_ITEM_CAPACITY, item_id, qty);
    if (r == 0 && Inventory_IsKeyItem(item_id))
        SessionLog_ItemAcquired(item_id, qty, 1, "script/shop/debug");
    return r;
}

int Inventory_Remove(uint8_t item_id, uint8_t qty) {
    return Inventory_RemoveFrom(&wNumBagItems, wBagItems, item_id, qty);
}

int Inventory_GetQty(uint8_t item_id) {
    for (int i = 0; i < (int)wNumBagItems; i++)
        if (wBagItems[i * 2] == item_id)
            return wBagItems[i * 2 + 1];
    return 0;
}

void Inventory_DecodeASCII(uint8_t item_id, char *buf, int buf_size) {
    if (buf_size <= 0) return;
    if (item_id >= HM01 && item_id < TM01) {
        int n = item_id - HM01 + 1;
        if (buf_size >= 5) { buf[0]='H'; buf[1]='M'; buf[2]='0'+n/10; buf[3]='0'+n%10; buf[4]='\0'; }
        else buf[0] = '\0';
        return;
    }
    if (item_id >= TM01) {
        int n = item_id - TM01 + 1;
        if (buf_size >= 5) { buf[0]='T'; buf[1]='M'; buf[2]='0'+n/10; buf[3]='0'+n%10; buf[4]='\0'; }
        else buf[0] = '\0';
        return;
    }
    if (item_id == 0 || item_id > NUM_ITEMS) { buf[0] = '\0'; return; }
    const uint8_t *src = gItemNames[item_id];
    int out = 0;
    for (int i = 0; i < ITEM_NAME_LENGTH && out < buf_size - 1; i++) {
        uint8_t c = src[i];
        if (c == 0x50) break;
        if (c >= 0x80 && c <= 0x99)                   { buf[out++] = 'A' + (c - 0x80); }
        else if (c >= 0xA0 && c <= 0xB9)              { buf[out++] = 'a' + (c - 0xA0); }
        else if (c == 0x7F)                            { buf[out++] = ' '; }
        else if (c >= 0xF6)                            { buf[out++] = '0' + (c - 0xF6); }
        else if (c == 0xE8)                            { buf[out++] = '.'; }
        else if (c == 0xE3)                            { buf[out++] = '-'; }

        else if (c == 0xBA)                            { buf[out++] = (char)0xE9; }
        else if ((c == 0xBB || c == 0xBC || c == 0xBD ||
                  c == 0xBE || c == 0xBF || c == 0xE4 || c == 0xE5)
                 && out < buf_size - 2) {

            static const char suf[] = "dlstvrm";
            static const uint8_t codes[] = {0xBB,0xBC,0xBD,0xBE,0xBF,0xE4,0xE5};
            buf[out++] = '\'';
            for (int k = 0; k < 7; k++) if (codes[k] == c) { buf[out++] = suf[k]; break; }
        }
        else { buf[out++] = '?'; }
    }
    buf[out] = '\0';
}

int Inventory_IsKeyItem(uint8_t item_id) {
    if (item_id == ITEM_NONE || item_id > NUM_ITEMS) return 0;
    int idx = item_id - 1;
    return (gKeyItemFlags[idx / 8] >> (idx % 8)) & 1;
}

int Inventory_IsHM(uint8_t item_id) {
    return item_id >= HM01 && item_id < TM01;
}

int Inventory_TmHmIdFromName(const char *name) {
    int n;
    if (!name || !name[0] || !name[1] || !name[2] || !name[3] || name[4]) return -1;
    if (name[1] != 'M' && name[1] != 'm') return -1;
    if (name[2] < '0' || name[2] > '9' || name[3] < '0' || name[3] > '9') return -1;
    n = (name[2] - '0') * 10 + (name[3] - '0');
    if (name[0] == 'T' || name[0] == 't') {
        if (n >= 1 && n <= 50) return TM01 + (n - 1);
    } else if (name[0] == 'H' || name[0] == 'h') {
        if (n >= 1 && n <= 5) return HM01 + (n - 1);
    }
    return -1;
}

const uint8_t *Inventory_GetName(uint8_t item_id) {

    static uint8_t tmhm_name[6];
    if (item_id >= HM01 && item_id < TM01) {
        int n = item_id - HM01 + 1;
        tmhm_name[0] = 0x87;
        tmhm_name[1] = 0x8C;
        tmhm_name[2] = 0xF6 + n / 10;
        tmhm_name[3] = 0xF6 + n % 10;
        tmhm_name[4] = 0x50;
        return tmhm_name;
    }
    if (item_id >= TM01) {
        int n = item_id - TM01 + 1;
        tmhm_name[0] = 0x93;
        tmhm_name[1] = 0x8C;
        tmhm_name[2] = 0xF6 + n / 10;
        tmhm_name[3] = 0xF6 + n % 10;
        tmhm_name[4] = 0x50;
        return tmhm_name;
    }
    if (item_id > NUM_ITEMS) item_id = 0;
    return gItemNames[item_id];
}
