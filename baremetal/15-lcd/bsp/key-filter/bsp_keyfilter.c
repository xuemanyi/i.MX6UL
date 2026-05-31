#include "bsp_key.h"
#include "bsp_gpio.h"
#include "bsp_int.h"
#include "bsp_beep.h"
#include "bsp_keyfilter.h"

#define KEY_FILTER_TIMER_COUNT	(66000000U / 100U)

/*
 * Initialize GPIO1_IO18 as an interrupt key with timer-based debounce.
 */
void filterkey_init(void)
{
	gpio_pin_config_t key_config;

	IOMUXC_SetPinMux(IOMUXC_UART1_CTS_B_GPIO1_IO18, 0);
	IOMUXC_SetPinConfig(IOMUXC_UART1_CTS_B_GPIO1_IO18, 0xf080);

	key_config.direction = kGPIO_DigitalInput;
	key_config.interruptMode = kGPIO_IntFallingEdge;
	key_config.outputLogic = 1;

	gpio_init(GPIO1, 18, &key_config);

	GIC_EnableIRQ(GPIO1_Combined_16_31_IRQn);

	system_register_irqhandler(GPIO1_Combined_16_31_IRQn,
				   gpio1_16_31_irqhandler,
				   NULL);

	gpio_enableint(GPIO1, 18);

	filtertimer_init(KEY_FILTER_TIMER_COUNT);
}

/*
 * Initialize EPIT1 as the debounce timer.
 *
 * The timer is configured but kept disabled. It is started from the GPIO
 * interrupt handler and stopped again after the debounce timeout expires.
 */
void filtertimer_init(unsigned int value)
{
	EPIT1->CR = 0;

	/*
	 * Configure EPIT1.
	 *
	 * Clock source  : Peripheral clock, 66 MHz
	 * Prescaler     : divide by 1
	 * Reload mode   : reload from LR
	 * Interrupt     : output compare interrupt enabled
	 * Initial value : load from LR when enabled
	 */
	EPIT1->CR = (1U << 24) |
		    (1U << 3) |
		    (1U << 2) |
		    (1U << 1);

	EPIT1->LR = value;
	EPIT1->CMPR = 0;

	GIC_EnableIRQ(EPIT1_IRQn);

	system_register_irqhandler(EPIT1_IRQn,
				   filtertimer_irqhandler,
				   NULL);
}

/*
 * Stop the debounce timer.
 */
void filtertimer_stop(void)
{
	EPIT1->CR &= ~(1U << 0);
}

/*
 * Restart the debounce timer with a new reload value.
 */
void filtertimer_restart(unsigned int value)
{
	EPIT1->CR &= ~(1U << 0);
	EPIT1->LR = value;
	EPIT1->SR |= 1U << 0;
	EPIT1->CR |= 1U << 0;
}

/*
 * Debounce timer interrupt handler.
 *
 * When the debounce timeout expires, sample the key level again. If the key
 * is still active, treat it as a valid key press.
 */
void filtertimer_irqhandler(unsigned int giccIar, void *userParam)
{
	static unsigned char state = OFF;

	(void)giccIar;
	(void)userParam;

	if (EPIT1->SR & (1U << 0)) {
		filtertimer_stop();

		if (gpio_pinread(GPIO1, 18) == 0) {
			state = !state;
			beep_switch(state);
		}
	}

	EPIT1->SR |= 1U << 0;
}

/*
 * GPIO1 combined interrupt handler.
 *
 * Start the debounce timer after a falling edge is detected.
 */
void gpio1_16_31_irqhandler(unsigned int giccIar, void *userParam)
{
	(void)giccIar;
	(void)userParam;

	filtertimer_restart(KEY_FILTER_TIMER_COUNT);

	gpio_clearintflags(GPIO1, 18);
}