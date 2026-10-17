#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t led_effect;
    uint8_t led_brightness;
    uint8_t led_speed;
    uint8_t led_color;
    uint8_t ul_effect;
    uint8_t ul_brightness;
    uint8_t ul_speed;
    uint8_t battery_indicator_on;
    uint8_t rf_link;
} user_settings_t;

extern user_settings_t user_settings;

bool settings_load(void);

void settings_save(void);

// Mark the settings for a save after a few idle matrix scans.
void settings_mark_dirty(void);

void settings_task(void);

// Board hooks around the flash write; the board turns its LEDs off here.
void settings_save_pre(void);
void settings_save_post(void);

#if DEBUG == 1
void settings_dump(void);
#endif
