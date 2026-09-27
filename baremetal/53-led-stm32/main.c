#include "imx6ul.h"

#define LED_GPIO_BIT        (3U)
#define LED_PAD_CONFIG      (0x10B0U)
#define LED_MUX_GPIO_MODE   (0x5U)

static void clk_enable(void)
{
    CCM->CCGR0 = 0xFFFFFFFFU;
    CCM->CCGR1 = 0xFFFFFFFFU;
    CCM->CCGR2 = 0xFFFFFFFFU;
    CCM->CCGR3 = 0xFFFFFFFFU;
    CCM->CCGR4 = 0xFFFFFFFFU;
    CCM->CCGR5 = 0xFFFFFFFFU;
    CCM->CCGR6 = 0xFFFFFFFFU;
}

static void led_init(void)
{
    /* 将 GPIO1_IO03 复用为 GPIO 功能。 */
    IOMUX_SW_MUX->GPIO1_IO03 = LED_MUX_GPIO_MODE;

    /* 配置 GPIO1_IO03 的 PAD 电气属性。 */
    IOMUX_SW_PAD->GPIO1_IO03 = LED_PAD_CONFIG;

    /* 仅修改目标位，将 GPIO1_IO03 设置为输出。 */
    GPIO1->GDIR |= (1U << LED_GPIO_BIT);

    /* 开发板 LED 低电平有效，初始化后默认点亮。 */
    GPIO1->DR &= ~(1U << LED_GPIO_BIT);
}

static void led_on(void)
{
    /* 低电平有效。 */
    GPIO1->DR &= ~(1U << LED_GPIO_BIT);
}

static void led_off(void)
{
    GPIO1->DR |= (1U << LED_GPIO_BIT);
}

static void delay_short(volatile unsigned int count)
{
    while (count--) {
    }
}

static void delay(volatile unsigned int ms)
{
    while (ms--) {
        delay_short(0x7FFU);
    }
}

int main(void)
{
    clk_enable();
    led_init();

    while (1) {
        led_off();
        delay(500U);

        led_on();
        delay(500U);
    }

    return 0;
}
