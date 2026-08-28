
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t  dest_map;
    int16_t  player_coord;
    int16_t  adjust;
} map_conn_t;

typedef struct {
    map_conn_t north;
    map_conn_t south;
    map_conn_t west;
    map_conn_t east;
} map_connections_t;

#define NUM_MAP_CONNECTIONS 256
