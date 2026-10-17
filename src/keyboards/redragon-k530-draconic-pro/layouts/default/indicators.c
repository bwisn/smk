#include "indicators.h"
#include "kbdef.h"
#include "pwm.h"
#include <stdlib.h>
#include <string.h>

#define PWM_CLK_DIV         0b010 // PWM_CLK = SYS_CLK / 4
#define PWM_SS_BIT          (1 << 3)
#define PWM_MOD_BIT         (1 << 4)
#define PWM_INT_ENABLE_BIT  (1 << 6)
#define PWM_MODE_ENABLE_BIT (1 << 7)

#include "settings.h"

#ifdef RF_ENABLED
#    include "rf_controller.h"
#endif

#define TOTAL_MODES 18

/* Slave channel config: ENABLE + SS + CLK_DIV = 0x8A. */
#define PWM_SLAVE_CON (uint8_t)(PWM_MODE_ENABLE_BIT | PWM_SS_BIT | PWM_CLK_DIV)

/* Brightness uses a plain multiplier: duty = colour * level (0..5). Level 5
 * gives the full 1280-cycle PWM duty. */

/* Row and colour for each phase (1..18): underline first, then the five key
 * rows. */
static __code const uint8_t phase_row[19]   = {0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5};
static __code const uint8_t phase_color[19] = {0, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};

/* Effect steps once every (limit + 1) LED frames. Speed 0 is fastest. */
#define LED_SPEED_LEVELS 5
static __code const uint8_t led_speed_limits[LED_SPEED_LEVELS] = {0, 5, 10, 15, 20};

void indicators_apply_defaults(void)
{
    user_settings.led_effect     = 0; // rainbow wave L->R
    user_settings.led_brightness = 5; // 100%
    user_settings.led_color      = 1; // enabled
    user_settings.led_speed      = 0; // fastest
#ifdef RF_ENABLED
    user_settings.rf_link = RF_MODE_2_4G;
#endif
}

void indicators_validate_settings(void)
{
    if (user_settings.led_effect >= TOTAL_MODES) {
        user_settings.led_effect = 0;
    }
    if (user_settings.led_brightness > 5) {
        user_settings.led_brightness = 5;
    }
    if (user_settings.led_color > 1) {
        user_settings.led_color = 1;
    }
    if (user_settings.led_speed >= LED_SPEED_LEVELS) {
        user_settings.led_speed = 0;
    }
}

// Published duties, already colour * brightness; the ISR only copies them.
static __xdata uint16_t led_col_duty[2][6][3][19];
static uint8_t anim_phase = 0;
static uint8_t anim_frames = 0;
// Double buffer between the main-loop renderer and the tick ISR. Byte reads
// and writes are atomic on the 8051.
static volatile uint8_t led_frame_active;
static volatile uint8_t led_reader_frame;  /* written by ISR, read by renderer */

// LED frame counter, used as the RF beacon time base.
static volatile uint16_t s_rf_gate_lap;

void indicators_brightness_up(void)
{
    if (user_settings.led_brightness < 5) {
        user_settings.led_brightness++;
        settings_mark_dirty();
    }
}

void indicators_brightness_down(void)
{
    if (user_settings.led_brightness > 0) {
        user_settings.led_brightness--;
        settings_mark_dirty();
    }
}

void indicators_toggle(void)
{
    user_settings.led_color = user_settings.led_color ? 0 : 1;
    settings_mark_dirty();
}

void indicators_next_mode(void)
{
    user_settings.led_effect = (uint8_t)((user_settings.led_effect + 1) % TOTAL_MODES);
    settings_mark_dirty();
}

void indicators_speed_cycle(void)
{
    user_settings.led_speed = (uint8_t)((user_settings.led_speed + 1) % LED_SPEED_LEVELS);
    settings_mark_dirty();
}

void indicators_start(void)
{
    indicators_pwm_enable();
}

void indicators_pre_update(void)
{
    // The RGB sinks are active low: clearing the port turns them off.
    P0 &= ~(RGB_R2R_P0_2 | RGB_R0B_P0_3 | RGB_R0R_P0_4);
    P4 &= ~(RGB_ULB_P4_0 | RGB_ULR_P4_1);
    P6 &= ~(RGB_ULG_P6_0);
    P4 &= ~(RGB_R4B_P4_4 | RGB_R4R_P4_3 | RGB_R3R_P4_5 | RGB_R3B_P4_6);
    P5 &= ~(RGB_R2B_P5_7);
    P6 &= ~(RGB_R0G_P6_1 | RGB_R1G_P6_2 | RGB_R2G_P6_3 | RGB_R3G_P6_4 | RGB_R4G_P6_5 | RGB_R1B_P6_6 | RGB_R1R_P6_7);

    indicators_pwm_disable();
}

void indicators_post_update(void)
{
    PWM00CON &= ~(1 << 5);
    indicators_pwm_enable();
}

// Stop the sinks and PWM during the flash write so no row stays lit.
void settings_save_pre(void)
{
    indicators_pre_update();
}

void settings_save_post(void)
{
    indicators_post_update();
}

void indicators_pwm_set_all_columns(uint16_t intensity)
{
    SET_PWM_DUTY_2(LED_PWM_C0, intensity);
    SET_PWM_DUTY_2(LED_PWM_C1, intensity);
    SET_PWM_DUTY_2(LED_PWM_C2, intensity);
    SET_PWM_DUTY_2(LED_PWM_C3, intensity);
    SET_PWM_DUTY_2(LED_PWM_C4, intensity);
    SET_PWM_DUTY_2(LED_PWM_C5, intensity);
    SET_PWM_DUTY_2(LED_PWM_C6, intensity);
    SET_PWM_DUTY_2(LED_PWM_C7, intensity);
    SET_PWM_DUTY_2(LED_PWM_C8, intensity);
    SET_PWM_DUTY_2(LED_PWM_C9, intensity);
    SET_PWM_DUTY_2(LED_PWM_C10, intensity);
    SET_PWM_DUTY_2(LED_PWM_C11, intensity);
    SET_PWM_DUTY_2(LED_PWM_C12, intensity);
    SET_PWM_DUTY_2(LED_PWM_C13, intensity);
    SET_PWM_DUTY_2(LED_PWM_C14, intensity);
    SET_PWM_DUTY_2(LED_PWM_C15, intensity);
    SET_PWM_DUTY_2(LED_PWM_C16, intensity);
    SET_PWM_DUTY_2(LED_PWM_C17, intensity);
    SET_PWM_DUTY_2(LED_PWM_C18, intensity);
}

uint16_t indicators_rf_gate_laps(void)
{
    return s_rf_gate_lap;
}

void indicators_pwm_enable(void)
{
    /* Master channel: ENABLE + INT + SS + CLK_DIV = 0xCA; slaves 0x8A. */
    PWM00CON = (uint8_t)(PWM_MODE_ENABLE_BIT | PWM_INT_ENABLE_BIT | PWM_SS_BIT | PWM_CLK_DIV);
    PWM01CON = PWM_SLAVE_CON;
    PWM02CON = PWM_SLAVE_CON;
    PWM03CON = PWM_SLAVE_CON;
    PWM04CON = PWM_SLAVE_CON;
    PWM05CON = PWM_SLAVE_CON;

    PWM10CON = (uint8_t)(PWM_MODE_ENABLE_BIT | PWM_SS_BIT | PWM_CLK_DIV);
    PWM11CON = PWM_SLAVE_CON;
    PWM12CON = PWM_SLAVE_CON;
    PWM13CON = PWM_SLAVE_CON;
    PWM14CON = PWM_SLAVE_CON;
    PWM15CON = PWM_SLAVE_CON;

    PWM20CON = (uint8_t)(PWM_MODE_ENABLE_BIT | PWM_CLK_DIV); // 0x82: group 2 master has no SS bit
    PWM22CON = PWM_SLAVE_CON;
    PWM23CON = PWM_SLAVE_CON;
    PWM24CON = PWM_SLAVE_CON;
    PWM25CON = PWM_SLAVE_CON;

    PWM40CON = (uint8_t)(PWM_MODE_ENABLE_BIT | PWM_SS_BIT | PWM_CLK_DIV);
    PWM41CON = PWM_SLAVE_CON;
    PWM42CON = PWM_SLAVE_CON;
}

void indicators_pwm_disable(void)
{
    /* Set all channels to CLK_DIV only (0x02). */
    PWM00CON = (uint8_t)(PWM_CLK_DIV);
    PWM01CON = (uint8_t)(PWM_CLK_DIV);
    PWM02CON = (uint8_t)(PWM_CLK_DIV);
    PWM03CON = (uint8_t)(PWM_CLK_DIV);
    PWM04CON = (uint8_t)(PWM_CLK_DIV);
    PWM05CON = (uint8_t)(PWM_CLK_DIV);

    PWM10CON = (uint8_t)(PWM_CLK_DIV);
    PWM11CON = (uint8_t)(PWM_CLK_DIV);
    PWM12CON = (uint8_t)(PWM_CLK_DIV);
    PWM13CON = (uint8_t)(PWM_CLK_DIV);
    PWM14CON = (uint8_t)(PWM_CLK_DIV);
    PWM15CON = (uint8_t)(PWM_CLK_DIV);

    PWM20CON = (uint8_t)(PWM_CLK_DIV);
    PWM22CON = (uint8_t)(PWM_CLK_DIV);
    PWM23CON = (uint8_t)(PWM_CLK_DIV);
    PWM24CON = (uint8_t)(PWM_CLK_DIV);
    PWM25CON = (uint8_t)(PWM_CLK_DIV);

    PWM40CON = (uint8_t)(PWM_CLK_DIV);
    PWM41CON = (uint8_t)(PWM_CLK_DIV);
    PWM42CON = (uint8_t)(PWM_CLK_DIV);
}

static void indicators_select_sink(uint8_t phase)
{
    switch (phase) {
        case 1:  RGB_ULR = 1; break;
        case 2:  RGB_ULG = 1; break;
        case 3:  RGB_ULB = 1; break;
        case 4:  RGB_R0R = 1; break;
        case 5:  RGB_R0G = 1; break;
        case 6:  RGB_R0B = 1; break;
        case 7:  RGB_R1R = 1; break;
        case 8:  RGB_R1G = 1; break;
        case 9:  RGB_R1B = 1; break;
        case 10: RGB_R2R = 1; break;
        case 11: RGB_R2G = 1; break;
        case 12: RGB_R2B = 1; break;
        case 13: RGB_R3R = 1; break;
        case 14: RGB_R3G = 1; break;
        case 15: RGB_R3B = 1; break;
        case 16: RGB_R4R = 1; break;
        case 17: RGB_R4G = 1; break;
        case 18: RGB_R4B = 1; break;
        default: break;
    }
}

static void get_rainbow_rgb8(uint8_t h, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (h < 85) {
        *r = (uint8_t)(255 - h * 3);
        *g = (uint8_t)(h * 3);
        *b = 0;
    } else if (h < 170) {
        h = (uint8_t)(h - 85);
        *r = 0;
        *g = (uint8_t)(255 - h * 3);
        *b = (uint8_t)(h * 3);
    } else {
        h = (uint8_t)(h - 170);
        *r = (uint8_t)(h * 3);
        *g = 0;
        *b = (uint8_t)(255 - h * 3);
    }
}

/* duty = colour * brightness. Use shifts and adds instead of a 16-bit
 * multiply, which is slow in the main-loop renderer. */
static uint16_t scale_bri(uint8_t v, uint8_t level)
{
    switch (level) {
        case 0:  return 0;
        case 1:  return v;
        case 2:  return (uint16_t)((uint16_t)v << 1);
        case 3:  return (uint16_t)(((uint16_t)v << 1) + v);
        case 4:  return (uint16_t)((uint16_t)v << 2);
        default: return (uint16_t)(((uint16_t)v << 2) + v);
    }
}

void indicators_render(void)
{
    if (!user_settings.led_color || user_settings.led_brightness == 0) {
        memset(led_col_duty, 0, sizeof(led_col_duty));
        return;
    }

    /* Publish sync: write only the buffer that the ISR is not reading. */
    const uint8_t target = (uint8_t)(led_frame_active ^ 1);
    if (led_frame_active != led_reader_frame) {
        return;
    }

    const uint8_t bri = user_settings.led_brightness;

    for (uint8_t r = 0; r < 6; r++) {
        for (uint8_t c = 0; c < 19; c++) {
            uint8_t red = 0, green = 0, blue = 0;

            switch (user_settings.led_effect) {
                case 0: { // Rainbow wave Left -> Right
                    uint8_t h = (uint8_t)(anim_phase + (c * 14));
                    get_rainbow_rgb8(h, &red, &green, &blue);
                    break;
                }
                case 1: { // Rainbow wave Right -> Left
                    uint8_t h = (uint8_t)(anim_phase - (c * 14));
                    get_rainbow_rgb8(h, &red, &green, &blue);
                    break;
                }
                case 2: { // Rainbow wave Top -> Bottom
                    uint8_t h = (uint8_t)(anim_phase + (r * 42));
                    get_rainbow_rgb8(h, &red, &green, &blue);
                    break;
                }
                case 3: { // Rainbow wave Diagonal (TL -> BR)
                    uint8_t h = (uint8_t)(anim_phase + (c * 10) + (r * 24));
                    get_rainbow_rgb8(h, &red, &green, &blue);
                    break;
                }
                case 4: { // Rainbow wave Diagonal (TR -> BL)
                    uint8_t h = (uint8_t)(anim_phase - (c * 10) + (r * 24));
                    get_rainbow_rgb8(h, &red, &green, &blue);
                    break;
                }
                case 5: { // Spectrum Cycle (all keys cycle together)
                    get_rainbow_rgb8(anim_phase, &red, &green, &blue);
                    break;
                }
                case 6: { // Rainbow Breathing
                    uint8_t h = (uint8_t)(anim_phase & 0xE0);
                    get_rainbow_rgb8(h, &red, &green, &blue);
                    uint8_t bp = (uint8_t)(anim_phase & 0x7F);
                    uint8_t breath = (bp < 64) ? (uint8_t)(bp << 2) : (uint8_t)((128 - bp) << 2);
                    red   = (uint8_t)(((uint16_t)red * breath) >> 8);
                    green = (uint8_t)(((uint16_t)green * breath) >> 8);
                    blue  = (uint8_t)(((uint16_t)blue * breath) >> 8);
                    break;
                }
                case 7: { // Aurora / Neon Stream
                    uint8_t wave = (uint8_t)((c * 12) + ((r & 1) ? 32 : 0));
                    get_rainbow_rgb8((uint8_t)(anim_phase + wave), &red, &green, &blue);
                    break;
                }
                case 8: { // Raindrops / Twinkling Stars
                    uint8_t seed = (uint8_t)((c * 17) ^ (r * 31) ^ (anim_phase >> 3));
                    uint8_t dp = (uint8_t)((anim_phase + seed * 7) & 0x3F);
                    if (dp < 32) {
                        get_rainbow_rgb8((uint8_t)(seed * 23), &red, &green, &blue);
                        uint8_t bri = (dp < 16) ? (uint8_t)(dp << 4) : (uint8_t)((32 - dp) << 4);
                        red   = (uint8_t)(((uint16_t)red * bri) >> 8);
                        green = (uint8_t)(((uint16_t)green * bri) >> 8);
                        blue  = (uint8_t)(((uint16_t)blue * bri) >> 8);
                    }
                    break;
                }
                case 9: { // Breathing Cyan
                    uint8_t bp = (uint8_t)(anim_phase & 0x7F);
                    uint8_t breath = (bp < 64) ? (uint8_t)(bp << 2) : (uint8_t)((128 - bp) << 2);
                    red = 0;
                    green = breath;
                    blue = breath;
                    break;
                }
                case 10: { // Breathing Red
                    uint8_t bp = (uint8_t)(anim_phase & 0x7F);
                    uint8_t breath = (bp < 64) ? (uint8_t)(bp << 2) : (uint8_t)((128 - bp) << 2);
                    red = breath;
                    green = 0;
                    blue = 0;
                    break;
                }
                case 11: // Static Red
                    red = 255; green = 0; blue = 0;
                    break;
                case 12: // Static Green
                    red = 0; green = 255; blue = 0;
                    break;
                case 13: // Static Blue
                    red = 0; green = 0; blue = 255;
                    break;
                case 14: // Static Yellow
                    red = 255; green = 255; blue = 0;
                    break;
                case 15: // Static Cyan
                    red = 0; green = 255; blue = 255;
                    break;
                case 16: // Static Purple
                    red = 255; green = 0; blue = 255;
                    break;
                case 17: // Static White
                    red = 255; green = 255; blue = 255;
                    break;
            }

            led_col_duty[target][r][0][c] = scale_bri(red, bri);
            led_col_duty[target][r][1][c] = scale_bri(green, bri);
            led_col_duty[target][r][2][c] = scale_bri(blue, bri);
        }
    }

    /* Caps Lock: the key shows the host lock state in green. */
    if (keyboard_state.led_state & (1 << 1)) {
        led_col_duty[target][3][0][0] = 0;
        led_col_duty[target][3][1][0] = scale_bri(255, bri);
        led_col_duty[target][3][2][0] = 0;
    }

    led_frame_active = target;

    /* Advance the animation once per published frame. The effect speed
     * follows the LED frame, not the main loop. */
    if (++anim_frames > led_speed_limits[user_settings.led_speed]) {
        anim_frames = 0;
        anim_phase++;
    }
}

bool indicators_update_step(keyboard_state_t *keyboard, uint8_t current_step)
{
    static uint8_t phase = 0;
    (void)keyboard;
    (void)current_step;

    s_rf_gate_lap++;

    const uint8_t sub_phase = phase;
    if (++phase >= 19) {
        phase = 0;
    }

    /* Report the frame boundary even when the backlight is off: the tick runs
     * the matrix scan in this slot. */
    const bool frame_boundary = (sub_phase == 0);

    if (!user_settings.led_color || user_settings.led_brightness == 0) {
        indicators_pwm_set_all_columns(0);
        return frame_boundary;
    }

    if (frame_boundary) {
        /* Use one published buffer for the whole frame. */
        led_reader_frame = led_frame_active;

        indicators_pwm_set_all_columns(0);
        return true;
    }

    indicators_select_sink(sub_phase);

    const uint8_t row   = phase_row[sub_phase];
    const uint8_t color = phase_color[sub_phase];

    __xdata uint16_t *fb = &led_col_duty[led_reader_frame][row][color][0];

    /* fb holds DUTY2 (0..1275). DUTY1=0, so the LED is on while the output is
     * LOW; a higher DUTY2 is brighter. */

    SET_PWM_DUTY_2(LED_PWM_C0,  fb[0]);
    SET_PWM_DUTY_2(LED_PWM_C1,  fb[1]);
    SET_PWM_DUTY_2(LED_PWM_C2,  fb[2]);
    SET_PWM_DUTY_2(LED_PWM_C3,  fb[3]);
    SET_PWM_DUTY_2(LED_PWM_C4,  fb[4]);
    SET_PWM_DUTY_2(LED_PWM_C5,  fb[5]);
    SET_PWM_DUTY_2(LED_PWM_C6,  fb[6]);
    SET_PWM_DUTY_2(LED_PWM_C7,  fb[7]);
    SET_PWM_DUTY_2(LED_PWM_C8,  fb[8]);
    SET_PWM_DUTY_2(LED_PWM_C9,  fb[9]);
    SET_PWM_DUTY_2(LED_PWM_C10, fb[10]);
    SET_PWM_DUTY_2(LED_PWM_C11, fb[11]);
    SET_PWM_DUTY_2(LED_PWM_C12, fb[12]);
    SET_PWM_DUTY_2(LED_PWM_C13, fb[13]);
    SET_PWM_DUTY_2(LED_PWM_C14, fb[14]);
    SET_PWM_DUTY_2(LED_PWM_C15, fb[15]);
    SET_PWM_DUTY_2(LED_PWM_C16, fb[16]);
    SET_PWM_DUTY_2(LED_PWM_C17, fb[17]);
    SET_PWM_DUTY_2(LED_PWM_C18, fb[18]);

    return false;
}
