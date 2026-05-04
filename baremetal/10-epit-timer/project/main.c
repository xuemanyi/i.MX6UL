#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_int.h"
#include "bsp_epittimer.h"

/*
 * Bare-metal EPIT timer demo.
 */
int main(void)
{
	int_init();
	imx6u_clkinit();
	clk_enable();

	led_init();
	beep_init();
	key_init();

	/*
	 * Initialize EPIT1 with IPG clock.
	 *
	 * IPG clock      : 66 MHz
	 * Prescaler      : 1
	 * Reload value   : 66000000 / 2
	 * Timer period   : 500 ms
	 */
	epit1_init(0, 66000000 / 2);

	while (1) {
		delay(500);
	}

	return 0;
}