#pragma once

// The Timer2 input on this part was measured at ~262 kHz.
#define SYSTICK_COUNTS_PER_US_NUM 262
#define SYSTICK_COUNTS_PER_US_DEN 1000

#define SYSTICK_RELOAD_FROM_US(us) \
    ((uint16_t)(65536u - (uint16_t)(((uint32_t)(us) * SYSTICK_COUNTS_PER_US_NUM) / SYSTICK_COUNTS_PER_US_DEN)))

#define SYSTICK_VECTOR _INT_TIMER2

// The timer flag needs no page handling on this part.
#define SYSTICK_ENTER() \
    do {                \
    } while (0)
#define SYSTICK_LEAVE() \
    do {                \
    } while (0)
