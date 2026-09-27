#include "bsp_gpio.h"

/*
 * 初始化一个 GPIO 引脚。
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
 * 读取 GPIO 输入电平。
 */
int gpio_pinread(GPIO_Type *base, int pin)
{
	return (base->DR >> pin) & 0x1;
}

/*
 * 写入 GPIO 输出电平。
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
 * 配置 GPIO 中断触发方式。
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
 * 使能 GPIO 中断。
 */
void gpio_enableint(GPIO_Type *base, unsigned int pin)
{
	base->IMR |= 1U << pin;
}

/*
 * 禁止 GPIO 中断。
 */
void gpio_disableint(GPIO_Type *base, unsigned int pin)
{
	base->IMR &= ~(1U << pin);
}

/*
 * 清除 GPIO 中断状态。
 *
 * GPIO ISR 位采用写 1 清除语义。
 */
void gpio_clearintflags(GPIO_Type *base, unsigned int pin)
{
	base->ISR |= 1U << pin;
}