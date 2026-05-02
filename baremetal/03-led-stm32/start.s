.global _start
.global __bss_start
.global __bss_end

.section .text

_start:
    /* Switch CPU to SVC mode */
    mrs r0, cpsr
    bic r0, r0, #0x1f
    orr r0, r0, #0x13
    msr cpsr, r0

    /* Clear BSS section */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0

bss_loop:
    cmp r0, r1
    bhs bss_done
    str r2, [r0], #4
    b bss_loop

bss_done:
    /* Set stack pointer */
    ldr sp, =0x80200000

    /* Jump to C entry */
    b main