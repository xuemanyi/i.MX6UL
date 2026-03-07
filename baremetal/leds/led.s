.global _start            /* Declare global entry symbol */

/*
 * Description:
 * The program starts from _start.
 * This function enables clocks, configures GPIO,
 * and finally outputs a low level to turn on the LED.
 */

_start:

/* ------------------------------------------------ */
/* 1. Enable all clock gates (CCM Clock Gating)     */
/* ------------------------------------------------ */

ldr r0, =0x020C4068      /* Address of CCM_CCGR0 register */
ldr r1, =0xFFFFFFFF      /* Enable all clocks */
str r1, [r0]

ldr r0, =0x020C406C      /* Address of CCM_CCGR1 register */
str r1, [r0]

ldr r0, =0x020C4070      /* Address of CCM_CCGR2 register */
str r1, [r0]

ldr r0, =0x020C4074      /* Address of CCM_CCGR3 register */
str r1, [r0]

ldr r0, =0x020C4078      /* Address of CCM_CCGR4 register */
str r1, [r0]

ldr r0, =0x020C407C      /* Address of CCM_CCGR5 register */
str r1, [r0]

ldr r0, =0x020C4080      /* Address of CCM_CCGR6 register */
str r1, [r0]


/* ------------------------------------------------ */
/* 2. Configure GPIO1_IO03 MUX as GPIO function     */
/* ------------------------------------------------ */

ldr r0, =0x020E0068      /* SW_MUX_GPIO1_IO03 register address */
ldr r1, =0x5             /* Set MUX_MODE = 5 (GPIO function) */
str r1, [r0]


/* ------------------------------------------------ */
/* 3. Configure GPIO1_IO03 PAD attributes           */
/* ------------------------------------------------ */
/*
 * Bit definition:
 * bit16      : HYS      = 0  (Hysteresis disabled)
 * bit15-14   : PUS      = 00 (100K pull-down)
 * bit13      : PUE      = 0  (Keeper function)
 * bit12      : PKE      = 1  (Pull/Keeper enabled)
 * bit11      : ODE      = 0  (Open-drain disabled)
 * bit7-6     : SPEED    = 10 (100MHz speed)
 * bit5-3     : DSE      = 110 (R0/6 drive strength)
 * bit0       : SRE      = 0  (Slow slew rate)
 */

ldr r0, =0x020E02F4      /* SW_PAD_GPIO1_IO03 register address */
ldr r1, =0x10B0          /* PAD configuration value */
str r1, [r0]


/* ------------------------------------------------ */
/* 4. Set GPIO1_IO03 as output                      */
/* ------------------------------------------------ */

ldr r0, =0x0209C004      /* GPIO1_GDIR register address */
ldr r1, =0x00000008      /* Set bit3 = 1 (GPIO1_IO03 output) */
str r1, [r0]


/* ------------------------------------------------ */
/* 5. Turn ON LED                                   */
/* ------------------------------------------------ */
/*
 * Output low level on GPIO1_IO03
 * Many development boards connect LED as active-low
 */

ldr r0, =0x0209C000      /* GPIO1_DR register address */
ldr r1, =0x0             /* Output low level */
str r1, [r0]


/* ------------------------------------------------ */
/* Infinite loop                                    */
/* ------------------------------------------------ */

loop:
    b loop               /* Branch to itself (infinite loop) */
