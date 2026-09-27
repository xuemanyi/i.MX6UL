.global _start

/*
 * 裸机入口负责设置 CPU 模式和栈指针，然后跳转到 C 入口。
 */

_start:
    /*
	 * 切换到 SVC 模式，确保后续初始化在特权级执行。
     */
    mrs r0, cpsr
    bic r0, r0, #0x1f
    orr r0, r0, #0x13
    msr cpsr, r0

    /*
	 * 栈指针必须位于有效 DDR 地址范围内。
     */
    ldr sp, =0x80200000

    /*
	 * 运行环境准备完成后进入 C 入口。
     */
    b main

.section .note.GNU-stack,"",%progbits
