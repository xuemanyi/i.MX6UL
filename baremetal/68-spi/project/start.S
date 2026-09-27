.global _start

/*
 * 异常向量表。
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
 * 复位入口。
 *
 * 禁止 IRQ 并关闭 MMU、缓存和分支预测，分别初始化 IRQ、SYS、SVC
 * 模式的栈指针，然后进入 C 入口。
 */
Reset_Handler:
	cpsid i

	/*
	 * 在进入 C 代码前关闭 I-cache、D-cache、对齐检查、分支预测和 MMU。
	 */
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1 << 12)
	bic r0, r0, #(1 << 2)
	bic r0, r0, #0x2
	bic r0, r0, #(1 << 11)
	bic r0, r0, #0x1
	mcr p15, 0, r0, c1, c0, 0

	/*
	 * 设置 IRQ 模式栈。
	 */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x12
	msr cpsr, r0
	ldr sp, =0x80600000

	/*
	 * 设置 SYS 模式栈。
	 */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x1f
	msr cpsr, r0
	ldr sp, =0x80400000

	/*
	 * 设置 SVC 模式栈。
	 */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x13
	msr cpsr, r0
	ldr sp, =0x80200000

	cpsie i

	b main

/*
 * 默认异常处理函数通过自循环保留现场，便于调试定位。
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
 * IRQ 异常处理入口。
 *
 * 保存 IRQ 上下文，从 GICC_IAR 读取中断确认值，切换到 SVC 模式调用
 * C 层分发函数，随后写入 GICC_EOIR 并恢复被中断现场。
 */
IRQ_Handler:
	push {lr}
	push {r0-r3, r12}

	mrs r0, spsr
	push {r0}

	/*
	 * 从 CP15 读取私有外设基地址并定位 GIC CPU 接口。
	 */
	mrc p15, 4, r1, c15, c0, 0
	add r1, r1, #0x2000

	/*
	 * 读取 GICC_IAR。r0 保存中断确认值，并作为第一个参数传递给
	 * system_irqhandler()。
	 */
	ldr r0, [r1, #0x0c]
	push {r0, r1}

	/*
	 * 在 SVC 模式下调用 C 层 IRQ 分发函数。
	 */
	cps #0x13
	push {lr}

	ldr r2, =system_irqhandler
	blx r2

	pop {lr}

	/*
	 * 返回 IRQ 模式并通知 GIC 中断处理结束。
	 */
	cps #0x12
	pop {r0, r1}
	str r0, [r1, #0x10]

	pop {r0}
	msr spsr_cxsf, r0

	pop {r0-r3, r12}
	pop {lr}

	/*
	 * 从 IRQ 返回到被中断位置。
	 */
	subs pc, lr, #4

FIQ_Handler:
	ldr r0, =FIQ_Handler
	bx r0

/* 明确声明不需要可执行栈。 */
.section .note.GNU-stack,"",%progbits
