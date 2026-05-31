#include "bsp_delay.h"

/*
 * Initialize GPT1 as a free-running 1 MHz timer.
 *
 * GPT1 input clock:
 *   ipg_clk = 66 MHz
 *
 * Prescaler:
 *   PR = 65
 *
 * GPT1 counter clock:
 *   66 MHz / (65 + 1) = 1 MHz
 *
 * Therefore, one GPT1 count equals 1 us.
 */
void delay_init(void)
{
	GPT1->CR = 0;

	GPT1->CR = 1U << 15;
	while ((GPT1->CR >> 15) & 0x1) {
	}

	/*
	 * Configure GPT1.
	 *
	 * Clock source : ipg_clk
	 * Mode         : free-running mode
	 * Output       : disabled
	 */
	GPT1->CR = 1U << 6;

	GPT1->PR = 65;
	GPT1->OCR[0] = 0xffffffff;

	GPT1->CR |= 1U << 0;
}

/*
 * Delay in microseconds.
 *
 * GPT1 runs at 1 MHz, so each counter tick is 1 us.
 */
void delayus(unsigned int usdelay)
{
	unsigned long oldcnt;
	unsigned long newcnt;
	unsigned long elapsed = 0;

	oldcnt = GPT1->CNT;

	while (1) {
		newcnt = GPT1->CNT;

		if (newcnt != oldcnt) {
			if (newcnt > oldcnt) {
				elapsed += newcnt - oldcnt;
			} else {
				elapsed += 0xffffffff - oldcnt + newcnt;
			}

			oldcnt = newcnt;

			if (elapsed >= usdelay) {
				break;
			}
		}
	}
}

/*
 * Delay in milliseconds.
 */
void delayms(unsigned int msdelay)
{
	unsigned int i;

	for (i = 0; i < msdelay; i++) {
		delayus(1000);
	}
}

/*
 * Short software delay loop.
 *
 * This function is kept for simple early-stage delay needs.
 */
void delay_short(volatile unsigned int n)
{
	while (n--) {
	}
}

/*
 * Legacy coarse delay.
 *
 * Prefer delayus() or delayms() for precise delays.
 */
void delay(volatile unsigned int n)
{
	while (n--) {
		delay_short(0x7ff);
	}
}