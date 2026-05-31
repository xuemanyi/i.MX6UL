#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_int.h"
#include "bsp_uart.h"
#include "bsp_lcd.h"
#include "bsp_lcdapi.h"
#include "bsp_rtc.h"
#include "stdio.h"
#include "string.h"

#define STARTUP_COUNTDOWN_S     (3)
#define STARTUP_TICK_PER_SECOND (100)
#define STARTUP_TICK_DELAY_MS   (10)

static void board_init(void)
{
    int_init();        /* 初始化中断(一定要最先调用！) */
    imx6u_clkinit();   /* 初始化系统时钟 */
    delay_init();      /* 初始化延时 */
    clk_enable();      /* 使能所有的时钟 */
    led_init();        /* 初始化LED */
    beep_init();       /* 初始化蜂鸣器 */
    uart_init();       /* 初始化串口，波特率115200 */
    lcd_init();        /* 初始化LCD */
    rtc_init();        /* 初始化RTC */
}

static void lcd_show_header(void)
{
    tftlcd_dev.forecolor = LCD_RED;
    lcd_show_string(50, 10, 400, 24, 24, (char *)"ALPHA-IMX6UL RTC TEST");
    lcd_show_string(50, 40, 200, 16, 16, (char *)"ATOM@ALIENTEK");
    lcd_show_string(50, 60, 200, 16, 16, (char *)"2019/3/27");
    tftlcd_dev.forecolor = LCD_BLUE;
}

static void rtc_set_default_datetime(void)
{
    struct rtc_datetime rtcdate;

    rtcdate.year = 2018U;
    rtcdate.month = 1U;
    rtcdate.day = 15U;
    rtcdate.hour = 16U;
    rtcdate.minute = 23U;
    rtcdate.second = 0U;

    rtc_setdatetime(&rtcdate);
}

/*
 * @description : main函数
 * @param       : 无
 * @return      : 无
 */
int main(void)
{
    unsigned char key;
    int countdown = STARTUP_COUNTDOWN_S;
    int tick = 0;
    char buf[160];
    struct rtc_datetime rtcdate;
    unsigned char state = OFF;

    board_init();
    lcd_show_header();
    memset(buf, 0, sizeof(buf));

    while (1) {
        if (tick == STARTUP_TICK_PER_SECOND) { /* 1s时间到了 */
            tick = 0;
            printf("will be running %d s......\r", countdown);

            lcd_fill(50, 90, 370, 110, tftlcd_dev.backcolor);
            sprintf(buf, "will be running %ds......", countdown);
            lcd_show_string(50, 90, 300, 16, 16, buf);

            countdown--;
            if (countdown < 0) {
                break;
            }
        }

        key = key_getvalue();
        if (key == KEY0_VALUE) {
            rtc_set_default_datetime();
            printf("\r\n RTC Init finish\r\n");
            break;
        }

        delayms(STARTUP_TICK_DELAY_MS);
        tick++;
    }

    tftlcd_dev.forecolor = LCD_RED;
    lcd_fill(50, 90, 370, 110, tftlcd_dev.backcolor);
    lcd_show_string(50, 90, 200, 16, 16, (char *)"Current Time:");
    tftlcd_dev.forecolor = LCD_BLUE;

    while (1) {
        rtc_getdatetime(&rtcdate);
        sprintf(buf, "%d/%d/%d %d:%d:%d",
                rtcdate.year,
                rtcdate.month,
                rtcdate.day,
                rtcdate.hour,
                rtcdate.minute,
                rtcdate.second);

        lcd_fill(50, 110, 300, 130, tftlcd_dev.backcolor);
        lcd_show_string(50, 110, 250, 16, 16, buf);

        state = !state;
        led_switch(LED0, state);
        delayms(1000);
    }

    return 0;
}
