#include "kbdef.h" // the board's slot lengths, before systick.h's defaults
#include "systick.h"
#include "interrupts.h"
#include "tick.h"
#include <stdint.h>

// Per-chip code: timer clock, interrupt vector, timer flag handling.
#include "systick_hw.h"

// Constants: converting microseconds to timer counts needs a divide.
#define RELOAD_LED_SUBFRAME SYSTICK_RELOAD_FROM_US(LED_SUBFRAME_US)
#define RELOAD_MATRIX_SCAN  SYSTICK_RELOAD_FROM_US(MATRIX_SCAN_SLOT_US)

static uint16_t slot_us;

static void timer2_reload(uint16_t reload)
{
    TR2    = 0;
    RCAP2H = (uint8_t)(reload >> 8);
    RCAP2L = (uint8_t)(reload & 0xFF);
    TH2    = (uint8_t)(reload >> 8);
    TL2    = (uint8_t)(reload & 0xFF);
    TR2    = 1;
}

void systick_init(void)
{
    TR2   = 0;
    T2CON = 0;
    T2MOD = 0;

    timer2_reload(RELOAD_LED_SUBFRAME);
    TF2 = 0;
    ET2 = 1;
    TR2 = 1;
}

void systick_arm(systick_slot_t slot)
{
    if (slot == SYSTICK_SLOT_MATRIX_SCAN) {
        slot_us = MATRIX_SCAN_SLOT_US;
        timer2_reload(RELOAD_MATRIX_SCAN);
    } else {
        slot_us = LED_SUBFRAME_US;
        timer2_reload(RELOAD_LED_SUBFRAME);
    }
}

void systick_pause(void)
{
    ET2 = 0;
}

void systick_resume(void)
{
    ET2 = 1;
}

volatile uint16_t systick_ticks;

void systick_interrupt_handler(void) __interrupt(SYSTICK_VECTOR)
{
    SYSTICK_ENTER();

    TF2 = 0;
    systick_ticks++;
    tick_time_advance(slot_us);
    tick_dispatch();

    SYSTICK_LEAVE();
}
