#include "main.h"

#define LED_MODE_FAST_BLINK     1
#define LED_MODE_SLOW_BLINK     2
#define LED_MODE_BREATHING      3

#ifndef LED_MODE
#define LED_MODE                LED_MODE_BREATHING
#endif

#define FAST_BLINK_DELAY_MS     100
#define SLOW_BLINK_DELAY_MS     500
#define BREATH_PWM_PERIOD       50
#define BREATH_PWM_TICK_DELAY   0x3ff
#define BREATH_STEP_HOLD_MS     8

/**
 * clk_enable() - 打开全部外设时钟门控
 */
static void clk_enable(void)
{
    CCM_CCGR0 = 0xffffffff;
    CCM_CCGR1 = 0xffffffff;
    CCM_CCGR2 = 0xffffffff;
    CCM_CCGR3 = 0xffffffff;
    CCM_CCGR4 = 0xffffffff;
    CCM_CCGR5 = 0xffffffff;
    CCM_CCGR6 = 0xffffffff;
}

/**
 * led_init() - 将 GPIO1_IO03 初始化为 LED 输出
 */
static void led_init(void)
{
    /*
     * 将 GPIO1_IO03 复用为 GPIO 功能。
     */
    SW_MUX_GPIO1_IO03 = 0x5;

    /*
     * 配置 PAD 电气属性。
     */
    SW_PAD_GPIO1_IO03 = 0x10B0;

    /*
     * 仅修改 bit3，将 GPIO1_IO03 设置为输出。
     */
    GPIO1_GDIR |= LED_GPIO_BIT;

    /*
     * 开发板 LED 低电平有效，初始化后默认点亮。
     */
    GPIO1_DR &= ~LED_GPIO_BIT;
}

/**
 * led_on() - 点亮低电平有效的 LED
 */
static void led_on(void)
{
    GPIO1_DR &= ~LED_GPIO_BIT;
}

/**
 * led_off() - 熄灭低电平有效的 LED
 */
static void led_off(void)
{
    GPIO1_DR |= LED_GPIO_BIT;
}

/**
 * delay_short() - 执行短忙等待
 * @n: 循环次数
 */
static void delay_short(volatile unsigned int n)
{
    while (n--) {
    }
}

/**
 * delay() - 执行近似毫秒级忙等待
 * @n: 近似延时毫秒数
 *
 * 实际时长受 CPU 频率和编译优化影响，不提供精确计时保证。
 */
#if LED_MODE == LED_MODE_FAST_BLINK || LED_MODE == LED_MODE_SLOW_BLINK
static void delay(volatile unsigned int n)
{
    while (n--) {
        delay_short(0x7ff);
    }
}
#endif

#if LED_MODE == LED_MODE_FAST_BLINK || LED_MODE == LED_MODE_SLOW_BLINK
/**
 * led_blink() - 按指定亮灭间隔持续闪烁 LED
 * @on_delay_ms: 近似点亮时间
 * @off_delay_ms: 近似熄灭时间
 */
static void led_blink(unsigned int on_delay_ms, unsigned int off_delay_ms)
{
    while (1) {
        led_on();
        delay(on_delay_ms);

        led_off();
        delay(off_delay_ms);
	}
}
#endif

#if LED_MODE == LED_MODE_FAST_BLINK
/**
 * led_fast_blink_mode() - 进入快速闪烁模式
 */
static void led_fast_blink_mode(void)
{
	led_blink(FAST_BLINK_DELAY_MS, FAST_BLINK_DELAY_MS);
}
#endif

#if LED_MODE == LED_MODE_SLOW_BLINK
/**
 * led_slow_blink_mode() - 进入慢速闪烁模式
 */
static void led_slow_blink_mode(void)
{
	led_blink(SLOW_BLINK_DELAY_MS, SLOW_BLINK_DELAY_MS);
}
#endif

#if LED_MODE == LED_MODE_BREATHING
/**
 * led_soft_pwm_cycle() - 执行一个软件 PWM 周期
 * @on_ticks: 周期内点亮的时间片数量
 * @period_ticks: 周期总时间片数量
 */
static void led_soft_pwm_cycle(unsigned int on_ticks, unsigned int period_ticks)
{
	unsigned int i;

	for (i = 0; i < period_ticks; i++) {
		if (i < on_ticks)
			led_on();
		else
			led_off();

		delay_short(BREATH_PWM_TICK_DELAY);
	}
}

/**
 * led_breathing_mode() - 使用软件 PWM 模拟呼吸灯
 */
static void led_breathing_mode(void)
{
	unsigned int duty;
	unsigned int hold;

	while (1) {
		for (duty = 0; duty <= BREATH_PWM_PERIOD; duty++) {
			for (hold = 0; hold < BREATH_STEP_HOLD_MS; hold++)
				led_soft_pwm_cycle(duty, BREATH_PWM_PERIOD);
		}

		for (duty = BREATH_PWM_PERIOD; duty > 0; duty--) {
			for (hold = 0; hold < BREATH_STEP_HOLD_MS; hold++)
				led_soft_pwm_cycle(duty, BREATH_PWM_PERIOD);
		}
	}
}
#endif

/**
 * main() - 裸机 C 入口
 *
 * Return: 裸机主循环不会正常返回。
 */
int main(void)
{
    clk_enable();
    led_init();

#if LED_MODE == LED_MODE_FAST_BLINK
    led_fast_blink_mode();
#elif LED_MODE == LED_MODE_SLOW_BLINK
    led_slow_blink_mode();
#elif LED_MODE == LED_MODE_BREATHING
    led_breathing_mode();
#else
    while (1) {
        led_off();
    }
#endif

    return 0;
}
