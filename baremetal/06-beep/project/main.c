/*
 * i.MX6UL BSP Beep example.
 */

#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"

/*
 * Main entry.
 */
int main(void)
{
	clk_enable();
	led_init();

	while (1) {
		led_switch(LED0, ON);
		beep_switch(ON);
		delay(500);

		led_switch(LED0, OFF);
		beep_switch(OFF);
		delay(500);
	}

	return 0;
}