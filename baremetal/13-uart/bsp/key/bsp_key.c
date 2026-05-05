#include "bsp_key.h"
#include "bsp_gpio.h"
#include "bsp_delay.h"

#define KEY0_GPIO       GPIO1
#define KEY0_PIN        18U

/*
 * Initialize key GPIO.
 *
 * KEY0 is connected to GPIO1_IO18 and is active low.
 */
void key_init(void)
{
	gpio_pin_config_t key_config;

	IOMUXC_SetPinMux(IOMUXC_UART1_CTS_B_GPIO1_IO18, 0);

	/*
	 * Configure UART1_CTS_B pad control.
	 *
	 * bit 16    : HYS disabled
	 * bit 15:14 : 22K pull-up
	 * bit 13    : pull function
	 * bit 12    : pull/keeper enabled
	 * bit 11    : open-drain disabled
	 * bit 7:6   : medium speed, 100 MHz
	 * bit 5:3   : output disabled
	 * bit 0     : slow slew rate
	 */
	IOMUXC_SetPinConfig(IOMUXC_UART1_CTS_B_GPIO1_IO18, 0xf080);

	key_config.direction = kGPIO_DigitalInput;
	key_config.outputLogic = 0;

	gpio_init(KEY0_GPIO, KEY0_PIN, &key_config);
}

/*
 * Get key value with software debounce.
 *
 * Return: KEY_NONE if no key is pressed, otherwise key value.
 */
int key_getvalue(void)
{
	static uint8_t released = 1;
	int ret = KEY_NONE;

	if (released && gpio_pinread(KEY0_GPIO, KEY0_PIN) == 0) {
		delay(10);

		if (gpio_pinread(KEY0_GPIO, KEY0_PIN) == 0) {
			released = 0;
			ret = KEY0_VALUE;
		}
	} else if (gpio_pinread(KEY0_GPIO, KEY0_PIN) == 1) {
		released = 1;
	}

	return ret;
}