#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_int.h"
#include "bsp_uart.h"
#include "stdio.h"
#include "bsp_lcd.h"
#include "bsp_lcdapi.h"

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

/* 背景颜色索引 */
static const unsigned int backcolor[] = {
    LCD_BLUE, LCD_GREEN, LCD_RED, LCD_CYAN, LCD_YELLOW,
    LCD_LIGHTBLUE, LCD_DARKBLUE, LCD_WHITE, LCD_BLACK, LCD_ORANGE
};

static void board_init(void)
{
    int_init();          /* 初始化中断(一定要最先调用！) */
    imx6u_clkinit();     /* 初始化系统时钟 */
    delay_init();        /* 初始化延时 */
    clk_enable();        /* 使能所有时钟 */
    led_init();          /* 初始化LED */
    beep_init();         /* 初始化蜂鸣器 */
    uart_init();         /* 初始化串口，波特率115200 */
    lcd_init();          /* 初始化LCD */
}

static void lcd_show_demo_text(void)
{
    lcd_show_string(10, 40, 260, 32, 32, (char *)"ALPHA IMX6U");
    lcd_show_string(10, 80, 240, 24, 24, (char *)"RGBLCD TEST");
    lcd_show_string(10, 110, 240, 16, 16, (char *)"ATOM@ALIENTEK");
    lcd_show_string(10, 130, 240, 12, 12, (char *)"2019/8/14");
}

int main(void)
{
    unsigned char index = 0;
    unsigned char state = OFF;

    board_init();
    tftlcd_dev.forecolor = LCD_RED;

    while (1) {
        lcd_clear(backcolor[index]);
        delayms(1);
        lcd_show_demo_text();

        index++;
        if (index >= ARRAY_SIZE(backcolor)) {
            index = 0;
        }

        state = !state;
        led_switch(LED0, state);
        delayms(1000);
    }

    return 0;
}
