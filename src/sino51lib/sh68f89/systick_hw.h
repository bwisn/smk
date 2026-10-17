#pragma once

// Timer2 takes T2MOD.TCLKP2's default 1/12 prescale: two counts per
// microsecond at 24 MHz FREQ_SYS.
#define SYSTICK_COUNTS_PER_US_NUM 2
#define SYSTICK_COUNTS_PER_US_DEN 1

#define SYSTICK_RELOAD_FROM_US(us) \
    ((uint16_t)(65536u - (uint16_t)(((uint32_t)(us) * SYSTICK_COUNTS_PER_US_NUM) / SYSTICK_COUNTS_PER_US_DEN)))

#define SYSTICK_VECTOR _INT_TIMER2

// TF2 is T2CON.7 on page 0 and PCA1's CF1 on page 1, so the page has to be
// pinned while the flag is touched.
static uint8_t systick_page;

#define SYSTICK_ENTER()       \
    do {                      \
        systick_page = INSCON; \
        sfr_page_0();         \
    } while (0)
#define SYSTICK_LEAVE()       \
    do {                      \
        INSCON = systick_page; \
    } while (0)
