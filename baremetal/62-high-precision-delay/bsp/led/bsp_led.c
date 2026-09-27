#include "bsp_led.h"

#define LED0_GPIO       GPIO1
#define LED0_PIN        3U
#define LED0_PIN_MASK   (1U << LED0_PIN)

/*
 * 初始化 LED GPIO。
 *
 * LED0 连接到 GPIO1_IO03，低电平有效。
 */
void led_init(void)
{
	/*
	 * 将 GPIO1_IO03 复用为 GPIO 功能。
	 */
	IOMUXC_SetPinMux(IOMUXC_GPIO1_IO03_GPIO1_IO03, 0);

	/*
	 * 配置 GPIO1_IO03 的 PAD 电气属性。
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
	IOMUXC_SetPinConfig(IOMUXC_GPIO1_IO03_GPIO1_IO03, 0x10b0);

	/*
	 * 将 GPIO1_IO03 设置为输出。
	 */
	LED0_GPIO->GDIR |= LED0_PIN_MASK;

	/*
	 * 初始化后默认点亮 LED0。
	 */
	LED0_GPIO->DR &= ~LED0_PIN_MASK;
}

/*
 * 切换 LED 状态。
 *
 * @led: LED 编号
 * @status: ON 表示点亮，OFF 表示熄灭
 */
void led_switch(int led, int status)
{
	if (led != LED0)
		return;

	if (status == ON)
		LED0_GPIO->DR &= ~LED0_PIN_MASK;
	else
		LED0_GPIO->DR |= LED0_PIN_MASK;
}