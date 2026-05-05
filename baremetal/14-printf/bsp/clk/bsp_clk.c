#include "bsp_clk.h"

/*
 * Enable all peripheral clocks.
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
 * Initialize the system clock tree.
 *
 * Clock targets:
 *   ARM core clock : 792 MHz
 *   PLL2 PFD0      : 352 MHz
 *   PLL2 PFD1      : 594 MHz
 *   PLL2 PFD2      : 396 MHz
 *   PLL2 PFD3      : 297 MHz
 *   PLL3 PFD0      : 720 MHz
 *   PLL3 PFD1      : 540 MHz
 *   PLL3 PFD2      : 508.24 MHz
 *   PLL3 PFD3      : 454.74 MHz
 *   AHB clock      : 132 MHz
 *   IPG clock      : 66 MHz
 *   PERCLK clock   : 66 MHz
 */
void imx6u_clkinit(void)
{
	unsigned int reg;

	/*
	 * Switch ARM clock source to step_clk before reconfiguring PLL1.
	 *
	 * pll1_sw_clk can be sourced from either pll1_main_clk or step_clk.
	 * Reprogramming PLL1 while ARM is running from PLL1 is unsafe, so move
	 * ARM temporarily to step_clk sourced from the 24 MHz oscillator.
	 */
	if (((CCM->CCSR >> 2) & 0x1) == 0) {
		CCM->CCSR &= ~(1 << 8);
		CCM->CCSR |= (1 << 2);
	}

	/*
	 * Configure PLL1 ARM clock.
	 *
	 * PLL_ARM output:
	 *
	 *   Fout = Fin * DIV_SELECT / 2
	 *
	 * With Fin = 24 MHz and DIV_SELECT = 66:
	 *
	 *   Fout = 24 * 66 / 2 = 792 MHz
	 */
	CCM_ANALOG->PLL_ARM = (1 << 13) | (66 & 0x7f);

	/*
	 * Switch ARM clock source back to PLL1 and use divider 1.
	 */
	CCM->CCSR &= ~(1 << 2);
	CCM->CACRR = 0;

	/*
	 * Configure PLL2 SYS PFD outputs.
	 *
	 * PFD output formula:
	 *
	 *   Fout = PLL * 18 / FRAC
	 *
	 * PLL2 base clock is 528 MHz.
	 */
	reg = CCM_ANALOG->PFD_528;
	reg &= ~0x3f3f3f3f;
	reg |= 32 << 24;	/* PLL2_PFD3 = 528 * 18 / 32 = 297 MHz */
	reg |= 24 << 16;	/* PLL2_PFD2 = 528 * 18 / 24 = 396 MHz */
	reg |= 16 << 8;		/* PLL2_PFD1 = 528 * 18 / 16 = 594 MHz */
	reg |= 27 << 0;		/* PLL2_PFD0 = 528 * 18 / 27 = 352 MHz */
	CCM_ANALOG->PFD_528 = reg;

	/*
	 * Configure PLL3 USB1 PFD outputs.
	 *
	 * PLL3 base clock is 480 MHz.
	 */
	reg = CCM_ANALOG->PFD_480;
	reg &= ~0x3f3f3f3f;
	reg |= 19 << 24;	/* PLL3_PFD3 = 480 * 18 / 19 = 454.74 MHz */
	reg |= 17 << 16;	/* PLL3_PFD2 = 480 * 18 / 17 = 508.24 MHz */
	reg |= 16 << 8;		/* PLL3_PFD1 = 480 * 18 / 16 = 540 MHz */
	reg |= 12 << 0;		/* PLL3_PFD0 = 480 * 18 / 12 = 720 MHz */
	CCM_ANALOG->PFD_480 = reg;

	/*
	 * Select PLL2_PFD2 as pre_periph_clk.
	 *
	 * pre_periph_clk = 396 MHz
	 * periph_clk     = pre_periph_clk
	 */
	CCM->CBCMR &= ~(3 << 18);
	CCM->CBCMR |= 1 << 18;

	CCM->CBCDR &= ~(1 << 25);

	while (CCM->CDHIPR & (1 << 5)) {
	}

	/*
	 * AHB_PODF is normally configured by Boot ROM.
	 *
	 * Default:
	 *
	 *   AHB_CLK_ROOT = 396 MHz / 3 = 132 MHz
	 *
	 * Do not modify AHB_PODF here because AHB_CLK_ROOT should be gated
	 * before changing the divider.
	 */

	/*
	 * Configure IPG clock.
	 *
	 * IPG_CLK_ROOT = AHB_CLK_ROOT / 2 = 66 MHz
	 */
	CCM->CBCDR &= ~(3 << 8);
	CCM->CBCDR |= 1 << 8;

	/*
	 * Configure PERCLK clock.
	 *
	 * PERCLK_CLK_ROOT source = IPG_CLK_ROOT
	 * PERCLK_PODF           = divide by 1
	 */
	CCM->CSCMR1 &= ~(1 << 6);
	CCM->CSCMR1 &= ~(7 << 0);
}