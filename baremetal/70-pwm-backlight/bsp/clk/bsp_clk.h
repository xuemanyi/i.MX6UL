#ifndef BSP_CLK_H
#define BSP_CLK_H

#include "imx6ul.h"

/**
 * clk_enable() - 打开全部外设时钟门控
 */
void clk_enable(void);

/**
 * imx6u_clkinit() - 初始化处理器主频和外设时钟树
 *
 * Context: 裸机启动阶段调用，调用期间不得访问依赖待切换时钟的外设。
 */
void imx6u_clkinit(void);

#endif /* BSP_CLK_H */
