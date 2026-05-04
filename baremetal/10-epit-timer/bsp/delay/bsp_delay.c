#include "bsp_delay.h"

/*
 * Busy-wait short delay.
 *
 * This function only burns CPU cycles. The real delay time depends on
 * CPU frequency, compiler optimization and memory timing.
 */
void delay_short(volatile unsigned int loops)
{
	while (loops--)
		;
}

/*
 * Busy-wait millisecond-like delay.
 *
 * The delay value is approximate when the CPU runs around 396 MHz.
 */
void delay(volatile unsigned int ms)
{
	while (ms--)
		delay_short(0x7ff);
}