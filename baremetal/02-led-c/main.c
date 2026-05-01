#include "main.h"

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
 * C entry point.
 */
int main(void)
{
    clk_enable();
    led_init();

    while (1) {
        led_off();
        delay(500);

        led_on();
        delay(500);
    }

    return 0;
}