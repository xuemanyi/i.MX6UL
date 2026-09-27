#include "bsp_clk.h"

/*
 * 打开全部外设时钟。
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

/*
 * 初始化系统时钟树。
 *
 * 目标频率：ARM 核心 792 MHz，PLL2 PFD0～PFD3 分别为
 * 352/594/396/297 MHz，PLL3 PFD0～PFD3 分别为
 * 720/540/508.24/454.74 MHz，AHB/IPG/PERCLK 分别为
 * 132/66/66 MHz。
 */
void imx6u_clkinit(void)
{
	unsigned int reg;

	/*
	 * 运行中直接改写当前供给 ARM 的 PLL1 会破坏时钟稳定性，
	 * 因此先将 ARM 临时切换到由 24 MHz 晶振提供的 step_clk。
	 */
	if (((CCM->CCSR >> 2) & 0x1) == 0) {
		CCM->CCSR &= ~(1 << 8);
		CCM->CCSR |= (1 << 2);
	}

	/*
	 * 配置 PLL1 ARM 时钟。计算公式为 Fout=Fin*DIV_SELECT/2，
	 * 输入 24 MHz、DIV_SELECT=66 时输出 792 MHz。
	 */
	CCM_ANALOG->PLL_ARM = (1 << 13) | (66 & 0x7f);

	/*
	 * PLL1 稳定后切回 PLL1，并使用 1 分频。
	 */
	CCM->CCSR &= ~(1 << 2);
	CCM->CACRR = 0;

	/*
	 * 配置 PLL2 SYS PFD 输出。PLL2 基准频率为 528 MHz，
	 * PFD 输出公式为 Fout=PLL*18/FRAC。
	 */
	reg = CCM_ANALOG->PFD_528;
	reg &= ~0x3f3f3f3f;
	reg |= 32 << 24;	/* PLL2_PFD3 = 528 * 18 / 32 = 297 MHz */
	reg |= 24 << 16;	/* PLL2_PFD2 = 528 * 18 / 24 = 396 MHz */
	reg |= 16 << 8;		/* PLL2_PFD1 = 528 * 18 / 16 = 594 MHz */
	reg |= 27 << 0;		/* PLL2_PFD0 = 528 * 18 / 27 = 352 MHz */
	CCM_ANALOG->PFD_528 = reg;

	/*
	 * 配置 PLL3 USB1 PFD 输出，PLL3 基准频率为 480 MHz。
	 */
	reg = CCM_ANALOG->PFD_480;
	reg &= ~0x3f3f3f3f;
	reg |= 19 << 24;	/* PLL3_PFD3 = 480 * 18 / 19 = 454.74 MHz */
	reg |= 17 << 16;	/* PLL3_PFD2 = 480 * 18 / 17 = 508.24 MHz */
	reg |= 16 << 8;		/* PLL3_PFD1 = 480 * 18 / 16 = 540 MHz */
	reg |= 12 << 0;		/* PLL3_PFD0 = 480 * 18 / 12 = 720 MHz */
	CCM_ANALOG->PFD_480 = reg;

	/*
	 * 选择 396 MHz 的 PLL2_PFD2 作为 pre_periph_clk，
	 * 并让 periph_clk 直接使用该时钟。
	 */
	CCM->CBCMR &= ~(3 << 18);
	CCM->CBCMR |= 1 << 18;

	CCM->CBCDR &= ~(1 << 25);

	while (CCM->CDHIPR & (1 << 5)) {
	}

	/*
	 * AHB_PODF 通常由 Boot ROM 配置，默认得到 132 MHz。
	 * 修改分频前必须先门控 AHB_CLK_ROOT，因此此处保持 Boot ROM 配置。
	 */

	/*
	 * 配置 IPG 时钟。
	 *
	 * IPG_CLK_ROOT = AHB_CLK_ROOT / 2 = 66 MHz
	 */
	CCM->CBCDR &= ~(3 << 8);
	CCM->CBCDR |= 1 << 8;

	/*
	 * 配置 PERCLK 时钟。
	 *
	 * PERCLK_CLK_ROOT 选择 IPG_CLK_ROOT，分频系数为 1。
	 */
	CCM->CSCMR1 &= ~(1 << 6);
	CCM->CSCMR1 &= ~(7 << 0);
}
