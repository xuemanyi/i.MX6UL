#ifndef __BSP_CLK_H
#define __BSP_CLK_H

/*
 * Clock driver interface.
 */

#include "imx6ul.h"

void clk_enable(void);
void imx6u_clkinit(void);

#endif /* __BSP_CLK_H */