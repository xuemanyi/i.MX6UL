#ifndef BSP_DELAY_H
#define BSP_DELAY_H

/*
 * 忙等待延时接口。
 */

#include "imx6ul.h"

void delay_short(volatile unsigned int loops);
void delay(volatile unsigned int ms);

#endif /* BSP_DELAY_H */