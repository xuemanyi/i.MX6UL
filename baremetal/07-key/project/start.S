/*
 * i.MX6UL bare-metal startup code.
 *
 * This file prepares the minimal C runtime environment:
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
	 */
	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0x13
	msr	cpsr, r0

	/*
	 * Set stack pointer.
	 *
	 * DDR range is usually 0x80000000 ~ 0x9fffffff.
	 * Stack grows downward, and the address must be 4-byte aligned.
	 */
	ldr	sp, =0x80200000

	/*
	 * Clear BSS section before entering C code.
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
/* Fix the warning belowing. 
 * arm-linux-gnueabihf-ld: warning: obj/start.o: missing .note.GNU-stack section implies executable stack
 * arm-linux-gnueabihf-ld: NOTE: This behaviour is deprecated and will be removed in a future version of the linker
 */
.section .note.GNU-stack,"",%progbits