#include "rf_controller.h"
#include <stdint.h>
#include "delay.h"
#include "debug.h"
#include "bb_spi.h"
#include "kbdef.h"

#ifndef RF_REPORT_KEY_COUNT
#    define RF_REPORT_KEY_COUNT 5
#endif
#ifndef RF_NKRO_BITMAP_BYTES
#    define RF_NKRO_BITMAP_BYTES 20
#endif
#ifndef RF_REPORT_HAS_ACTIVITY_BYTE
#    define RF_REPORT_HAS_ACTIVITY_BYTE 1
#endif
#ifndef RF_REPORT_KRO_PHANTOM_BLANKING
#    define RF_REPORT_KRO_PHANTOM_BLANKING 1
#endif
#ifndef RF_USB_MODE_FRAME_COUNT
#    define RF_USB_MODE_FRAME_COUNT 2
#endif
#ifndef RF_STATUS_READBACK
#    define RF_STATUS_READBACK 1
#endif
#ifndef RF_INIT_BT_NAMES
#    define RF_INIT_BT_NAMES 1
#endif
#ifndef RF_LINK_FRAME_COUNT
#    define RF_LINK_FRAME_COUNT 2
#endif
#ifndef RF_EXTRA_FIXED_BYTE8
#    define RF_EXTRA_FIXED_BYTE8 0
#endif
#ifndef RF_INIT_SEND_LINK
#    define RF_INIT_SEND_LINK 1
#endif

// K530: cmd 0x05 media frame uses this layout (byte 8 = 1, usage in
// bytes 9..10). Other boards keep their own layout.
#ifndef RF_EXTRA_FRAME_MEDIA_LAYOUT
#    define RF_EXTRA_FRAME_MEDIA_LAYOUT 0
#endif

#define MAGIC_BYTE 0xaa
#define CMD_REPORT 0x02

typedef enum {
    RF_PAIRING_OFF = 0x00,
    RF_PAIRING_ON  = 0x01
} rf_pairing_t;

typedef enum {
    RF_SET_NAME_BT5 = 0x00,
    RF_SET_NAME_BT3 = 0x01
} rf_set_name_t;

static const __code char rf_bt5_name[] = "SMK BT5.0";
static const __code char rf_bt3_name[] = "SMK BT3.0";

uint8_t rf_tx_buf[32];

static uint8_t kro_prev_active;
static uint8_t blanking_pending;
static bool    blanking_active;

static bool mac_mode_compat;
static bool byte9_disable;

void rf_set_mac_mode_compat(bool is_mac)
{
    mac_mode_compat = is_mac;
}

void rf_byte9_set_disable(bool on)
{
    byte9_disable = on;
}

static uint8_t compute_byte9(bool curr_active)
{
    if (mac_mode_compat) return 0;
    if (curr_active) return 0;
    if (byte9_disable) return 0;
    return 1;
}

bool    rf_get_status(uint8_t status_bytes[2]);
void    rf_set_link_mode(uint8_t mode, uint8_t pairing);
bool    rf_send_kro_report(uint8_t *buffer);
void    rf_send_nkro_report(uint8_t mods, __xdata uint8_t *nkro_buffer);
void    rf_cmd_03(uint8_t param);
void    rf_cmd_04();
void    rf_send_consumer_system(uint16_t consumer, uint16_t system);
void    rf_cmd_06(uint8_t param);
void    rf_sleep(uint8_t param);
void    rf_set_bt_name(uint8_t type, char *name);
void    rf_query_status();
void    rf_wake();
void    rf_wake_nudge();
void    rf_fetch_4();
uint8_t checksum(uint8_t *data, int len);

void rf_init()
{
#if RF_STATUS_READBACK
    uint8_t status_bytes[2];
#endif

    delay_ms(255);
    delay_ms(255);
    delay_ms(255);
    delay_ms(255);
    delay_ms(255);
    delay_ms(255);

#if RF_STATUS_READBACK
    for (uint8_t tries = 10; tries > 0; tries--) {
        rf_wake_nudge();
        delay_ms(tries);
        if (rf_get_status(status_bytes) && (status_bytes[0] & 0x80)) {
            break;
        }
    }
#endif

#if RF_INIT_BT_NAMES
    rf_set_bt_name(RF_SET_NAME_BT5, rf_bt5_name);
    delay_ms(50);
    rf_set_bt_name(RF_SET_NAME_BT3, rf_bt3_name);
    delay_ms(5);
#endif

#if RF_INIT_SEND_LINK
    rf_set_link(RF_MODE_2_4G);
#endif
}

void rf_reassert_link(rf_mode_t link)
{
    rf_set_link_mode((uint8_t)link, 0);
#if RF_LINK_FRAME_COUNT > 1
    delay_ms(20);
    rf_set_link_mode((uint8_t)link, 0);
#endif
}

// Wipe all BT bonds / factory reset. Destructive: tears down BLE advertising
// and committed BT names, so rf_init must re-run afterwards. Recovers a BK3632
// that's accumulated bad BLE state across repeated failed pairings (rotating-MAC
// advertising the host can't pair against because SMP keeps timing out).
void rf_factory_reset_bonds(void)
{
    rf_cmd_03(2); // wipe stored bonds
    delay_ms(200);
    rf_sleep(0);
    delay_ms(100);
    rf_wake();
    delay_ms(200);
    rf_init(); // reload BT names cleared by the wipe + sleep cycle
    delay_ms(200);
}

uint8_t kro6buffer[RF_REPORT_KEY_COUNT + 1];

static uint8_t rf_pending_buf[RF_REPORT_KEY_COUNT + 1];
static bool    rf_pending;

void rf_send_report(__xdata report_keyboard_t *report)
{
    rf_pending_buf[0] = report->raw[0];
    rf_pending_buf[1] = report->raw[2];
    rf_pending_buf[2] = report->raw[3];
    rf_pending_buf[3] = report->raw[4];
    rf_pending_buf[4] = report->raw[5];
    rf_pending_buf[5] = report->raw[6];
#if RF_REPORT_KEY_COUNT == 6
    rf_pending_buf[6] = report->raw[7];
#endif
    rf_pending        = true;

    rf_send_pending_flush();
}

void rf_send_pending_flush(void)
{
    if (!rf_pending) return;
    if (rf_send_kro_report(rf_pending_buf)) {
        rf_pending = false;
    }
}

void rf_send_nkro(__xdata report_nkro_t *report)
{
    bool blank = true;
    for (int i = 1; i < NKRO_REPORT_SIZE - 1; i++) {
        if (report->raw[i] != 0) {
            blank = false;
        }
    }

    if (blank) {
        for (int i = 0; i < RF_REPORT_KEY_COUNT + 1; i++) {
            kro6buffer[i] = 0;
        }
        rf_send_kro_report(kro6buffer);
    } else {
        rf_send_nkro_report(report->mods, report->bits);
    }
}

void rf_send_extra(__xdata report_extra_t *report)
{
    switch (report->report_id) {
        case REPORT_ID_SYSTEM:
            rf_send_consumer_system(0, report->usage);
            break;
        case REPORT_ID_CONSUMER:
            rf_send_consumer_system(report->usage, 0);
            break;
    }
}

bool rf_update_keyboard_state(keyboard_state_t *keyboard)
{
    uint8_t status_bytes[2];

    if (!rf_get_status(status_bytes)) {
        return false; // no fresh status - leave keyboard_state untouched
    }

    if (!(status_bytes[0] & 0x80)) {
        rf_wake_nudge();
    }

    keyboard->battery_level = status_bytes[0] & 0x07;

    keyboard->led_state = status_bytes[1] & ((1 << 0) | (1 << 1) | (1 << 2));
    keyboard->connected = (status_bytes[1] >> 3) & 1;
    keyboard->paired    = (status_bytes[1] >> 4) & 1;
    keyboard->low_power = (status_bytes[1] >> 7) & 1;

    const uint8_t old_rf_link = keyboard->rf_link;
    keyboard->rf_link         = ((status_bytes[1] & ((1 << 5) | (1 << 6))) >> 5);
    dprintf_if(old_rf_link != keyboard->rf_link, "rf link changed %02x\r\n", keyboard->rf_link);

    return true;
}

#define RF_SUPERVISOR_TICK_INTERVAL 2000u
#define RF_PAIRING_WINDOW_POLLS     600u

static uint8_t  commanded_link       = RF_MODE_2_4G;
static uint16_t pairing_window_polls = 0;

static uint16_t supervisor_ticks      = 0;
static uint8_t  supervisor_was_paired = 0;

void rf_link_supervisor(keyboard_state_t *keyboard)
{
#if !RF_STATUS_READBACK
    (void)keyboard;
    return;
#else
    supervisor_ticks++;
    if (supervisor_ticks < RF_SUPERVISOR_TICK_INTERVAL) {
        return;
    }
    supervisor_ticks = 0;

    supervisor_was_paired = keyboard->paired;

    if (!rf_update_keyboard_state(keyboard)) {
        return;
    }

    if (keyboard->paired && !supervisor_was_paired) {
        pairing_window_polls = 0;
        rf_reassert_link((rf_mode_t)commanded_link);
        return;
    }

    if (pairing_window_polls != 0) {
        pairing_window_polls--;
        return;
    }

    if ((!keyboard->connected && !keyboard->paired) || (keyboard->rf_link != commanded_link)) {
        rf_set_link_mode(commanded_link, 0);
    }
#endif
}

void rf_set_link(rf_mode_t link)
{
    commanded_link = (uint8_t)link;
    rf_set_link_mode(link, 0);
#if RF_LINK_FRAME_COUNT > 1
    delay_ms(20);
    rf_set_link_mode(link, 0);
#endif
}

void rf_apply_usb_mode(void)
{
    rf_cmd_06(1);
#if RF_USB_MODE_FRAME_COUNT > 1
    delay_ms(20);
    rf_cmd_06(1);
#endif
}

static bool lazy_init_pending;

void rf_kbd_lazy_state_init(void)
{
    lazy_init_pending = true;
    kro_prev_active   = 0;
    blanking_pending  = 0;
}

void rf_blanking_tick(void)
{
    if (blanking_pending == 0) {
        return;
    }

#if RF_REPORT_KRO_PHANTOM_BLANKING
    static uint8_t phantom_buf[6] = {0, 0x01, 0, 0, 0, 0};
    static uint8_t blank_buf[6]   = {0, 0, 0, 0, 0, 0};
    uint8_t       *buf            = ((blanking_pending & 1) == 0) ? phantom_buf : blank_buf;
#else
    static uint8_t blank_buf[RF_REPORT_KEY_COUNT + 1] = {0};
    uint8_t       *buf = blank_buf;
#endif

    blanking_pending--;

    blanking_active = true;
    rf_send_kro_report(buf);
    blanking_active = false;
}
static uint8_t pairing_status_bytes[2];
static uint8_t pairing_paired_now;

void rf_set_link_pairing(rf_mode_t link, __xdata keyboard_state_t *keyboard)
{
    commanded_link = (uint8_t)link;

    rf_set_link_mode(link, 1);

#if !RF_STATUS_READBACK
    (void)keyboard;
    pairing_paired_now = 0;
    pairing_window_polls = 0;
    return;
#else
    pairing_paired_now = 0;
    delay_ms(100);
    for (uint8_t tries = 10; tries > 0; tries--) {
        rf_wake_nudge();
        delay_ms(10);
        if (rf_get_status(pairing_status_bytes) && (pairing_status_bytes[0] & 0x80)) {
            keyboard->battery_level = pairing_status_bytes[0] & 0x07;
            keyboard->led_state     = pairing_status_bytes[1] & ((1 << 0) | (1 << 1) | (1 << 2));
            keyboard->connected     = (pairing_status_bytes[1] >> 3) & 1;
            keyboard->paired        = (pairing_status_bytes[1] >> 4) & 1;
            keyboard->low_power     = (pairing_status_bytes[1] >> 7) & 1;
            keyboard->rf_link       = ((pairing_status_bytes[1] & ((1 << 5) | (1 << 6))) >> 5);
            if (keyboard->paired) {
                pairing_paired_now = 1;
                break;
            }
        }
        delay_ms(20);
    }

    if (pairing_paired_now) {
        delay_ms(50);
        rf_reassert_link(link);
    } else {
        pairing_window_polls = RF_PAIRING_WINDOW_POLLS;
    }
#endif
}

bool rf_get_status(uint8_t status_bytes[2])
{
    const uint8_t len = 4;

    rf_query_status();
    delay_us(100);
    rf_fetch_4();

    if (rf_tx_buf[0] != 0xBB) {
        rf_wake_nudge();
        return false;
    }

    uint8_t expected_sum = checksum(rf_tx_buf + 2, len - 2);
    if (rf_tx_buf[1] != expected_sum) {
        rf_wake_nudge();
        return false;
    }

    status_bytes[0] = rf_tx_buf[2];
    status_bytes[1] = rf_tx_buf[3];

    return true;
}

void rf_fetch_4()
{
    rf_tx_buf[0] = 0xff;
    rf_tx_buf[1] = 0xff;
    rf_tx_buf[2] = 0xff;
    rf_tx_buf[3] = 0xff;

    bb_spi_recv(rf_tx_buf, 4);
}

// Never nudge between attempts: it reuses rf_tx_buf and corrupts the in-flight report.
#define RF_SEND_MAX_ATTEMPTS 5

static bool rf_send_or_retry(uint8_t *buf, int len)
{
    for (uint8_t attempt = 0; attempt < RF_SEND_MAX_ATTEMPTS; attempt++) {
        if (bb_spi_xfer(buf, len)) {
            return true;
        }
    }
    return false;
}

void rf_set_link_mode(uint8_t mode, uint8_t pairing)
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x01;
    rf_tx_buf[3] = pairing;
    rf_tx_buf[4] = mode;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, 6);
}

#define BLANKING_COUNT_AFTER_RELEASE 6

static void rf_kro_post_send_blanking(bool curr_active)
{
    if (blanking_active) {
        return; // inside rf_blanking_tick; the tick owns the counter
    }
#if RF_REPORT_KRO_PHANTOM_BLANKING
    if (curr_active) {
        blanking_pending = 0; // a press cancels any pending blanking
    } else if (kro_prev_active) {
        blanking_pending = BLANKING_COUNT_AFTER_RELEASE;
    }
#else
    // Nothing to blank: the release report is already empty, and extra frames
    // would only stall the bit-banged SPI.
    (void)curr_active;
    blanking_pending = 0;
#endif
}

bool rf_send_kro_report(uint8_t *buffer)
{
    const uint8_t len = 32;

    static uint8_t empty_buf[RF_REPORT_KEY_COUNT + 1] = {0};
    if (lazy_init_pending) {
        lazy_init_pending = false;
        buffer            = empty_buf;
    }

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = CMD_REPORT;
    rf_tx_buf[3] = buffer[0];
    for (uint8_t i = 1; i <= RF_REPORT_KEY_COUNT; i++) {
        rf_tx_buf[3 + i] = buffer[i];
    }

    uint8_t active = (buffer[0] != 0);
    for (uint8_t i = 1; i <= RF_REPORT_KEY_COUNT; i++) {
        if (buffer[i] != 0) {
            active = 1;
            break;
        }
    }

#if RF_REPORT_KEY_COUNT == 6
    // The K530 dongle puts the 6th key in byte 9 instead of the activity byte.
    rf_tx_buf[9] = buffer[6];
#else
    rf_tx_buf[9] = compute_byte9(active);
#endif

    for (int i = 10; i < 31; i++) {
        rf_tx_buf[i] = 0x00;
    }

    rf_tx_buf[31] = checksum(rf_tx_buf, len - 1);

    const bool ack = rf_send_or_retry(rf_tx_buf, len);

    rf_kro_post_send_blanking(active);
    kro_prev_active = active;
    return ack;
}

void rf_send_nkro_report(uint8_t mods, __xdata uint8_t *nkro_buffer)
{
    const uint8_t len = 32;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = CMD_REPORT;
    rf_tx_buf[3] = mods;

    rf_tx_buf[4] = 0x00;
    rf_tx_buf[5] = 0x00;
    rf_tx_buf[6] = 0x00;
    rf_tx_buf[7] = 0x00;
    rf_tx_buf[8] = RF_EXTRA_FIXED_BYTE8 ? 0x01 : 0x00;

    uint8_t active = (mods != 0);
    if (!active) {
        for (uint8_t i = 0; i < RF_NKRO_BITMAP_BYTES; i++) {
            if (nkro_buffer[i] != 0) {
                active = 1;
                break;
            }
        }
    }
 #if RF_REPORT_HAS_ACTIVITY_BYTE
    rf_tx_buf[9] = compute_byte9(active);
 #else
    rf_tx_buf[9] = 0;
 #endif

    for (uint8_t i = 0; i < RF_NKRO_BITMAP_BYTES; i++) {
        rf_tx_buf[10 + i] = nkro_buffer[i];
    }
#if RF_NKRO_BITMAP_BYTES < 20
    for (uint8_t i = RF_NKRO_BITMAP_BYTES; i < 20; i++) {
        rf_tx_buf[10 + i] = 0;
    }
#endif

    rf_tx_buf[30] = 0x00;

    rf_tx_buf[31] = checksum(rf_tx_buf, len - 1);

    rf_send_or_retry(rf_tx_buf, len);

    rf_kro_post_send_blanking(active);
    kro_prev_active = active;
}

void rf_cmd_03(uint8_t param)
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x03;
    rf_tx_buf[3] = param;
    rf_tx_buf[4] = 0x00;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, 6);
}

void rf_cmd_04()
{
    const uint8_t len = 4;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x04;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, 3);

    bb_spi_xfer(rf_tx_buf, 4);
}

void rf_send_consumer_system(uint16_t consumer, uint16_t system)
{
    const uint8_t len = 14;

    rf_tx_buf[0]  = MAGIC_BYTE;
    rf_tx_buf[1]  = len - 3;
    rf_tx_buf[2]  = 0x05;
    rf_tx_buf[3]  = 0x00;
    rf_tx_buf[4]  = 0x00;
    rf_tx_buf[5]  = 0x00;
    rf_tx_buf[6]  = 0x00;
    rf_tx_buf[7]  = 0x00;
#if RF_EXTRA_FRAME_MEDIA_LAYOUT
    // Byte 8 = 1, usage in bytes 9..10, bytes 11..12 = 0.
    rf_tx_buf[8]  = 0x01;
    rf_tx_buf[9]  = consumer ? (consumer & 0xff) : (system & 0xff);
    rf_tx_buf[10] = consumer ? (consumer >> 8)   : (system >> 8);
    rf_tx_buf[11] = 0x00;
    rf_tx_buf[12] = 0x00;
#else
    rf_tx_buf[8]  = 0x00;
    rf_tx_buf[9]  = consumer & 0xff;
    rf_tx_buf[10] = consumer >> 8;
    rf_tx_buf[11] = system & 0xff;
    rf_tx_buf[12] = system >> 8;
#endif

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    rf_send_or_retry(rf_tx_buf, len);
}

void rf_cmd_06(uint8_t param) // 0x00 or 0x01
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x06;
    rf_tx_buf[3] = param;
    rf_tx_buf[4] = 0x00;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, len);
}

void rf_sleep(uint8_t param)
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x07;
    rf_tx_buf[3] = param;
    rf_tx_buf[4] = 0x00;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, len);
}

void rf_set_bt_name(uint8_t type, char *name)
{
    const uint8_t len = 32;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x08;
    rf_tx_buf[3] = type;

    uint8_t name_len = 0;
    int     i        = 5;
    while (*name && i < (len - 1)) {
        rf_tx_buf[i] = *name;
        name++;
        i++;
        name_len++;
    }
    rf_tx_buf[4] = name_len;

    for (int j = i; j < (len - 1); j++) {
        rf_tx_buf[j] = 0x00;
    }

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, len);
    delay_ms(20);
    rf_cmd_04();
}

void rf_query_status()
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x0a;
    rf_tx_buf[3] = 0x00;
    rf_tx_buf[4] = 0x00;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, len);
}

void rf_wake()
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x0b;
    rf_tx_buf[3] = 0x00;
    rf_tx_buf[4] = 0x00;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, len);
}

void rf_wake_nudge()
{
    const uint8_t len = 6;

    rf_tx_buf[0] = MAGIC_BYTE;
    rf_tx_buf[1] = len - 3;
    rf_tx_buf[2] = 0x0c;
    rf_tx_buf[3] = 0x00;
    rf_tx_buf[4] = 0x00;

    rf_tx_buf[len - 1] = checksum(rf_tx_buf, len - 1);

    bb_spi_xfer(rf_tx_buf, len);
}

uint8_t checksum(uint8_t *data, int len)
{
    uint8_t checksum = 0;

    for (int i = 0; i < len; i++) {
        checksum += data[i];
    }

    return 0x55 - checksum;
}
