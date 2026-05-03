#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

/*
 * GPIO driver interface.
 */

#include "imx6ul.h"

typedef enum {
	kGPIO_DigitalInput = 0U,
	kGPIO_DigitalOutput = 1U,
} gpio_pin_direction_t;

typedef struct {
	gpio_pin_direction_t direction;
	uint8_t outputLogic;
} gpio_pin_config_t;

void gpio_init(GPIO_Type *base, uint32_t pin, const gpio_pin_config_t *config);
int gpio_pinread(GPIO_Type *base, uint32_t pin);
void gpio_pinwrite(GPIO_Type *base, uint32_t pin, int value);

#endif /* __BSP_GPIO_H */