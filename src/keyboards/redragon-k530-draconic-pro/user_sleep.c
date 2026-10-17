#include "user_sleep.h"

#ifdef SLEEP_ENABLE

#include "kbdef.h"
#include "gpio.h"
#include "user_init.h"
#include "extint.h"

/*
 * Drive matrix and LED pins low while the MCU clock is stopped. P4.2 is the
 * wake input.
 */
user_sleep_mode_t user_sleep_supported(void)
{
    return USB_DETECT ? USER_SLEEP_USB : USER_SLEEP_RF;
}

static void k530_sleep_park(void)
{
    const uint8_t p2_cols = KB_C9_P2_5 | KB_C10_P2_4 | KB_C11_P2_3 |
                            KB_C12_P2_2 | KB_C13_P2_1;
    const uint8_t p3_cols = KB_C3_P3_5 | KB_C4_P3_4 | KB_C5_P3_3 |
                            KB_C6_P3_2 | KB_C7_P3_1 | KB_C8_P3_0;
    const uint8_t p5_cols = KB_C0_P5_0 | KB_C1_P5_1 | KB_C2_P5_2;
    const uint8_t p0_rgb = RGB_R2R_P0_2 | RGB_R0B_P0_3 | RGB_R0R_P0_4;
    const uint8_t p4_rgb = RGB_R4B_P4_4 | RGB_R4R_P4_3 | RGB_R3R_P4_5 |
                           RGB_R3B_P4_6 | RGB_ULB_P4_0 | RGB_ULR_P4_1;
    const uint8_t p5_rgb = RGB_R2B_P5_7;
    const uint8_t p6_rgb = RGB_ULG_P6_0 | RGB_R0G_P6_1 | RGB_R1G_P6_2 |
                           RGB_R2G_P6_3 | RGB_R3G_P6_4 | RGB_R4G_P6_5 |
                           RGB_R1B_P6_6 | RGB_R1R_P6_7;

    GPIO_PULLUP_OFF(2, p2_cols);
    GPIO_PULLUP_OFF(3, p3_cols);
    GPIO_PULLUP_OFF(5, p5_cols);

    GPIO_OUTPUT(2, p2_cols);
    GPIO_OUTPUT(3, p3_cols);
    GPIO_OUTPUT(5, p5_cols);
    GPIO_LOW(2, p2_cols);
    GPIO_LOW(3, p3_cols);
    GPIO_LOW(5, p5_cols);

    GPIO_PULLUP_OFF(0, p0_rgb);
    GPIO_PULLUP_OFF(4, p4_rgb);
    GPIO_PULLUP_OFF(5, p5_rgb);
    GPIO_PULLUP_OFF(6, p6_rgb);
    GPIO_OUTPUT(0, p0_rgb);
    GPIO_OUTPUT(4, p4_rgb);
    GPIO_OUTPUT(5, p5_rgb);
    GPIO_OUTPUT(6, p6_rgb);
    GPIO_LOW(0, p0_rgb);
    GPIO_LOW(4, p4_rgb);
    GPIO_LOW(5, p5_rgb);
    GPIO_LOW(6, p6_rgb);

    GPIO_PULLUP_OFF(4, KB_WAKE_P4_2);
    GPIO_INPUT(4, KB_WAKE_P4_2);

    GPIO_INPUT(4, RF_BB_SPI_SCK_P4_7);
    GPIO_INPUT(0, RF_BB_SPI_MOT_P0_5 | RF_BB_SPI_MOSI_P0_7);
    GPIO_INPUT(7, RF_BB_SPI_CS_P7_4);
}

void user_sleep_prepare(void)
{
    k530_sleep_park();
    extint_wake_arm();
}

void user_sleep_wake(void)
{
    extint_wake_disable();
    user_gpio_init();
}

#endif // SLEEP_ENABLE
