/*
 * i.MX6UL main frequency clock configuration example.
 *
 */

#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_key.h"

#define LED_BLINK_TICKS    50U
#define MAIN_LOOP_DELAY_MS 10U

/*
 * Main entry.
 */
int main(void)
{
	unsigned int tick = 0;
	int keyvalue;
	unsigned char led_state = OFF;
	unsigned char beep_state = OFF;

	imx6u_clkinit();
	clk_enable();
	led_init();
	beep_init();
	key_init();

	while (1) {
		keyvalue = key_getvalue();

		if (keyvalue == KEY0_VALUE) {
			beep_state = !beep_state;
			beep_switch(beep_state);
		}

		if (++tick >= LED_BLINK_TICKS) {
			tick = 0;
			led_state = !led_state;
			led_switch(LED0, led_state);
		}

		delay(MAIN_LOOP_DELAY_MS);
	}

	return 0;
}