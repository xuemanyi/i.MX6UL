#include "bsp_exit.h"
#include "bsp_gpio.h"
#include "bsp_int.h"
#include "bsp_delay.h"
#include "bsp_beep.h"

/*
 * Initialize external interrupt for GPIO1_IO18.
 */
void exit_init(void)
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
				   gpio1_io18_irqhandler,
				   NULL);

	gpio_enableint(GPIO1, 18);
}

/*
 * GPIO1_IO18 interrupt handler.
 *
 * The delay-based debounce method is used only for this bare-metal demo.
 * Production IRQ handlers should keep the ISR short and defer debounce
 * handling to a timer or bottom-half style routine.
 */
void gpio1_io18_irqhandler(unsigned int giccIar, void *userParam)
{
	static unsigned char state;

	(void)giccIar;
	(void)userParam;

	delay(10);

	if (gpio_pinread(GPIO1, 18) == 0) {
		state = !state;
		beep_switch(state);
	}

	gpio_clearintflags(GPIO1, 18);
}