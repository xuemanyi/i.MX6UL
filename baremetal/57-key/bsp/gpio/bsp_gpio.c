#include "bsp_gpio.h"

/*
 * 初始化 GPIO 引脚方向和默认输出电平。
 *
 * @base: GPIO controller base address
 * @pin: GPIO pin number inside the controller
 * @config: GPIO configuration
 */
void gpio_init(GPIO_Type *base, uint32_t pin, const gpio_pin_config_t *config)
{
	if (!base || !config)
		return;

	if (config->direction == kGPIO_DigitalInput) {
		base->GDIR &= ~(1U << pin);
	} else {
		base->GDIR |= (1U << pin);
		gpio_pinwrite(base, pin, config->outputLogic);
	}
}

/*
 * 读取 GPIO 引脚电平。
 *
 * @base: GPIO controller base address
 * @pin: GPIO pin number inside the controller
 *
 * Return: 0 for low level, 1 for high level.
 */
int gpio_pinread(GPIO_Type *base, uint32_t pin)
{
	if (!base)
		return 0;

	return (base->DR >> pin) & 0x1U;
}

/*
 * 写入 GPIO 引脚电平。
 *
 * @base: GPIO controller base address
 * @pin: GPIO pin number inside the controller
 * @value: 0 for low level, non-zero for high level
 */
void gpio_pinwrite(GPIO_Type *base, uint32_t pin, int value)
{
	if (!base)
		return;

	if (value == 0)
		base->DR &= ~(1U << pin);
	else
		base->DR |= (1U << pin);
}