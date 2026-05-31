#ifndef __BSP_LED_H
#define __BSP_LED_H

/*
 * LED driver interface.
 */

#include "imx6ul.h"

#define LED0    0

void led_init(void);
void led_switch(int led, int status);

#endif /* __BSP_LED_H */