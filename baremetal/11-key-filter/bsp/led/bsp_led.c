#include "bsp_led.h"

#define LED0_GPIO       GPIO1
#define LED0_PIN        3U
#define LED0_PIN_MASK   (1U << LED0_PIN)

/*
 * Initialize LED GPIO.
 *
 * LED0 is connected to GPIO1_IO03 and is active low.
 */
void led_init(void)
{
	/*
	 * Configure GPIO1_IO03 pin mux as GPIO.
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
	LED0_GPIO->GDIR |= LED0_PIN_MASK;

	/*
	 * Turn LED0 on by default.
	 */
	LED0_GPIO->DR &= ~LED0_PIN_MASK;
}

/*
 * Switch LED state.
 *
 * @led: LED index
 * @status: ON to enable LED, OFF to disable LED
 */
void led_switch(int led, int status)
{
	if (led != LED0)
		return;

	if (status == ON)
		LED0_GPIO->DR &= ~LED0_PIN_MASK;
	else
		LED0_GPIO->DR |= LED0_PIN_MASK;
}