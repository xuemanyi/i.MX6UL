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
    /* Configure GPIO1_IO03 mux mode */
    IOMUX_SW_MUX->GPIO1_IO03 = LED_MUX_GPIO_MODE;

    /* Configure GPIO1_IO03 pad electrical attributes */
    IOMUX_SW_PAD->GPIO1_IO03 = LED_PAD_CONFIG;

    /* Set GPIO1_IO03 as output */
    GPIO1->GDIR |= (1U << LED_GPIO_BIT);

    /* Turn on LED by outputting low level */
    GPIO1->DR &= ~(1U << LED_GPIO_BIT);
}

static void led_on(void)
{
    /* Active low LED */
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