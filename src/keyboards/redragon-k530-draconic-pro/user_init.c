#include "kbdef.h"
#include "user_init.h"
#include "pwm.h"
#include "gpio.h"

#define PWM_PERD 0x0500

#define PWM_DUTY1 (uint16_t)0
#define PWM_DUTY2 (uint16_t)0

#define PWM_PERDH_INIT ((uint8_t)(PWM_PERD >> 8))
#define PWM_PERDL_INIT ((uint8_t)(PWM_PERD))

void user_gpio_init();
void user_pwm_init();

void user_init()
{
    user_gpio_init();
    user_pwm_init();
}

void user_gpio_init()
{
    DRVCON = DRVCON_UNLOCK_P1;
    P1DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P2;
    P2DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P3;
    P3DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P5;
    P5DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_LOCK;

    GPIO_DIR_WRITE(0, (uint8_t)(RGB_R2R_P0_2 | RGB_R0B_P0_3 | RGB_R0R_P0_4));
    // P1CR = 0x3E: P1.1..P1.5 are outputs. The mode switch ties slider line A
    // to one of them. P1.0 is the input.
    GPIO_DIR_WRITE(1, 0x3E);
    P1 = 0x00;
    GPIO_DIR_WRITE(2, (uint8_t)(KB_C13_P2_1 | KB_C12_P2_2 | KB_C11_P2_3 | KB_C10_P2_4 | KB_C9_P2_5));
    GPIO_DIR_WRITE(3, (uint8_t)(KB_C8_P3_0 | KB_C7_P3_1 | KB_C6_P3_2 | KB_C5_P3_3 | KB_C4_P3_4 | KB_C3_P3_5));
    GPIO_DIR_WRITE(4, (uint8_t)(RGB_R4B_P4_4 | RGB_R4R_P4_3 | RGB_R3R_P4_5 | RGB_R3B_P4_6 | RGB_ULB_P4_0 | RGB_ULR_P4_1));
    GPIO_DIR_WRITE(5, (uint8_t)(KB_C0_P5_0 | KB_C1_P5_1 | KB_C2_P5_2 | RGB_R2B_P5_7)); // Keep P5.6 as input.
    GPIO_DIR_WRITE(6, (uint8_t)(RGB_ULG_P6_0 | RGB_R0G_P6_1 | RGB_R1G_P6_2 | RGB_R2G_P6_3 | RGB_R3G_P6_4 | RGB_R4G_P6_5 | RGB_R1B_P6_6 | RGB_R1R_P6_7));

    // Matrix rows, slider line B, and P7.0 (unused sixth row).
    GPIO_PULLUP_WRITE(7, (uint8_t)(KB_R0_P7_1 | KB_R1_P7_2 | KB_R2_P7_3 | _P7_0 | USB_DETECT_OUT_P));
    GPIO_PULLUP_WRITE(5, (uint8_t)(KB_R3_P5_3 | KB_R4_P5_4 | WIRELESS_MODE_SW_B_P));

    // Write P1PCR = 0x3F outside the DRVCON window; inside it, the pull-up
    // would prevent the mode switch from pulling line A low.
    GPIO_PULLUP_ON(1, (uint8_t)WIRELESS_MODE_SW_A_P);

    GPIO_PULLUP_WRITE(0, (uint8_t)(RGB_R2R_P0_2 | RGB_R0B_P0_3 | RGB_R0R_P0_4 | RF_BB_SPI_MISO_P0_6 | RF_BB_SPI_MOSI_P0_7 | RF_BB_SPI_MOT_P0_5));
    GPIO_PULLUP_WRITE(4, (uint8_t)(RGB_R4B_P4_4 | RGB_R4R_P4_3 | RGB_R3R_P4_5 | RGB_R3B_P4_6 | _P4_0 | _P4_1 | RF_BB_SPI_SCK_P4_7));

    // USB cable sense: P7.5 is a plain input with no pull-up (VBUS divider on
    // the board). P7.6 mirrors !USB_DETECT; P7.4 is the RF chip select.
    GPIO_INPUT(7, USB_DETECT_P);
    GPIO_OUTPUT(7, USB_DETECT_OUT_P);
    USB_DETECT_OUT = USB_DETECT ? 0 : 1;

    // Keep the SPI lines high between bursts.
    GPIO_HIGH(7, RF_BB_SPI_CS_P7_4);
    GPIO_HIGH(4, RF_BB_SPI_SCK_P4_7);
    GPIO_HIGH(0, (RF_BB_SPI_MOSI_P0_7 | RF_BB_SPI_MOT_P0_5));
}

void user_pwm_init()
{
    PWM0PERDH = PWM_PERDH_INIT;
    PWM0PERDL = PWM_PERDL_INIT;

    PWM1PERDH = PWM_PERDH_INIT;
    PWM1PERDL = PWM_PERDL_INIT;

    PWM2PERDH = PWM_PERDH_INIT;
    PWM2PERDL = PWM_PERDL_INIT;

    PWM4PERDH = PWM_PERDH_INIT;
    PWM4PERDL = PWM_PERDL_INIT;

    SET_PWM_DUTY(LED_PWM_C0, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C1, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C2, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C3, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C4, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C5, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C6, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C7, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C8, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C9, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C10, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C11, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C12, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C13, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C14, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C15, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C16, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C17, PWM_DUTY1, PWM_DUTY2);
    SET_PWM_DUTY(LED_PWM_C18, PWM_DUTY1, PWM_DUTY2);
}
