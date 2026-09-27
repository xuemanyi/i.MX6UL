/*
 * i.MX6UL 裸机启动代码。
 *
 * 建立最小 C 运行环境：切换到 SVC 模式、设置栈、清零 BSS，
 * 最后跳转到 main()。
 */

.global _start

.extern main
.extern __bss_start
.extern __bss_end

_start:
	/*
	 * 切换到 SVC 模式。CPSR[4:0] 保存当前 ARM 模式，
	 * 0x13 表示 Supervisor 模式。
	 *
	 */
	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0x13
	msr	cpsr, r0

	/*
	 * 栈指针必须指向有效 RAM，并满足 ARM 对齐要求。
	 */
	ldr	sp, =0x80200000

	/*
	 * 进入 main() 前清零 BSS，满足 C 语言静态存储期对象的初始化要求。
	 */
	ldr	r0, =__bss_start
	ldr	r1, =__bss_end
	mov	r2, #0

clear_bss:
	cmp	r0, r1
	strlo	r2, [r0], #4
	blo	clear_bss

	/*
	 * 运行环境准备完成后进入 C 入口。
	 */
	b	main

.section .note.GNU-stack,"",%progbits
