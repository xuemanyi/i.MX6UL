#include "bsp_clk.h"

/*
 * Enable all peripheral clocks.
 *
 * This is convenient for bare-metal learning. In real products,
 * only required clocks should be enabled to reduce power consumption.
 */
void clk_enable(void)
{
	CCM->CCGR0 = 0xffffffff;
	CCM->CCGR1 = 0xffffffff;
	CCM->CCGR2 = 0xffffffff;
	CCM->CCGR3 = 0xffffffff;
	CCM->CCGR4 = 0xffffffff;
	CCM->CCGR5 = 0xffffffff;
	CCM->CCGR6 = 0xffffffff;
}