#pragma once

#include <stdint.h>
#include <stdbool.h>

// Tap/hold engine for dual-role keys. A short press sends the tap key. A long
// press runs the hold action. The board handles the action in the callback.

#define TAP_HOLD_FLAG_LAYER        0x01 // hold sends no key; the board handles it
#define TAP_HOLD_FLAG_MOD          0x02 // hold adds hold_mod; release removes it
#define TAP_HOLD_FLAG_KEY          0x04 // hold presses hold_key; release releases it
#define TAP_HOLD_FLAG_CLICK        0x08 // on release, click hold_key after click_ms
#define TAP_HOLD_FLAG_TAP_FALLBACK 0x10 // if the click is skipped, send the tap key
#define TAP_HOLD_FLAG_INSTANT      0x20 // hold starts as soon as the key is down

typedef struct {
    uint16_t keycode;
    uint16_t tap_key;
    uint16_t hold_key;
    uint8_t  hold_mod;
    uint16_t grace_ms; // other keys within this time do not trigger the hold
    uint8_t  flags;    // TAP_HOLD_FLAG_*
} tap_hold_entry_t;

typedef struct {
    uint16_t promote_ms; // hold timeout, in milliseconds
    uint16_t click_ms;   // minimum hold time for a click
    uint8_t  tap_ms;     // delay between press and release of a click
} tap_hold_config_t;

typedef void (*tap_hold_callback_t)(uint8_t index, bool held);

void tap_hold_init(const tap_hold_config_t *config, const tap_hold_entry_t *entries,
                   uint8_t count, tap_hold_callback_t callback);

// Handle a key. Returns true if the engine consumed it. A key not in the table
// promotes pending entries.
bool tap_hold_process(uint16_t keycode, bool key_pressed);

// Promote pending presses when the hold timeout expires.
void tap_hold_task(void);

bool tap_hold_held(uint8_t index);

// True if another key was pressed during the hold (a modifier combo).
bool tap_hold_used(uint8_t index);
