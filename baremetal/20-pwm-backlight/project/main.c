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
#include "bsp_backlight.h"
#include "stdio.h"

/*
 * @description	: 使能I.MX6U的硬件NEON和FPU
 * @param 		: 无
 * @return 		: 无
 */
 void imx6ul_hardfpu_enable(void)
{
	uint32_t cpacr;
	uint32_t fpexc;

	/* 使能NEON和FPU */
	cpacr = __get_CPACR();
	cpacr = (cpacr & ~(CPACR_ASEDIS_Msk | CPACR_D32DIS_Msk))
		   |  (3UL << CPACR_cp10_Pos) | (3UL << CPACR_cp11_Pos);
	__set_CPACR(cpacr);
	fpexc = __get_FPEXC();
	fpexc |= 0x40000000UL;	
	__set_FPEXC(fpexc);
}

/*
 * @description	: main函数
 * @param 		: 无
 * @return 		: 无
 */
int main(void)
{
	unsigned char keyvalue = 0;
	unsigned char i = 0;
	unsigned char state = OFF;
	unsigned char duty = 0;
	
	imx6ul_hardfpu_enable();	/* 使能I.MX6U的硬件浮点 			*/
	int_init(); 				/* 初始化中断(一定要最先调用！) */
	imx6u_clkinit();			/* 初始化系统时钟 					*/
	delay_init();				/* 初始化延时 					*/
	clk_enable();				/* 使能所有的时钟 					*/
	led_init();					/* 初始化led 					*/
	beep_init();				/* 初始化beep	 				*/
	uart_init();				/* 初始化串口，波特率115200 */
	lcd_init();					/* 初始化LCD 					*/
	backlight_init();			/* 初始化背光PWM			 		*/ 


	tftlcd_dev.forecolor = LCD_RED;
	lcd_show_string(50, 10, 400, 24, 24, (char*)"ZERO-IMX6U BACKLIGHT PWM TEST");  
	lcd_show_string(50, 40, 200, 16, 16, (char*)"ATOM@ALIENTEK");  
	lcd_show_string(50, 60, 200, 16, 16, (char*)"2019/3/27");   
	lcd_show_string(50, 90, 400, 16, 16, (char*)"PWM Duty:   %");  
	tftlcd_dev.forecolor = LCD_BLUE;

	/* 设置默认占空比 10% */
	if(tftlcd_dev.id == ATKVGA)
		duty=100;	//VGA只能在满输出时才能亮屏
	else
		duty = 10;
	lcd_shownum(50 + 72, 90, duty, 3, 16);
	pwm1_setduty(duty);	
	
	while(1)					
	{
		keyvalue = key_getvalue();
		if(keyvalue == KEY0_VALUE)
		{
			duty += 10;				/* 占空比加10% */
			if(duty > 100)			/* 如果占空比超过100%，重新从10%开始 */
				duty = 10;
			lcd_shownum(50 + 72, 90, duty, 3, 16);
			pwm1_setduty(duty);		/* 设置占空比 */
		}
		
		delayms(10);
		i++;
		if(i == 50)
		{	
			i = 0;
			state = !state;
			led_switch(LED0,state);	
		}
	}
	return 0;
}
