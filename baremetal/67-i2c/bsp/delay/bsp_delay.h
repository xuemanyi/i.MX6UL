#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include "imx6ul.h"

/**
 * delay_init() - 初始化 GPT1 高精度延时计数器
 */
void delay_init(void);

/**
 * delayus() - 忙等待指定微秒数
 * @usdelay: 延时微秒数
 */
void delayus(unsigned int usdelay);

/**
 * delayms() - 忙等待指定毫秒数
 * @msdelay: 延时毫秒数
 */
void delayms(unsigned int msdelay);

/**
 * delay_short() - 执行兼容旧示例的短软件延时
 * @n: 循环次数
 */
void delay_short(volatile unsigned int n);

/**
 * delay() - 执行兼容旧示例的粗略延时
 * @n: 延时循环次数
 */
void delay(volatile unsigned int n);

#endif /* BSP_DELAY_H */
