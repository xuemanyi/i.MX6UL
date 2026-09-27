.global _start
.global __bss_start
.global __bss_end

.section .text

_start:
    /* 切换到 SVC 模式，确保后续初始化在特权级执行。 */
    mrs r0, cpsr
    bic r0, r0, #0x1f
    orr r0, r0, #0x13
    msr cpsr, r0

    /* C 语言未显式初始化的全局变量依赖清零后的 BSS。 */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0

bss_loop:
    cmp r0, r1
    bhs bss_done
    str r2, [r0], #4
    b bss_loop

bss_done:
    /* 栈位于有效 DDR 地址范围内，并保持向下增长。 */
    ldr sp, =0x80200000

    /* 运行环境准备完成后进入 C 入口。 */
    b main

.section .note.GNU-stack,"",%progbits
