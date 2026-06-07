#include "main.h"

#define LED_MODE_FAST_BLINK     1
#define LED_MODE_SLOW_BLINK     2
#define LED_MODE_BREATHING      3

#ifndef LED_MODE
#define LED_MODE                LED_MODE_BREATHING
#endif

#define FAST_BLINK_DELAY_MS     100
#define SLOW_BLINK_DELAY_MS     500
#define BREATH_PWM_PERIOD       50
#define BREATH_PWM_TICK_DELAY   0x3ff
#define BREATH_STEP_HOLD_MS     8

/*
 * Enable all peripheral clock gates.
 */
void clk_enable(void)
{
    CCM_CCGR0 = 0xffffffff;
    CCM_CCGR1 = 0xffffffff;
    CCM_CCGR2 = 0xffffffff;
    CCM_CCGR3 = 0xffffffff;
    CCM_CCGR4 = 0xffffffff;
    CCM_CCGR5 = 0xffffffff;
    CCM_CCGR6 = 0xffffffff;
}

/*
 * Initialize GPIO1_IO03 as LED output.
 */
void led_init(void)
{
    /*
     * Configure GPIO1_IO03 pin mux as GPIO function.
     */
    SW_MUX_GPIO1_IO03 = 0x5;

    /*
     * Configure pad electrical attributes.
     */
    SW_PAD_GPIO1_IO03 = 0x10B0;

    /*
     * Configure GPIO1_IO03 as output.
     * Only bit3 is modified.
     */
    GPIO1_GDIR |= LED_GPIO_BIT;

    /*
     * Turn on LED.
     * The LED is active-low on this board.
     */
    GPIO1_DR &= ~LED_GPIO_BIT;
}

/*
 * Turn on LED.
 */
void led_on(void)
{
    GPIO1_DR &= ~LED_GPIO_BIT;
}

/*
 * Turn off LED.
 */
void led_off(void)
{
    GPIO1_DR |= LED_GPIO_BIT;
}

/*
 * Short software delay.
 */
void delay_short(volatile unsigned int n)
{
    while (n--) {
    }
}

/*
 * Millisecond-level software delay.
 * The delay value depends on CPU frequency and compiler optimization.
 */
void delay(volatile unsigned int n)
{
    while (n--) {
        delay_short(0x7ff);
    }
}

/*
 * Blink LED with configurable on/off delays.
 */
void led_blink(unsigned int on_delay_ms, unsigned int off_delay_ms)
{
    while (1) {
        led_on();
        delay(on_delay_ms);

        led_off();
        delay(off_delay_ms);
    }
}

/*
 * Fast blink mode.
 */
void led_fast_blink_mode(void)
{
    led_blink(FAST_BLINK_DELAY_MS, FAST_BLINK_DELAY_MS);
}

/*
 * Slow blink mode.
 */
void led_slow_blink_mode(void)
{
    led_blink(SLOW_BLINK_DELAY_MS, SLOW_BLINK_DELAY_MS);
}

/*
 * One software PWM cycle for simulated LED brightness.
 */
void led_soft_pwm_cycle(unsigned int on_ticks, unsigned int period_ticks)
{
    unsigned int i;

    for (i = 0; i < period_ticks; i++) {
        if (i < on_ticks) {
            led_on();
        } else {
            led_off();
        }

        delay_short(BREATH_PWM_TICK_DELAY);
    }
}

/*
 * Breathing LED mode simulated by software PWM.
 */
void led_breathing_mode(void)
{
    unsigned int duty;
    unsigned int hold;

    while (1) {
        for (duty = 0; duty <= BREATH_PWM_PERIOD; duty++) {
            for (hold = 0; hold < BREATH_STEP_HOLD_MS; hold++) {
                led_soft_pwm_cycle(duty, BREATH_PWM_PERIOD);
            }
        }

        for (duty = BREATH_PWM_PERIOD; duty > 0; duty--) {
            for (hold = 0; hold < BREATH_STEP_HOLD_MS; hold++) {
                led_soft_pwm_cycle(duty, BREATH_PWM_PERIOD);
            }
        }
    }
}

/*
 * C entry point.
 */
int main(void)
{
    clk_enable();
    led_init();

#if LED_MODE == LED_MODE_FAST_BLINK
    led_fast_blink_mode();
#elif LED_MODE == LED_MODE_SLOW_BLINK
    led_slow_blink_mode();
#elif LED_MODE == LED_MODE_BREATHING
    led_breathing_mode();
#else
    while (1) {
        led_off();
    }
#endif

    return 0;
}
