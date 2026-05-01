.global _start

/*
 * Bare-metal entry point.
 * This file sets up the CPU mode and stack pointer,
 * then jumps to the C main function.
 */

_start:
    /*
     * Switch CPU to SVC mode.
     * SVC mode is commonly used for bare-metal startup code.
     */
    mrs r0, cpsr
    bic r0, r0, #0x1f
    orr r0, r0, #0x13
    msr cpsr, r0

    /*
     * Set stack pointer.
     * The address must be located in valid DDR memory.
     */
    ldr sp, =0x80200000

    /*
     * Jump to C entry function.
     */
    b main