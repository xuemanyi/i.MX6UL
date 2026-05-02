/*
 * i.MX6UL bare-metal LED example.
 *
 * This example uses the NXP i.MX6ULL SDK register definition files
 * in a GCC bare-metal environment.
 *
 * The LED is connected to GPIO1_IO03.
 */

#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "MCIMX6Y2.h"

#define LED_GPIO        GPIO1
#define LED_PIN         3U
#define LED_PIN_MASK    (1U << LED_PIN)

/*
 * Enable all peripheral clocks.
 *
 * This is simple for learning purposes. In production code, only the
 * required peripheral clocks should be enabled.
 */
static void clk_enable(void)
{
	CCM->CCGR0 = 0xffffffff;
	CCM->CCGR1 = 0xffffffff;
	CCM->CCGR2 = 0xffffffff;
	CCM->CCGR3 = 0xffffffff;
	CCM->CCGR4 = 0xffffffff;
	CCM->CCGR5 = 0xffffffff;
	CCM->CCGR6 = 0xffffffff;
}

/*
 * Initialize GPIO1_IO03 as LED output.
 *
 * GPIO1_IO03 is configured as GPIO function, then its pad control
 * register is configured, and finally the GPIO direction is set to output.
 */
static void led_init(void)
{
	/*
	 * Configure GPIO1_IO03 pin mux.
	 */
	IOMUXC_SetPinMux(IOMUXC_GPIO1_IO03_GPIO1_IO03, 0);

	/*
	 * Configure GPIO1_IO03 pad control.
	 *
	 * bit 16    : HYS disabled
	 * bit 15:14 : default pull-down
	 * bit 13    : keeper
	 * bit 12    : pull/keeper enabled
	 * bit 11    : open-drain disabled
	 * bit 7:6   : medium speed, 100 MHz
	 * bit 5:3   : R0/6 drive strength
	 * bit 0     : slow slew rate
	 */
	IOMUXC_SetPinConfig(IOMUXC_GPIO1_IO03_GPIO1_IO03, 0x10b0);

	/*
	 * Set GPIO1_IO03 as output.
	 */
	LED_GPIO->GDIR |= LED_PIN_MASK;

	/*
	 * Turn LED on by default.
	 *
	 * The LED is active low.
	 */
	LED_GPIO->DR &= ~LED_PIN_MASK;
}

/*
 * Turn on LED.
 *
 * The LED is active low, so clearing GPIO output turns it on.
 */
static void led_on(void)
{
	LED_GPIO->DR &= ~LED_PIN_MASK;
}

/*
 * Turn off LED.
 *
 * The LED is active low, so setting GPIO output turns it off.
 */
static void led_off(void)
{
	LED_GPIO->DR |= LED_PIN_MASK;
}

/*
 * Busy-wait short delay.
 *
 * This delay is not accurate. It only burns CPU cycles.
 */
static void delay_short(volatile unsigned int loops)
{
	while (loops--)
		;
}

/*
 * Busy-wait millisecond-like delay.
 *
 * The delay value is approximate and depends on CPU frequency,
 * compiler optimization and memory timing.
 */
static void delay(volatile unsigned int ms)
{
	while (ms--)
		delay_short(0x7ff);
}

/*
 * Main entry.
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