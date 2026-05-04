#ifndef __BSP_DELAY_H
#define __BSP_DELAY_H

#include "imx6ul.h"

void delay_init(void);
void delayus(unsigned int usdelay);
void delayms(unsigned int msdelay);
void delay_short(volatile unsigned int n);
void delay(volatile unsigned int n);

#endif /* __BSP_DELAY_H */