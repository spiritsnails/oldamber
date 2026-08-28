#pragma once

#include <stdint.h>

typedef enum {
    CATCH_RESULT_SUCCESS     = 0x43,
    CATCH_RESULT_CANNOT_CATCH= 0x10,
    CATCH_RESULT_0_SHAKES    = 0x20,
    CATCH_RESULT_1_SHAKE     = 0x61,
    CATCH_RESULT_2_SHAKES    = 0x62,
    CATCH_RESULT_3_SHAKES    = 0x63,
} catch_result_t;

catch_result_t Battle_CatchAttempt(uint8_t ball_id);
