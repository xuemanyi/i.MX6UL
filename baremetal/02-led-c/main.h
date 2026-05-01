#ifndef __MAIN_H
#define __MAIN_H

/*
 * 32-bit memory-mapped register access macro.
 */
#define REG32(addr) (*(volatile unsigned int *)(addr))

/*
 * CCM clock gating registers.
 */
#define CCM_CCGR0           REG32(0x020C4068)
#define CCM_CCGR1           REG32(0x020C406C)
#define CCM_CCGR2           REG32(0x020C4070)
#define CCM_CCGR3           REG32(0x020C4074)
#define CCM_CCGR4           REG32(0x020C4078)
#define CCM_CCGR5           REG32(0x020C407C)
#define CCM_CCGR6           REG32(0x020C4080)

/*
 * IOMUX registers for GPIO1_IO03.
 */
#define SW_MUX_GPIO1_IO03   REG32(0x020E0068)
#define SW_PAD_GPIO1_IO03   REG32(0x020E02F4)

/*
 * GPIO1 registers.
 */
#define GPIO1_DR            REG32(0x0209C000)
#define GPIO1_GDIR          REG32(0x0209C004)
#define GPIO1_PSR           REG32(0x0209C008)
#define GPIO1_ICR1          REG32(0x0209C00C)
#define GPIO1_ICR2          REG32(0x0209C010)
#define GPIO1_IMR           REG32(0x0209C014)
#define GPIO1_ISR           REG32(0x0209C018)
#define GPIO1_EDGE_SEL      REG32(0x0209C01C)

/*
 * GPIO1_IO03 bit mask.
 */
#define LED_GPIO_BIT        (1U << 3)

#endif