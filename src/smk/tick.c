#include "kbdef.h" // the board's slot lengths, before systick.h's defaults
#include "tick.h"
#include "systick.h"
#include "matrix.h"
#include "indicators.h"
#include "keyboard.h"
#include "sleep.h"
#include <stdbool.h>
#include <stdint.h>

// Slot lengths are board settings, not silicon settings. The LED subframe must
// equal one backlight PWM cycle; the scan slot must cover the matrix scan.
#ifndef LED_SUBFRAME_US
#    define LED_SUBFRAME_US 400
#endif
#ifndef MATRIX_SCAN_SLOT_US
#    define MATRIX_SCAN_SLOT_US 500
#endif

// subframes must outnumber the scans, or the refresh rate is the scan rate divided by the subframe count.
#ifndef LED_SUBFRAMES_PER_SCAN
#    define LED_SUBFRAMES_PER_SCAN 1
#endif

// Run the matrix scan in the LED frame-wrap slot instead of a slot of its own.
#ifndef LED_SCAN_IN_FRAME_SLOT
#    define LED_SCAN_IN_FRAME_SLOT 0
#endif

static volatile bool    scan_due;
static volatile uint8_t subframes_since_scan;

// The ISR writes the low byte; the reader tracks the high byte so a 16-bit
// read is never split.
static volatile uint8_t tick_ms_low;
static uint16_t         tick_us_acc;

void tick_time_advance(uint16_t us)
{
    tick_us_acc += us;
    while (tick_us_acc >= 1000) {
        tick_us_acc -= 1000;
        tick_ms_low++;
    }
}

uint16_t tick_millis(void)
{
    static uint8_t high;
    static uint8_t last_low;

    const uint8_t low = tick_ms_low; // a byte read is atomic
    if (low < last_low) {
        high++; // the low byte wrapped since the last read
    }
    last_low = low;

    return (uint16_t)(((uint16_t)high << 8) | low);
}

void tick_init(void)
{
    scan_due             = true;
    subframes_since_scan = 0;

    systick_init();
}

static void run_matrix_scan(void)
{
    systick_arm(SYSTICK_SLOT_MATRIX_SCAN);
    matrix_scan_full();
}

static bool run_led_subframe(void)
{
    systick_arm(SYSTICK_SLOT_LED_SUBFRAME);

    indicators_pre_update();
    const bool frame_wrapped = indicators_update_step(&keyboard_state, 0);
    indicators_post_update();

    sleep_note_frame(frame_wrapped);

    return frame_wrapped;
}

void tick_dispatch(void)
{
#if LED_SCAN_IN_FRAME_SLOT
    if (run_led_subframe()) {
        run_matrix_scan();
    }
#else
    if (scan_due) {
        scan_due             = false;
        subframes_since_scan = 0;
        run_matrix_scan();
        return;
    }

    run_led_subframe();

    if (++subframes_since_scan >= LED_SUBFRAMES_PER_SCAN) {
        scan_due = true;
    }
#endif
}

void tick_pause(void)
{
    systick_pause();
}

void tick_resume(void)
{
    systick_resume();
}
