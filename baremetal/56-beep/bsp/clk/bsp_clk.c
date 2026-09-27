#include "bsp_clk.h"

/*
 * 打开全部外设时钟。
 *
 * 裸机示例为便于学习直接打开全部时钟；产品代码应只使能实际需要的时钟以降低功耗。
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