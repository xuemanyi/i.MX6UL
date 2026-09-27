#ifndef BSP_LED_H
#define BSP_LED_H

/*
 * LED 驱动接口。
 */

#include "imx6ul.h"

#define LED0    0

void led_init(void);
void led_switch(int led, int status);

#endif /* BSP_LED_H */