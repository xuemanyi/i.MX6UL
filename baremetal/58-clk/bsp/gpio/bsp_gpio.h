#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "imx6ul.h"

/**
 * enum gpio_pin_direction - GPIO 引脚方向
 * @kGPIO_DigitalInput: 数字输入
 * @kGPIO_DigitalOutput: 数字输出
 */
enum gpio_pin_direction {
	kGPIO_DigitalInput = 0U,
	kGPIO_DigitalOutput = 1U,
};

typedef enum gpio_pin_direction gpio_pin_direction_t;

/**
 * struct gpio_pin_config - GPIO 引脚配置
 * @direction: 引脚方向
 * @outputLogic: 输出模式下的初始逻辑电平
 */
struct gpio_pin_config {
	gpio_pin_direction_t direction;
	uint8_t outputLogic;
};

typedef struct gpio_pin_config gpio_pin_config_t;

/**
 * gpio_init() - 初始化 GPIO 引脚
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 * @config: 引脚方向和初始输出配置，不允许为 NULL
 */
void gpio_init(GPIO_Type *base, uint32_t pin, const gpio_pin_config_t *config);

/**
 * gpio_pinread() - 读取 GPIO 引脚电平
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 *
 * Return: 高电平返回 1，低电平返回 0。
 */
int gpio_pinread(GPIO_Type *base, uint32_t pin);

/**
 * gpio_pinwrite() - 写入 GPIO 引脚电平
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 * @value: 0 表示低电平，非 0 表示高电平
 */
void gpio_pinwrite(GPIO_Type *base, uint32_t pin, int value);

#endif /* BSP_GPIO_H */
