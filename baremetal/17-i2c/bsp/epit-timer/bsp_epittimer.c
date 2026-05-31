#include "bsp_epittimer.h"
#include "bsp_int.h"
#include "bsp_led.h"

/*
 * Initialize EPIT1.
 *
 * EPIT is a 32-bit down counter. EPIT1 uses IPG clock as its clock source.
 *
 * frac:
 *   0    means divide by 1
 *   1    means divide by 2
 *   ...
 *   4095 means divide by 4096
 *
 * value:
 *   Reload value loaded into the counter.
 */
void epit1_init(unsigned int frac, unsigned int value)
{
	if (frac > 0xfff) {
		frac = 0xfff;
	}

	EPIT1->CR = 0;

	/*
	 * Configure EPIT1 control register.
	 *
	 * CLKSRC     : Peripheral clock
	 * PRESCALAR  : frac
	 * RLD        : Reload from LR when counter reaches 0
	 * OCIEN      : Enable output compare interrupt
	 * ENMOD      : Load counter from LR when enabled
	 * EN         : Keep disabled during configuration
	 */
	EPIT1->CR = (1U << 24) |
		    (frac << 4) |
		    (1U << 3) |
		    (1U << 2) |
		    (1U << 1);

	EPIT1->LR = value;
	EPIT1->CMPR = 0;

	GIC_EnableIRQ(EPIT1_IRQn);

	system_register_irqhandler(EPIT1_IRQn,
				   epit1_irqhandler,
				   NULL);

	EPIT1->CR |= 1U << 0;
}

/*
 * EPIT1 interrupt handler.
 *
 * Toggle LED0 when the output compare event occurs.
 */
void epit1_irqhandler(unsigned int giccIar, void *userParam)
{
	static unsigned char state;

	(void)giccIar;
	(void)userParam;

	if (EPIT1->SR & (1U << 0)) {
		state = !state;
		led_switch(LED0, state);
	}

	/*
	 * EPIT status bit is write-one-to-clear.
	 */
	EPIT1->SR |= 1U << 0;
}