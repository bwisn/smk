#include "tap_hold.h"
#include "keycodes.h"
#include "host.h"
#include "report.h"
#include "delay.h"
#include "tick.h"
#include <stddef.h>

#define TAP_HOLD_MAX_ENTRIES 8

#define TAP_HOLD_STATE_ACTIVE    0x01
#define TAP_HOLD_STATE_HELD      0x02
#define TAP_HOLD_STATE_USED      0x04
#define TAP_HOLD_STATE_MOD_OWNED 0x08

typedef struct {
    uint16_t press_ms;
    uint8_t  state;
} tap_hold_state_t;

static const tap_hold_entry_t *tap_hold_entries;
static tap_hold_config_t tap_hold_config;
static tap_hold_callback_t tap_hold_callback;
static uint8_t tap_hold_count;
static tap_hold_state_t tap_hold_states[TAP_HOLD_MAX_ENTRIES];

static uint16_t tap_hold_elapsed(uint8_t index)
{
    return (uint16_t)(tick_millis() - tap_hold_states[index].press_ms);
}

static void tap_hold_click(uint16_t keycode)
{
    if (keycode == KC_NO) {
        return;
    }

    host_keycode_send(keycode, true);
    delay_ms(tap_hold_config.tap_ms);
    host_keycode_send(keycode, false);
}

static void tap_hold_promote_entry(uint8_t index)
{
    const tap_hold_entry_t *entry = &tap_hold_entries[index];
    tap_hold_state_t *state = &tap_hold_states[index];

    state->state |= TAP_HOLD_STATE_HELD;

    if (tap_hold_callback != NULL) {
        tap_hold_callback(index, true);
    }

    if (entry->flags & TAP_HOLD_FLAG_KEY) {
        if (entry->hold_key != KC_NO) {
            host_keycode_send(entry->hold_key, true);
        }
    } else if (entry->flags & TAP_HOLD_FLAG_MOD) {
        // If the modifier was already on, another key is using it; leave it on
        // at release.
        if (entry->hold_mod != 0 && (get_mods() & entry->hold_mod) == 0) {
            add_mods(entry->hold_mod);
            state->state |= TAP_HOLD_STATE_MOD_OWNED;
            send_keyboard_report();
        }
    }
}

static void tap_hold_promote_pending(bool preemptive)
{
    for (uint8_t i = 0; i < tap_hold_count; i++) {
        const tap_hold_entry_t *entry = &tap_hold_entries[i];
        const tap_hold_state_t *state = &tap_hold_states[i];

        if (!(state->state & TAP_HOLD_STATE_ACTIVE) || (state->state & TAP_HOLD_STATE_HELD)) {
            continue;
        }

        if (preemptive && entry->grace_ms != 0 && tap_hold_elapsed(i) < entry->grace_ms) {
            continue;
        }

        tap_hold_promote_entry(i);
    }
}

static void tap_hold_mark_used(void)
{
    for (uint8_t i = 0; i < tap_hold_count; i++) {
        tap_hold_state_t *state = &tap_hold_states[i];

        if ((state->state & TAP_HOLD_STATE_ACTIVE) && (state->state & TAP_HOLD_STATE_HELD)) {
            state->state |= TAP_HOLD_STATE_USED;
        }
    }
}

static void tap_hold_release(uint8_t index)
{
    const tap_hold_entry_t *entry = &tap_hold_entries[index];
    tap_hold_state_t *state = &tap_hold_states[index];

    if (!(state->state & TAP_HOLD_STATE_ACTIVE)) {
        return;
    }

    if (!(state->state & TAP_HOLD_STATE_HELD)) {
        tap_hold_click(entry->tap_key);
    } else {
        if (tap_hold_callback != NULL) {
            tap_hold_callback(index, false);
        }

        if (entry->flags & TAP_HOLD_FLAG_CLICK) {
            if (!(state->state & TAP_HOLD_STATE_USED)) {
                if (tap_hold_elapsed(index) >= tap_hold_config.click_ms) {
                    tap_hold_click(entry->hold_key);
                } else if (entry->flags & TAP_HOLD_FLAG_TAP_FALLBACK) {
                    tap_hold_click(entry->tap_key);
                }
            }
        } else if (entry->flags & TAP_HOLD_FLAG_MOD) {
            if (state->state & TAP_HOLD_STATE_MOD_OWNED) {
                del_mods(entry->hold_mod);
                state->state &= (uint8_t)~TAP_HOLD_STATE_MOD_OWNED;
                send_keyboard_report();
            }
        } else if (entry->flags & TAP_HOLD_FLAG_KEY) {
            if (entry->hold_key != KC_NO) {
                host_keycode_send(entry->hold_key, false);
            }
        } else if (entry->flags & TAP_HOLD_FLAG_LAYER) {
            // A layer key sends its tap if the hold was not used.
            if (!(state->state & TAP_HOLD_STATE_USED)) {
                tap_hold_click(entry->tap_key);
            }
        }
    }

    state->state = 0;
}

void tap_hold_init(const tap_hold_config_t *config, const tap_hold_entry_t *entries,
                   uint8_t count, tap_hold_callback_t callback)
{
    if (count > TAP_HOLD_MAX_ENTRIES) {
        count = TAP_HOLD_MAX_ENTRIES;
    }

    tap_hold_entries  = entries;
    tap_hold_count    = count;
    tap_hold_callback = callback;

    tap_hold_config.promote_ms = config->promote_ms;
    tap_hold_config.click_ms   = config->click_ms;
    tap_hold_config.tap_ms     = config->tap_ms;

    for (uint8_t i = 0; i < count; i++) {
        tap_hold_states[i].press_ms = 0;
        tap_hold_states[i].state    = 0;
    }
}

bool tap_hold_process(uint16_t keycode, bool key_pressed)
{
    for (uint8_t i = 0; i < tap_hold_count; i++) {
        if (tap_hold_entries[i].keycode != keycode) {
            continue;
        }

        if (key_pressed) {
            tap_hold_states[i].press_ms = tick_millis();
            tap_hold_states[i].state    = TAP_HOLD_STATE_ACTIVE;

            if (tap_hold_entries[i].flags & TAP_HOLD_FLAG_INSTANT) {
                // Promote now so the next key in a combo is not lost.
                tap_hold_promote_entry(i);
            }
        } else {
            tap_hold_release(i);
        }

        return true;
    }

    if (key_pressed) {
        tap_hold_promote_pending(true);
    }

    tap_hold_mark_used();

    return false;
}

void tap_hold_task(void)
{
    for (uint8_t i = 0; i < tap_hold_count; i++) {
        const tap_hold_state_t *state = &tap_hold_states[i];

        if (!(state->state & TAP_HOLD_STATE_ACTIVE) || (state->state & TAP_HOLD_STATE_HELD)) {
            continue;
        }

        if (tap_hold_elapsed(i) >= tap_hold_config.promote_ms) {
            tap_hold_promote_pending(false);
            return;
        }
    }
}

bool tap_hold_held(uint8_t index)
{
    if (index >= tap_hold_count) {
        return false;
    }

    return (tap_hold_states[index].state & TAP_HOLD_STATE_HELD) != 0;
}

bool tap_hold_used(uint8_t index)
{
    if (index >= tap_hold_count) {
        return false;
    }

    return (tap_hold_states[index].state & TAP_HOLD_STATE_USED) != 0;
}
