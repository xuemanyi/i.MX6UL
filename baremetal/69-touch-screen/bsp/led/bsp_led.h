#ifndef BSP_LED_H
#define BSP_LED_H

#include "imx6ul.h"

#define LED0    0

/**
 * led_init() - 初始化 LED GPIO
 */
void led_init(void);

/**
 * led_switch() - 切换指定 LED 状态
 * @led: LED 编号，当前仅支持 LED0
 * @status: ON 表示点亮，OFF 表示熄灭
 */
void led_switch(int led, int status);

#endif /* BSP_LED_H */
