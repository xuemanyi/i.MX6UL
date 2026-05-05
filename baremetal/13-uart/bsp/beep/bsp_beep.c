#include "bsp_beep.h"

#define BEEP_GPIO        GPIO5
#define BEEP_PIN         1U
#define BEEP_PIN_MASK    (1U << BEEP_PIN)

/*
 * Initialize buzzer GPIO.
 *
 * The buzzer is connected to GPIO5_IO01 and is active low.
 */
void beep_init(void)
{
	/*
	 * Configure SNVS_TAMPER1 as GPIO5_IO01.
	 */
	IOMUXC_SetPinMux(IOMUXC_SNVS_SNVS_TAMPER1_GPIO5_IO01, 0);

	/*
	 * Configure GPIO5_IO01 pad control.
	 *
	 * bit 16    : HYS disabled
	 * bit 15:14 : default pull-down
	 * bit 13    : keeper
	 * bit 12    : pull/keeper enabled
	 * bit 11    : open-drain disabled
	 * bit 7:6   : medium speed, 100 MHz
	 * bit 5:3   : R0/6 drive strength
	 * bit 0     : slow slew rate
	 */
	IOMUXC_SetPinConfig(IOMUXC_SNVS_SNVS_TAMPER1_GPIO5_IO01, 0x10b0);

	/*
	 * Set GPIO5_IO01 as output.
	 */
	BEEP_GPIO->GDIR |= BEEP_PIN_MASK;

	/*
	 * Turn buzzer off by default.
	 *
	 * The buzzer is active low.
	 */
	BEEP_GPIO->DR |= BEEP_PIN_MASK;
}

/*
 * Switch buzzer state.
 *
 * @status: ON to enable buzzer, OFF to disable buzzer
 */
void beep_switch(int status)
{
	if (status == ON)
		BEEP_GPIO->DR &= ~BEEP_PIN_MASK;
	else if (status == OFF)
		BEEP_GPIO->DR |= BEEP_PIN_MASK;
}