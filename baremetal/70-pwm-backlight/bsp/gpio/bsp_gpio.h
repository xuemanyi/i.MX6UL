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
 * enum gpio_interrupt_mode - GPIO 中断触发方式
 * @kGPIO_NoIntmode: 不使用中断
 * @kGPIO_IntLowLevel: 低电平触发
 * @kGPIO_IntHighLevel: 高电平触发
 * @kGPIO_IntRisingEdge: 上升沿触发
 * @kGPIO_IntFallingEdge: 下降沿触发
 * @kGPIO_IntRisingOrFallingEdge: 双边沿触发
 */
enum gpio_interrupt_mode {
	kGPIO_NoIntmode = 0U,
	kGPIO_IntLowLevel = 1U,
	kGPIO_IntHighLevel = 2U,
	kGPIO_IntRisingEdge = 3U,
	kGPIO_IntFallingEdge = 4U,
	kGPIO_IntRisingOrFallingEdge = 5U,
};

typedef enum gpio_interrupt_mode gpio_interrupt_mode_t;

/**
 * struct gpio_pin_config - GPIO 引脚配置
 * @direction: 引脚方向
 * @outputLogic: 输出模式下的初始逻辑电平
 * @interruptMode: GPIO 中断触发方式
 */
struct gpio_pin_config {
	gpio_pin_direction_t direction;
	uint8_t outputLogic;
	gpio_interrupt_mode_t interruptMode;
};

typedef struct gpio_pin_config gpio_pin_config_t;

/**
 * gpio_init() - 初始化 GPIO 引脚
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 * @config: 引脚方向、初始电平和中断配置，不允许为 NULL
 */
void gpio_init(GPIO_Type *base, int pin, gpio_pin_config_t *config);

/**
 * gpio_pinread() - 读取 GPIO 引脚电平
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 *
 * Return: 高电平返回 1，低电平返回 0。
 */
int gpio_pinread(GPIO_Type *base, int pin);

/**
 * gpio_pinwrite() - 写入 GPIO 引脚电平
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 * @value: 0 表示低电平，非 0 表示高电平
 */
void gpio_pinwrite(GPIO_Type *base, int pin, int value);

/**
 * gpio_intconfig() - 配置 GPIO 中断触发方式
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 * @pinInterruptMode: 中断触发方式
 */
void gpio_intconfig(GPIO_Type *base,
		    unsigned int pin,
		    gpio_interrupt_mode_t pinInterruptMode);
/**
 * gpio_enableint() - 使能 GPIO 引脚中断
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 */
void gpio_enableint(GPIO_Type *base, unsigned int pin);

/**
 * gpio_disableint() - 禁止 GPIO 引脚中断
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 */
void gpio_disableint(GPIO_Type *base, unsigned int pin);

/**
 * gpio_clearintflags() - 清除 GPIO 引脚中断状态
 * @base: GPIO 控制器基地址
 * @pin: 控制器内的引脚编号
 */
void gpio_clearintflags(GPIO_Type *base, unsigned int pin);

#endif /* BSP_GPIO_H */
