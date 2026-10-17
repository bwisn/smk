#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "keycodes.h"
#include "kbdef.h"
#include "keyboard.h"
#include "layout.h"
#include "settings.h"
#include "debug.h"
#include "report.h"
#include "host.h"
#include "usb.h"
#include "indicators.h"
#include "tap_hold.h"
#include "tick.h"
#include "matrix.h"

#ifdef RF_ENABLED
#    include "bb_spi.h"
#    include "rf_controller.h"
#endif

/*
 * Redragon K530 Pro transport and wireless handling.
 *
 * Switches on the left side:
 *   - power ON/OFF: cuts battery power; not read by the MCU.
 *     Wired mode = switch OFF + USB cable (MCU powered from VBUS).
 *   - 3-position wireless slider "2.4G / B1 / B2" on P1_0 + P5_6
 *     (combos in kbdef.h; (0,0) keeps the current profile).
 *
 * Transport (USB or radio) is selected at runtime by cable presence:
 * VBUS is sensed on P7_5. USB is used while the cable is plugged in;
 * otherwise reports go to the BK3632.
 *
 * Radio frames:
 *   boot announce:  AA 03 03 02 00 <chk>  (cmd 0x03, param 0x02, once)
 *   profile set:    AA 03 01 00 <profile> (cmd 0x01, pairing = 0)
 *   cable plug:     AA 03 06 01 00 <chk>  (cmd 0x06, param 0x01)
 *   pairing chord:  AA 03 01 01 <channel> (cmd 0x01 pairing frame)
 */

typedef enum {
    KEYBOARD_CONN_MODE_RF  = 0,
    KEYBOARD_CONN_MODE_USB = 1,
} user_keyboard_conn_mode_t;

typedef enum {
    RF_PROFILE_2_4G = RF_MODE_2_4G,
    RF_PROFILE_BT1  = RF_MODE_BT1,
    RF_PROFILE_BT2  = RF_MODE_BT2,
} user_rf_profile_t;

// Cable detect and slider debounce time, in ms.
#define SWITCH_DEBOUNCE_MS 250

enum {
    K530_TH_UP = 0,
    K530_TH_LEFT,
    K530_TH_DOWN,
    K530_TH_RIGHT,
    K530_TH_MOD1,
    K530_TH_COUNT,
};

#define K530_TAP_HOLD_MS 105
// Fn2 hold time for Menu; shorter holds send Down.
#define K530_FN2_MENU_MS 214
#define K530_TAP_HOLD_TAP_MS 10

// Layer keys: Fn taps Left, Fn2 taps Down/Menu. Both are active immediately,
// so a combo key is not lost.
static const tap_hold_entry_t k530_tap_hold_entries[K530_TH_COUNT] = {
    // key,        tap key,      hold key,  hold modifier, grace, flags
    {K530_TAP_UP,    KC_UP,       KC_NO,   MOD_BIT(KC_RSFT), 0, TAP_HOLD_FLAG_MOD},
    {K530_TAP_LEFT,  KC_LEFT,     KC_NO,   0,                0, TAP_HOLD_FLAG_LAYER | TAP_HOLD_FLAG_INSTANT},
    {K530_TAP_DOWN,  KC_DOWN,     KC_APP,  0,                0, TAP_HOLD_FLAG_LAYER | TAP_HOLD_FLAG_CLICK | TAP_HOLD_FLAG_TAP_FALLBACK | TAP_HOLD_FLAG_INSTANT},
    {K530_TAP_RIGHT, KC_RIGHT,    KC_NO,   MOD_BIT(KC_RCTL), 0, TAP_HOLD_FLAG_MOD},
    {K530_MOD1,      KC_CAPS_LOCK,KC_NO,   0,                0, TAP_HOLD_FLAG_LAYER | TAP_HOLD_FLAG_INSTANT},
};

static const tap_hold_config_t k530_tap_hold_config = {
    K530_TAP_HOLD_MS,
    K530_FN2_MENU_MS,
    K530_TAP_HOLD_TAP_MS,
};

static uint8_t k530_virtual_keys[4];

// Layer key state seen by the event stream.
static bool k530_fn_held_event;
static bool k530_fn2_held_event;

extern void indicators_brightness_up(void);
extern void indicators_brightness_down(void);
extern void indicators_toggle(void);
extern void indicators_next_mode(void);
extern void indicators_speed_cycle(void);
extern uint16_t indicators_rf_gate_laps(void);

// Fn and Fn2 positions in the matrix.
#define K530_FN_ROW  4
#define K530_FN_COL  9
#define K530_FN2_ROW 4
#define K530_FN2_COL 12

// Return true if the key is currently held. Events are ordered by column, so a
// combo key in the same column can arrive before the layer key. The matrix
// snapshot gives the current state; event flags catch a release missed by the
// main loop.
static bool k530_key_held(uint8_t row, uint8_t col)
{
    return (matrix_snapshot[col] & MATRIX_ROW_BIT(row)) != 0;
}

static bool k530_fn_held(void)
{
    return k530_fn_held_event || k530_key_held(K530_FN_ROW, K530_FN_COL);
}

static bool k530_fn2_held(void)
{
    return k530_fn2_held_event || k530_key_held(K530_FN2_ROW, K530_FN2_COL);
}

static void k530_tap_hold_on_hold(uint8_t index, bool held)
{
    switch (index) {
        case K530_TH_LEFT:
            k530_fn_held_event = held;
            break;
        case K530_TH_DOWN:
            k530_fn2_held_event = held;
            break;
        default:
            break;
    }
}

static uint16_t k530_arrow_keycode(uint16_t keycode)
{
    switch (keycode) {
        case KC_W: return KC_UP;
        case KC_A: return KC_LEFT;
        case KC_S: return KC_DOWN;
        case KC_D: return KC_RIGHT;
        default: return KC_NO;
    }
}

static int8_t k530_arrow_index(uint16_t keycode)
{
    switch (keycode) {
        case KC_W: return 0;
        case KC_A: return 1;
        case KC_S: return 2;
        case KC_D: return 3;
        default: return -1;
    }
}

static uint16_t k530_fn_keycode(uint16_t keycode)
{
    switch (keycode) {
        case KC_1: return KC_F1;
        case KC_2: return KC_F2;
        case KC_3: return KC_F3;
        case KC_4: return KC_F4;
        case KC_5: return KC_F5;
        case KC_6: return KC_F6;
        case KC_7: return KC_F7;
        case KC_8: return KC_F8;
        case KC_9: return KC_F9;
        case KC_0: return KC_F10;
        case KC_MINUS: return KC_F11;
        case KC_EQUAL: return KC_F12;
        case KC_W: return KC_UP;
        case KC_P: return KC_PRINT_SCREEN;
        case KC_LEFT_BRACKET: return KC_HOME;
        case KC_RIGHT_BRACKET: return KC_END;
        case KC_A: return KC_LEFT;
        case KC_S: return KC_DOWN;
        case KC_D: return KC_RIGHT;
        case KC_SEMICOLON: return KC_PAGE_UP;
        case KC_QUOTE: return KC_PAGE_DOWN;
        case KC_COMMA: return KC_MEDIA_PREV_TRACK;
        case KC_DOT: return KC_INSERT;
        case KC_SLASH: return KC_DELETE;
        default: return KC_NO;
    }
}

static volatile user_keyboard_conn_mode_t user_keyboard_conn;

volatile user_rf_profile_t user_rf_profile = RF_PROFILE_2_4G;

#ifdef RF_ENABLED
static bool     usb_boot_announced;

// Slider lines A (P1.0) and B (P5.6):
// (1,1)=2.4G, (1,0)=BT1, (0,1)=BT2.
static uint8_t rf_profile_switch_read(void)
{
    if (WIRELESS_MODE_SW_A) {
        if (WIRELESS_MODE_SW_B) {
            return RF_PROFILE_2_4G;
        }
        return RF_PROFILE_BT1;
    }
    if (WIRELESS_MODE_SW_B) {
        return RF_PROFILE_BT2;
    }
    // Keep the current profile while the switch moves between positions.
    return 0xFF;
}

// The slider selects the profile. If both lines are low (dead position), use
// the saved profile.
static void rf_profile_resolve(void)
{
    const uint8_t slider = rf_profile_switch_read();

    if (slider != 0xFF) {
        user_rf_profile       = (user_rf_profile_t)slider;
        user_settings.rf_link = slider; // RAM only; do not write flash here
        return;
    }

    const uint8_t saved = user_settings.rf_link;
    user_rf_profile = (saved <= RF_PROFILE_BT2) ? (user_rf_profile_t)saved : RF_PROFILE_2_4G;
}

// Send cmd 0x01 announce once per profile change.
static void rf_profile_announce(void)
{
    rf_set_link((rf_mode_t)user_rf_profile);
    rf_kbd_lazy_state_init();

    keyboard_state.rf_link   = (uint8_t)user_rf_profile;
    keyboard_state.connected = 1;
    keyboard_state.paired    = 1;

    dprintf("RF_MODE: profile %u\r\n", (unsigned)user_rf_profile);
}

static void kb_apply_conn_mode(user_keyboard_conn_mode_t mode)
{
    user_keyboard_conn = mode;

    /* The host at the other end of the link controls the lock state; clear it
     * when the transport changes. */
    keyboard_state.led_state = 0;

    switch (mode) {
        case KEYBOARD_CONN_MODE_USB:
            USBCON |= _SW2CON;
            rf_apply_usb_mode();
            usb_boot_announced = true;
            dprintf("USB_MODE\r\n");
            break;
        case KEYBOARD_CONN_MODE_RF:
            usb_boot_announced = false;
            rf_profile_announce();
            break;
    }
}
#endif // RF_ENABLED

void kb_init()
{
    USBCON |= _SW2CON;

    tap_hold_init(&k530_tap_hold_config, k530_tap_hold_entries, K530_TH_COUNT, k530_tap_hold_on_hold);

#ifdef RF_ENABLED
    // Resolved in kb_restore_rf_link() after settings load.
    user_rf_profile     = RF_PROFILE_2_4G;
    usb_boot_announced  = false;
#endif

    user_keyboard_conn = KEYBOARD_CONN_MODE_RF;
}

// The cable selects the transport, not the saved profile.
void kb_restore_rf_link()
{
#ifdef RF_ENABLED
    rf_profile_resolve();

    if (USB_DETECT && usb_is_configured()) {
        // Host enumerated: stay on USB.
        user_keyboard_conn = KEYBOARD_CONN_MODE_USB;
        rf_apply_usb_mode();
        usb_boot_announced = true;
        dprintf("USB_MODE\r\n");
        return;
    }

    rf_profile_announce();

    // Do not send cmd 0x03 here; only the user's factory-reset and RF-reset
    // gestures send it.
    dprintf("RF_MODE: boot\r\n");

    dprintf("RF_MODE: slider A=%u B=%u profile=%u\r\n",
            (unsigned)WIRELESS_MODE_SW_A, (unsigned)WIRELESS_MODE_SW_B,
            (unsigned)user_rf_profile);
#endif
}

void kb_update_switches()
{
    USB_DETECT_OUT = USB_DETECT ? 0 : 1;

#ifdef RF_ENABLED
    const uint8_t raw_conn = (USB_DETECT && usb_is_configured()) ? 1 : 0;
    static uint8_t  last_raw_conn;
    static uint16_t raw_conn_since;

    if (raw_conn != last_raw_conn) {
        last_raw_conn   = raw_conn;
        raw_conn_since  = tick_millis();
    } else if ((uint16_t)(tick_millis() - raw_conn_since) >= SWITCH_DEBOUNCE_MS) {
        if (raw_conn != (uint8_t)user_keyboard_conn) {
            kb_apply_conn_mode((user_keyboard_conn_mode_t)raw_conn);
        } else if (raw_conn && !usb_boot_announced) {
            rf_apply_usb_mode();
            usb_boot_announced = true;
        }
    }

    if (user_keyboard_conn == KEYBOARD_CONN_MODE_RF) {
        // No periodic cmd 0x03 here; only reset gestures send it.
        const uint8_t raw_profile = rf_profile_switch_read();
        static uint8_t  last_raw_profile;
        static uint16_t raw_profile_since;

        if (raw_profile != last_raw_profile) {
            last_raw_profile  = raw_profile;
            raw_profile_since = tick_millis();
        } else if (raw_profile != 0xFF && raw_profile != (uint8_t)user_rf_profile
                   && (uint16_t)(tick_millis() - raw_profile_since) >= SWITCH_DEBOUNCE_MS) {
            user_rf_profile       = (user_rf_profile_t)raw_profile;
            user_settings.rf_link = raw_profile;
            settings_mark_dirty();
            rf_profile_announce();

            dprintf("RF_MODE: switched to profile %u\r\n", (unsigned)raw_profile);
        }
    }
#endif
}

bool kb_process_record(uint16_t keycode, bool key_pressed)
{
    if (tap_hold_process(keycode, key_pressed)) {
        return false;
    }

    const int8_t arrow_index = k530_arrow_index(keycode);
    if (!key_pressed && arrow_index >= 0 && k530_virtual_keys[arrow_index]) {
        host_keycode_send(k530_virtual_keys[arrow_index], false);
        k530_virtual_keys[arrow_index] = 0;
        return false;
    }

    if (k530_fn2_held()) {
        if (keycode == SFT_ESC || keycode == KC_ESCAPE) {
#ifdef RF_ENABLED
            if (user_keyboard_conn == KEYBOARD_CONN_MODE_RF && key_pressed) {
                dprintf("LNK_PAIR: rf pairing frame\r\n");
                rf_set_link_pairing((rf_mode_t)user_rf_profile, &keyboard_state);
            }
#endif
            return false;
        }

        switch (keycode) {
            case KC_EQUAL:
            case KC_F12:
                if (key_pressed) {
                    indicators_brightness_up();
                }
                return false;
            case KC_MINUS:
            case KC_F11:
                if (key_pressed) {
                    indicators_brightness_down();
                }
                return false;
            case KC_0:
            case KC_F10:
                if (key_pressed) {
                    indicators_toggle();
                }
                return false;
            case KC_9:
            case KC_F9:
                if (key_pressed) {
                    indicators_next_mode();
                }
                return false;
            case KC_8:
            case KC_F8:
                if (key_pressed) {
                    indicators_speed_cycle();
                }
                return false;
            case KC_BSPC:
                // MR (Macro Record)
                return false;
            default:
                break;
        }
    }

    if (k530_fn_held() && (keycode == SFT_ESC || keycode == KC_ESCAPE)) {
        host_keycode_send(KC_GRAVE, key_pressed);
        return false;
    }

    if (k530_fn_held()) {
        const uint16_t fn_keycode = k530_fn_keycode(keycode);
        if (fn_keycode != KC_NO) {
            if (arrow_index >= 0) {
                k530_virtual_keys[arrow_index] = (uint8_t)fn_keycode;
            }
            host_keycode_send(fn_keycode, key_pressed);
            return false;
        }
    }

    if (tap_hold_held(K530_TH_MOD1)) {
        const uint16_t mod1_keycode = k530_arrow_keycode(keycode);
        if (mod1_keycode != KC_NO) {
            if (arrow_index >= 0) {
                k530_virtual_keys[arrow_index] = (uint8_t)mod1_keycode;
            }
            host_keycode_send(mod1_keycode, key_pressed);
            return false;
        }
    }

    switch (keycode) {
#ifdef RF_ENABLED
        case LNK_24G:
        case LNK_BT1:
        case LNK_BT2:
            if (user_keyboard_conn == KEYBOARD_CONN_MODE_RF && key_pressed) {
                switch (keycode) {
                    case LNK_24G:
                        user_rf_profile = RF_PROFILE_2_4G;
                        break;
                    case LNK_BT1:
                        user_rf_profile = RF_PROFILE_BT1;
                        break;
                    case LNK_BT2:
                        user_rf_profile = RF_PROFILE_BT2;
                        break;
                }
                user_settings.rf_link = (uint8_t)user_rf_profile;
                settings_mark_dirty();
                rf_profile_announce();
            }
            return false;

        case LNK_BT3:
            // K530 Pro slider has only 2.4G/B1/B2; there is no third BT channel.
            dprintf("LNK_BT3: not present on K530 Pro\r\n");
            return false;

        case LNK_PAIR:
            // Fn+ESC starts pairing on the current Bluetooth channel.
            if (user_keyboard_conn == KEYBOARD_CONN_MODE_RF && key_pressed) {
                dprintf("LNK_PAIR: rf pairing frame\r\n");
                rf_set_link_pairing((rf_mode_t)user_rf_profile, &keyboard_state);
            }
            return false;
#endif

        default:
            return true;
    }
}

#ifdef RF_ENABLED
/* This board cannot read radio status. The data line is wired, but it only
 * echoes bytes clocked out, so the host lock state cannot come back. On radio,
 * the backlight follows the Caps Lock key we send; USB uses the host LED
 * report. */
static void kb_track_local_caps(bool caps_down)
{
    static bool was_down;

    if (caps_down && !was_down) {
        keyboard_state.led_state ^= (1 << 1);
    }
    was_down = caps_down;
}
#endif

void kb_send_report(__xdata report_keyboard_t *report)
{
    switch (user_keyboard_conn) {
        case KEYBOARD_CONN_MODE_USB:
            usb_send_report(report);
            break;
#ifdef RF_ENABLED
        case KEYBOARD_CONN_MODE_RF: {
            bool caps_down = false;
            for (uint8_t i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
                if (report->keys[i] == KC_CAPS_LOCK) {
                    caps_down = true;
                }
            }
            kb_track_local_caps(caps_down);
            rf_send_report(report);
            break;
        }
#endif
    }
}

void kb_send_nkro(__xdata report_nkro_t *report)
{
    switch (user_keyboard_conn) {
        case KEYBOARD_CONN_MODE_USB:
            usb_send_nkro(report);
            break;
#ifdef RF_ENABLED
        case KEYBOARD_CONN_MODE_RF:
            kb_track_local_caps((report->bits[KC_CAPS_LOCK >> 3] & (1 << (KC_CAPS_LOCK & 7))) != 0);
            rf_send_nkro(report);
            break;
#endif
    }
}

void kb_send_extra(__xdata report_extra_t *report)
{
    switch (user_keyboard_conn) {
        case KEYBOARD_CONN_MODE_USB:
            usb_send_extra(report);
            break;
#ifdef RF_ENABLED
        case KEYBOARD_CONN_MODE_RF:
            rf_send_extra(report);
            break;
#endif
    }
}

void kb_update()
{
    tap_hold_task();

#ifdef RF_ENABLED
    if (user_keyboard_conn == KEYBOARD_CONN_MODE_RF) {
        rf_link_supervisor(&keyboard_state);

        rf_send_pending_flush();

        rf_blanking_tick();
    }
#endif
}
