#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_int.h"

/*
 * Bare-metal GPT delay demo.
 */
int main(void)
{
	unsigned char state = OFF;

	int_init();
	imx6u_clkinit();
	delay_init();
	clk_enable();

	led_init();
	beep_init();

	while (1) {
		state = !state;
		led_switch(LED0, state);
		delayms(500);
	}

	return 0;
}