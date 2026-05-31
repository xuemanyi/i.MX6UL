.global _start

/*
 * Exception vector table.
 */
_start:
	ldr pc, =Reset_Handler
	ldr pc, =Undefined_Handler
	ldr pc, =SVC_Handler
	ldr pc, =PrefAbort_Handler
	ldr pc, =DataAbort_Handler
	ldr pc, =NotUsed_Handler
	ldr pc, =IRQ_Handler
	ldr pc, =FIQ_Handler

/*
 * Reset entry.
 *
 * Disable IRQ, disable MMU/cache/branch prediction, initialize stack
 * pointers for IRQ/SYS/SVC modes, then jump to C entry.
 */
Reset_Handler:
	cpsid i

	/*
	 * Disable I-cache, D-cache, alignment check, branch prediction
	 * and MMU before entering C code.
	 */
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1 << 12)
	bic r0, r0, #(1 << 2)
	bic r0, r0, #0x2
	bic r0, r0, #(1 << 11)
	bic r0, r0, #0x1
	mcr p15, 0, r0, c1, c0, 0

	/*
	 * Set IRQ mode stack.
	 */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x12
	msr cpsr, r0
	ldr sp, =0x80600000

	/*
	 * Set SYS mode stack.
	 */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x1f
	msr cpsr, r0
	ldr sp, =0x80400000

	/*
	 * Set SVC mode stack.
	 */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x13
	msr cpsr, r0
	ldr sp, =0x80200000

	cpsie i

	b main

/*
 * Default exception handlers.
 */
Undefined_Handler:
	ldr r0, =Undefined_Handler
	bx r0

SVC_Handler:
	ldr r0, =SVC_Handler
	bx r0

PrefAbort_Handler:
	ldr r0, =PrefAbort_Handler
	bx r0

DataAbort_Handler:
	ldr r0, =DataAbort_Handler
	bx r0

NotUsed_Handler:
	ldr r0, =NotUsed_Handler
	bx r0

/*
 * IRQ exception handler.
 *
 * Save IRQ context, read interrupt ID from GICC_IAR, switch to SVC mode
 * to run the C-level IRQ dispatcher, then write GICC_EOIR and restore
 * the interrupted context.
 */
IRQ_Handler:
	push {lr}
	push {r0-r3, r12}

	mrs r0, spsr
	push {r0}

	/*
	 * Read private peripheral base address from CP15 and locate
	 * the GIC CPU interface.
	 */
	mrc p15, 4, r1, c15, c0, 0
	add r1, r1, #0x2000

	/*
	 * Read GICC_IAR.
	 *
	 * r0 contains the interrupt acknowledge value and will be passed
	 * as the first argument to system_irqhandler().
	 */
	ldr r0, [r1, #0x0c]
	push {r0, r1}

	/*
	 * Run the C IRQ dispatcher in SVC mode.
	 */
	cps #0x13
	push {lr}

	ldr r2, =system_irqhandler
	blx r2

	pop {lr}

	/*
	 * Return to IRQ mode and signal end of interrupt.
	 */
	cps #0x12
	pop {r0, r1}
	str r0, [r1, #0x10]

	pop {r0}
	msr spsr_cxsf, r0

	pop {r0-r3, r12}
	pop {lr}

	/*
	 * Return from IRQ.
	 */
	subs pc, lr, #4

FIQ_Handler:
	ldr r0, =FIQ_Handler
	bx r0