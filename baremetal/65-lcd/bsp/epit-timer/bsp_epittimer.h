#ifndef BSP_EPITTIMER_H
#define BSP_EPITTIMER_H

#include "imx6ul.h"

/**
 * epit1_init() - 初始化并启动 EPIT1 周期定时器
 * @frac: 时钟预分频值
 * @value: 计数器重装值
 */
void epit1_init(unsigned int frac, unsigned int value);

/**
 * epit1_irqhandler() - 处理 EPIT1 定时中断
 * @giccIar: GIC 中断确认值
 * @userParam: 注册时传入的用户参数
 *
 * Context: IRQ 分发路径调用，不允许睡眠。
 */
void epit1_irqhandler(unsigned int giccIar, void *userParam);

#endif /* BSP_EPITTIMER_H */
