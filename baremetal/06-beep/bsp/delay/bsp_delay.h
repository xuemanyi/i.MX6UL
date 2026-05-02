#ifndef __BSP_DELAY_H
#define __BSP_DELAY_H

/*
 * Busy-wait delay interface.
 */

#include "imx6ul.h"

void delay_short(volatile unsigned int loops);
void delay(volatile unsigned int ms);

#endif /* __BSP_DELAY_H */