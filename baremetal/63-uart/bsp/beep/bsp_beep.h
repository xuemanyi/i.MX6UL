#ifndef BSP_BEEP_H
#define BSP_BEEP_H

#include "imx6ul.h"

/**
 * beep_init() - 初始化蜂鸣器 GPIO
 */
void beep_init(void);

/**
 * beep_switch() - 切换蜂鸣器状态
 * @status: ON 表示鸣响，OFF 表示关闭
 */
void beep_switch(int status);

#endif /* BSP_BEEP_H */
