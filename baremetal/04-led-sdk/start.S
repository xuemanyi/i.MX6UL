/*
 * i.MX6UL bare-metal startup code.
 *
 * This file prepares a minimal C runtime environment:
 * - Switch CPU to SVC mode
 * - Set up stack pointer
 * - Clear BSS section
 * - Jump to main()
 */

.global _start

.extern main
.extern __bss_start
.extern __bss_end

_start:
	/*
	 * Switch to SVC mode.
	 *
	 * CPSR[4:0] contains the current ARM mode.
	 * 0x13 means Supervisor mode.
	 */
	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0x13
	msr	cpsr, r0

	/*
	 * Set stack pointer.
	 *
	 * The stack address must point to valid RAM.
	 */
	ldr	sp, =0x80200000

	/*
	 * Clear BSS section.
	 *
	 * C global/static variables without explicit initialization
	 * must be zero before entering main().
	 */
	ldr	r0, =__bss_start
	ldr	r1, =__bss_end
	mov	r2, #0

clear_bss:
	cmp	r0, r1
	strlo	r2, [r0], #4
	blo	clear_bss

	/*
	 * Jump to C entry.
	 */
	b	main
