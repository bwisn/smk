#pragma once

#include "sh68f90.h"
#include "keycodes.h"

#define MATRIX_ROWS 5
#define MATRIX_COLS 14

#define KB_R0_P7_1 _P7_1
#define KB_R1_P7_2 _P7_2
#define KB_R2_P7_3 _P7_3
#define KB_R3_P5_3 _P5_3
#define KB_R4_P5_4 _P5_4

#define KB_R0 P7_1
#define KB_R1 P7_2
#define KB_R2 P7_3
#define KB_R3 P5_3
#define KB_R4 P5_4

#define KB_C0_P5_0  _P5_0
#define KB_C1_P5_1  _P5_1
#define KB_C2_P5_2  _P5_2
#define KB_C3_P3_5  _P3_5
#define KB_C4_P3_4  _P3_4
#define KB_C5_P3_3  _P3_3
#define KB_C6_P3_2  _P3_2
#define KB_C7_P3_1  _P3_1
#define KB_C8_P3_0  _P3_0
#define KB_C9_P2_5  _P2_5
#define KB_C10_P2_4 _P2_4
#define KB_C11_P2_3 _P2_3
#define KB_C12_P2_2 _P2_2
#define KB_C13_P2_1 _P2_1

#define KB_C0  P5_0
#define KB_C1  P5_1
#define KB_C2  P5_2
#define KB_C3  P3_5
#define KB_C4  P3_4
#define KB_C5  P3_3
#define KB_C6  P3_2
#define KB_C7  P3_1
#define KB_C8  P3_0
#define KB_C9  P2_5
#define KB_C10 P2_4
#define KB_C11 P2_3
#define KB_C12 P2_2
#define KB_C13 P2_1

#define LED_PWM_C0  PWM40
#define LED_PWM_C1  PWM41
#define LED_PWM_C2  PWM42
#define LED_PWM_C3  PWM05
#define LED_PWM_C4  PWM04
#define LED_PWM_C5  PWM03
#define LED_PWM_C6  PWM02
#define LED_PWM_C7  PWM01
#define LED_PWM_C8  PWM00
#define LED_PWM_C9  PWM15
#define LED_PWM_C10 PWM14
#define LED_PWM_C11 PWM13
#define LED_PWM_C12 PWM12
#define LED_PWM_C13 PWM11
#define LED_PWM_C14 PWM10
#define LED_PWM_C15 PWM25
#define LED_PWM_C16 PWM24
#define LED_PWM_C17 PWM23
#define LED_PWM_C18 PWM22

#define RGB_R0R P0_4
#define RGB_R0G P6_1
#define RGB_R0B P0_3
#define RGB_R1R P6_7
#define RGB_R1G P6_2
#define RGB_R1B P6_6
#define RGB_R2R P0_2
#define RGB_R2G P6_3
#define RGB_R2B P5_7
#define RGB_R3R P4_5
#define RGB_R3G P6_4
#define RGB_R3B P4_6
#define RGB_R4R P4_3
#define RGB_R4G P6_5
#define RGB_R4B P4_4
#define RGB_ULR P4_1
#define RGB_ULG P6_0
#define RGB_ULB P4_0

#define RGB_R0R_P0_4 _P0_4
#define RGB_R0G_P6_1 _P6_1
#define RGB_R0B_P0_3 _P0_3
#define RGB_R1R_P6_7 _P6_7
#define RGB_R1G_P6_2 _P6_2
#define RGB_R1B_P6_6 _P6_6
#define RGB_R2R_P0_2 _P0_2
#define RGB_R2G_P6_3 _P6_3
#define RGB_R2B_P5_7 _P5_7
#define RGB_R3R_P4_5 _P4_5
#define RGB_R3G_P6_4 _P6_4
#define RGB_R3B_P4_6 _P4_6
#define RGB_R4R_P4_3 _P4_3
#define RGB_R4G_P6_5 _P6_5
#define RGB_R4B_P4_4 _P4_4
#define RGB_ULR_P4_1 _P4_1
#define RGB_ULG_P6_0 _P6_0
#define RGB_ULB_P4_0 _P4_0

// P4.2 is the INT4 wake input.
#define KB_WAKE_P4_2 _P4_2

/*
 * Redragon K530 Pro hardware map.
 *
 * No USB/RF switch pin and no OS/layout pin.
 * Switches: power ON/OFF (cuts the battery, not connected to the MCU) and a
 * 3-position wireless slider "2.4G / B1 / B2" on two GPIOs:
 *   (P1_0, P5_6) = (1, 1) -> profile 0 = 2.4G dongle
 *   (P1_0, P5_6) = (1, 0) -> profile 1 = Bluetooth channel 1 (B1)
 *   (P1_0, P5_6) = (0, 1) -> profile 2 = Bluetooth channel 2 (B2)
 *   (0, 0)               -> dead position, keep the current profile
 * Transport is selected at runtime by cable presence (USB_DETECT).
 */

#define WIRELESS_MODE_SW_A   P1_0
#define WIRELESS_MODE_SW_B   P5_6

#define WIRELESS_MODE_SW_A_P _P1_0
#define WIRELESS_MODE_SW_B_P _P5_6

#define USB_DETECT   P7_5
#define USB_DETECT_P _P7_5

#define USB_DETECT_OUT   P7_6
#define USB_DETECT_OUT_P _P7_6

#define RF_BB_SPI_CS   P7_4
#define RF_BB_SPI_SCK  P4_7
#define RF_BB_SPI_MISO P0_6
#define RF_BB_SPI_MOSI P0_7
#define RF_BB_SPI_MOT  P0_5

#define RF_BB_SPI_CS_P7_4   _P7_4
#define RF_BB_SPI_SCK_P4_7  _P4_7
#define RF_BB_SPI_MISO_P0_6 _P0_6
#define RF_BB_SPI_MOSI_P0_7 _P0_7
#define RF_BB_SPI_MOT_P0_5  _P0_5

// RF reports use six key slots and a 14-byte bitmap.
#define RF_REPORT_KEY_COUNT   6
#define RF_NKRO_BITMAP_BYTES 14
#define RF_REPORT_HAS_ACTIVITY_BYTE 0
#define RF_REPORT_KRO_PHANTOM_BLANKING 0
#define RF_USB_MODE_FRAME_COUNT 1
#define RF_STATUS_READBACK 0
#define RF_LINK_FRAME_COUNT 1
#define RF_INIT_SEND_LINK 0
#define RF_EXTRA_FIXED_BYTE8 0

#define RF_BB_SPI_HAS_ACK 0

// Media frame (cmd 0x05): byte 8 = 1, usage code in bytes 9..10, bytes 11..12
// = 0.
#define RF_EXTRA_FRAME_MEDIA_LAYOUT 1

// One LED phase must equal one backlight PWM cycle (0x0500 counts at
// SYSCLK/4 = 213.3 us).
#ifndef LED_SUBFRAME_US
#define LED_SUBFRAME_US 213
#endif

// The scan takes ~110 us; the margin covers the USB interrupt.
#ifndef MATRIX_SCAN_SLOT_US
#define MATRIX_SCAN_SLOT_US 250
#endif

// Sample a column in ~5 us. Keep the settles short so the scan stays near that
// time.
#ifndef MATRIX_SETTLE_US
#define MATRIX_SETTLE_US 3
#endif
#ifndef MATRIX_SAMPLE_GAP_US
#define MATRIX_SAMPLE_GAP_US 3
#endif

// Run the matrix scan in the LED frame-wrap slot (PWM phase 0). Scan rate =
// LED frame rate (~240 Hz).
#define LED_SCAN_IN_FRAME_SLOT 1

enum custom_keycodes {
    LNK_24G = SAFE_RANGE,
    LNK_BT1,
    LNK_BT2,
    LNK_BT3,
    LNK_PAIR, // Bluetooth pairing (vendor: FN2+ESC); map it in the layout

    K530_TAP_UP,
    K530_TAP_LEFT,
    K530_TAP_DOWN,
    K530_TAP_RIGHT,
    K530_MOD1,
    SFT_ESC,

    KB_SAFE_RANGE,
};
