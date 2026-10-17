#include "settings.h"
#include "nvm.h"
#include "matrix.h"
#include "tick.h"
#if DEBUG == 1
#    include "debug.h"
#endif

_Static_assert(sizeof(user_settings_t) <= NVM_CAPACITY, "user_settings_t too large for the settings record");

user_settings_t user_settings;

// Wait for idle scans after the last change, so a held brightness key writes
// the flash once. A slower board overrides SETTINGS_SAVE_IDLE_SCANS.
#ifndef SETTINGS_SAVE_IDLE_SCANS
#    define SETTINGS_SAVE_IDLE_SCANS 480
#endif

static bool settings_dirty;
static uint16_t settings_dirty_scan;

bool settings_load(void)
{
    return nvm_load((__xdata uint8_t *)&user_settings, (uint8_t)sizeof(user_settings));
}

#if DEBUG == 1
void settings_dump(void)
{
    dprintf("settings le=%02x lb=%02x ls=%02x lc=%02x ue=%02x ub=%02x us=%02x bat=%02x rf=%02x\r\n", user_settings.led_effect, user_settings.led_brightness, user_settings.led_speed, user_settings.led_color, user_settings.ul_effect, user_settings.ul_brightness, user_settings.ul_speed, user_settings.battery_indicator_on, user_settings.rf_link);
}
#endif

void settings_save(void)
{
    settings_dirty = false; // clear first: a mark during the write is not lost

#if DEBUG == 1
    settings_dump();
#endif

    // Pause the tick so no scan leaves a row lit during the write.
    tick_pause();

    settings_save_pre();
    nvm_save((const __xdata uint8_t *)&user_settings, (uint8_t)sizeof(user_settings));
    settings_save_post();

    tick_resume();
}

void settings_mark_dirty(void)
{
    settings_dirty_scan = matrix_scan_count();
    settings_dirty      = true;
}

void settings_task(void)
{
    if (!settings_dirty) {
        return;
    }

    if ((uint16_t)(matrix_scan_count() - settings_dirty_scan) < SETTINGS_SAVE_IDLE_SCANS) {
        return;
    }

    settings_save();
}
