/*
 * i.MX6UL BSP LED example.
 */

#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"

/*
 * 裸机 C 入口。
 */
int main(void)
{
	clk_enable();
	led_init();

	while (1) {
		led_switch(LED0, ON);
		delay(500);

		led_switch(LED0, OFF);
		delay(500);
	}

	return 0;
}