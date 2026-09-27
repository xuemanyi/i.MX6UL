#ifndef BSP_EXIT_H
#define BSP_EXIT_H

#include "imx6ul.h"

/**
 * exit_init() - 初始化 KEY0 对应的 GPIO1_IO18 外部中断
 */
void exit_init(void);

/**
 * gpio1_io18_irqhandler() - 处理 GPIO1_IO18 中断
 * @giccIar: GIC 中断确认值
 * @userParam: 注册时传入的用户参数
 *
 * Context: IRQ 分发路径调用，不允许睡眠。
 */
void gpio1_io18_irqhandler(unsigned int giccIar, void *userParam);

#endif /* BSP_EXIT_H */
