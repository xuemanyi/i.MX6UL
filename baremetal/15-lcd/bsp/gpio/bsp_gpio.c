#include "bsp_gpio.h"

/*
 * Initialize a GPIO pin.
 */
void gpio_init(GPIO_Type *base, int pin, gpio_pin_config_t *config)
{
	base->IMR &= ~(1U << pin);

	if (config->direction == kGPIO_DigitalInput) {
		base->GDIR &= ~(1U << pin);
	} else {
		base->GDIR |= (1U << pin);
		gpio_pinwrite(base, pin, config->outputLogic);
	}

	gpio_intconfig(base, pin, config->interruptMode);
}

/*
 * Read GPIO input level.
 */
int gpio_pinread(GPIO_Type *base, int pin)
{
	return (base->DR >> pin) & 0x1;
}

/*
 * Write GPIO output level.
 */
void gpio_pinwrite(GPIO_Type *base, int pin, int value)
{
	if (value == 0U) {
		base->DR &= ~(1U << pin);
	} else {
		base->DR |= 1U << pin;
	}
}

/*
 * Configure GPIO interrupt trigger mode.
 */
void gpio_intconfig(GPIO_Type *base,
		    unsigned int pin,
		    gpio_interrupt_mode_t pin_int_mode)
{
	volatile uint32_t *icr;
	uint32_t icrShift;

	icrShift = pin;

	base->EDGE_SEL &= ~(1U << pin);

	if (pin < 16) {
		icr = &base->ICR1;
	} else {
		icr = &base->ICR2;
		icrShift -= 16;
	}

	switch (pin_int_mode) {
	case kGPIO_IntLowLevel:
		*icr &= ~(3U << (2 * icrShift));
		break;

	case kGPIO_IntHighLevel:
		*icr &= ~(3U << (2 * icrShift));
		*icr |= 1U << (2 * icrShift);
		break;

	case kGPIO_IntRisingEdge:
		*icr &= ~(3U << (2 * icrShift));
		*icr |= 2U << (2 * icrShift);
		break;

	case kGPIO_IntFallingEdge:
		*icr |= 3U << (2 * icrShift);
		break;

	case kGPIO_IntRisingOrFallingEdge:
		base->EDGE_SEL |= 1U << pin;
		break;

	default:
		break;
	}
}

/*
 * Enable GPIO interrupt.
 */
void gpio_enableint(GPIO_Type *base, unsigned int pin)
{
	base->IMR |= 1U << pin;
}

/*
 * Disable GPIO interrupt.
 */
void gpio_disableint(GPIO_Type *base, unsigned int pin)
{
	base->IMR &= ~(1U << pin);
}

/*
 * Clear GPIO interrupt status.
 *
 * GPIO ISR bits are write-one-to-clear.
 */
void gpio_clearintflags(GPIO_Type *base, unsigned int pin)
{
	base->ISR |= 1U << pin;
}