#ifndef _BSP_GPIO_H
#define _BSP_GPIO_H

#include "imx6ul.h"

enum gpio_pin_direction {
	kGPIO_DigitalInput = 0U,
	kGPIO_DigitalOutput = 1U,
};

typedef enum gpio_pin_direction gpio_pin_direction_t;

enum gpio_interrupt_mode {
	kGPIO_NoIntmode = 0U,
	kGPIO_IntLowLevel = 1U,
	kGPIO_IntHighLevel = 2U,
	kGPIO_IntRisingEdge = 3U,
	kGPIO_IntFallingEdge = 4U,
	kGPIO_IntRisingOrFallingEdge = 5U,
};

typedef enum gpio_interrupt_mode gpio_interrupt_mode_t;

/*
 * GPIO pin configuration.
 *
 * outputLogic is used only when the pin is configured as output.
 * interruptMode selects the GPIO interrupt trigger mode.
 */
struct gpio_pin_config {
	gpio_pin_direction_t direction;
	uint8_t outputLogic;
	gpio_interrupt_mode_t interruptMode;
};

typedef struct gpio_pin_config gpio_pin_config_t;

void gpio_init(GPIO_Type *base, int pin, gpio_pin_config_t *config);
int gpio_pinread(GPIO_Type *base, int pin);
void gpio_pinwrite(GPIO_Type *base, int pin, int value);
void gpio_intconfig(GPIO_Type *base,
		    unsigned int pin,
		    gpio_interrupt_mode_t pinInterruptMode);
void gpio_enableint(GPIO_Type *base, unsigned int pin);
void gpio_disableint(GPIO_Type *base, unsigned int pin);
void gpio_clearintflags(GPIO_Type *base, unsigned int pin);

#endif /* _BSP_GPIO_H */