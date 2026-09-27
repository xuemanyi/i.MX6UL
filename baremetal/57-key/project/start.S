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
	 * 切换到 SVC 模式，确保后续初始化在特权级执行。
	 */
	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0x13
	msr	cpsr, r0

	/*
	 * 栈放在 DDR 有效范围内。ARM 栈向下增长，并保持 4 字节对齐。
	 */
	ldr	sp, =0x80200000

	/*
	 * 进入 C 代码前清零 BSS。
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
/* 明确声明不需要可执行栈，避免链接器按兼容行为推断为可执行栈。 */
.section .note.GNU-stack,"",%progbits
