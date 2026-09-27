#include "bsp_beep.h"

#define BEEP_GPIO        GPIO5
#define BEEP_PIN         1U
#define BEEP_PIN_MASK    (1U << BEEP_PIN)

/*
 * 初始化蜂鸣器 GPIO。
 *
 * 蜂鸣器连接到 GPIO5_IO01，低电平有效。
 */
void beep_init(void)
{
	/*
	 * 将 SNVS_TAMPER1 复用为 GPIO5_IO01。
	 */
	IOMUXC_SetPinMux(IOMUXC_SNVS_SNVS_TAMPER1_GPIO5_IO01, 0);

	/*
	 * 配置 GPIO5_IO01 的 PAD 电气属性。
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
	 * 将 GPIO5_IO01 设置为输出。
	 */
	BEEP_GPIO->GDIR |= BEEP_PIN_MASK;

	/*
	 * 初始化后默认关闭蜂鸣器。
	 *
	 * 蜂鸣器低电平有效。
	 */
	BEEP_GPIO->DR |= BEEP_PIN_MASK;
}

/*
 * 切换蜂鸣器状态。
 *
 * @status: ON 表示鸣响，OFF 表示关闭
 */
void beep_switch(int status)
{
	if (status == ON)
		BEEP_GPIO->DR &= ~BEEP_PIN_MASK;
	else if (status == OFF)
		BEEP_GPIO->DR |= BEEP_PIN_MASK;
}