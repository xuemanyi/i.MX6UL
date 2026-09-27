#include "bsp_delay.h"

/*
 * 执行短忙等待。
 *
 * 该函数仅消耗 CPU 周期，实际延时时长取决于 CPU 频率、编译优化和存储器时序。
 */
void delay_short(volatile unsigned int loops)
{
	while (loops--)
		;
}

/*
 * 执行近似毫秒级忙等待。
 *
 * CPU 运行在约 396 MHz 时，该延时值仅为近似值。
 */
void delay(volatile unsigned int ms)
{
	while (ms--)
		delay_short(0x7ff);
}