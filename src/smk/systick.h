#pragma once

#include <stdint.h>

// Keyboard tick: one hardware timer alternates between LED subframes and
// matrix scans. The silicon provides the timer; the board sets the slot
// lengths.

typedef enum {
    SYSTICK_SLOT_LED_SUBFRAME,
    SYSTICK_SLOT_MATRIX_SCAN,
} systick_slot_t;

// The slot lengths, in microseconds. A board overrides them in its kbdef.h.
#ifndef LED_SUBFRAME_US
#    define LED_SUBFRAME_US 400
#endif
#ifndef MATRIX_SCAN_SLOT_US
#    define MATRIX_SCAN_SLOT_US 500
#endif

void systick_init(void);

void systick_arm(systick_slot_t slot);

void systick_pause(void);
void systick_resume(void);

// Tick interrupts since start, for diagnostics.
extern volatile uint16_t systick_ticks;
