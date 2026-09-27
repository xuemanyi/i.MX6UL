/*
 * i.MX6UL 裸机 LED 示例。
 *
 * 在 GCC 裸机环境中复用 NXP i.MX6ULL SDK 寄存器定义，
 * LED 连接到 GPIO1_IO03。
 */

#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "MCIMX6Y2.h"

#define LED_GPIO        GPIO1
#define LED_PIN         3U
#define LED_PIN_MASK    (1U << LED_PIN)

/*
 * clk_enable() - 打开全部外设时钟
 *
 * 为便于展示直接打开全部时钟门控；产品代码应只使能实际使用的外设时钟。
 */
static void clk_enable(void)
{
	CCM->CCGR0 = 0xffffffff;
	CCM->CCGR1 = 0xffffffff;
	CCM->CCGR2 = 0xffffffff;
	CCM->CCGR3 = 0xffffffff;
	CCM->CCGR4 = 0xffffffff;
	CCM->CCGR5 = 0xffffffff;
	CCM->CCGR6 = 0xffffffff;
}

/*
 * led_init() - 将 GPIO1_IO03 初始化为 LED 输出
 *
 * 依次配置引脚复用、PAD 电气属性和 GPIO 输出方向。
 */
static void led_init(void)
{
	/*
	 * 将 GPIO1_IO03 复用为 GPIO 功能。
	 */
	IOMUXC_SetPinMux(IOMUXC_GPIO1_IO03_GPIO1_IO03, 0);

	/*
	 * 配置 GPIO1_IO03 PAD：关闭迟滞和开漏，使用保持器，
	 * 速度为 100 MHz，驱动强度为 R0/6，使用慢转换速率。
	 *
	 */
	IOMUXC_SetPinConfig(IOMUXC_GPIO1_IO03_GPIO1_IO03, 0x10b0);

	/*
	 * 仅修改目标位，将 GPIO1_IO03 设置为输出。
	 */
	LED_GPIO->GDIR |= LED_PIN_MASK;

	/*
	 * LED 低电平有效，初始化后默认点亮。
	 */
	LED_GPIO->DR &= ~LED_PIN_MASK;
}

/*
 * led_on() - 点亮低电平有效的 LED
 */
static void led_on(void)
{
	LED_GPIO->DR &= ~LED_PIN_MASK;
}

/*
 * led_off() - 熄灭低电平有效的 LED
 */
static void led_off(void)
{
	LED_GPIO->DR |= LED_PIN_MASK;
}

/*
 * delay_short() - 执行短忙等待
 * @loops: 循环次数
 *
 * 该延时只消耗 CPU 周期，不提供精确计时保证。
 */
static void delay_short(volatile unsigned int loops)
{
	while (loops--)
		;
}

/*
 * delay() - 执行近似毫秒级忙等待
 * @ms: 近似延时毫秒数
 *
 * 实际时长受 CPU 频率、编译优化和存储器时序影响。
 */
static void delay(volatile unsigned int ms)
{
	while (ms--)
		delay_short(0x7ff);
}

/*
 * main() - 裸机 C 入口
 */
int main(void)
{
	clk_enable();
	led_init();

	while (1) {
		led_off();
		delay(500);

		led_on();
		delay(500);
	}

	return 0;
}
