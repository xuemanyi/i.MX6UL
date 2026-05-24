#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_int.h"
#include "bsp_uart.h"
#include "stdio.h"

/*
 * Bare-metal UART printf/scanf demo.
 */
int main(void)
{
	unsigned char state = OFF;
	int a;
	int b;

	int_init();
	imx6u_clkinit();
	delay_init();
	clk_enable();

	led_init();
	beep_init();
	uart_init();

	while (1) {
		printf("Input two integers separated by space:");
		scanf("%d %d", &a, &b);

		printf("\r\n%d + %d = %d\r\n\r\n", a, b, a + b);

		state = !state;
		led_switch(LED0, state);
	}

	return 0;
}