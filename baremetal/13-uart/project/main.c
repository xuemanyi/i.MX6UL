#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_int.h"
#include "bsp_uart.h"

/*
 * Bare-metal UART echo demo.
 */
int main(void)
{
	unsigned char ch;
	unsigned char state = OFF;

	int_init();
	imx6u_clkinit();
	delay_init();
	clk_enable();

	led_init();
	beep_init();
	uart_init();

	while (1) {
		puts("Please input one character:");
		ch = getc();

		putc(ch);
		puts("\r\n");

		puts("You input:");
		putc(ch);
		puts("\r\n\r\n");

		state = !state;
		led_switch(LED0, state);
	}

	return 0;
}