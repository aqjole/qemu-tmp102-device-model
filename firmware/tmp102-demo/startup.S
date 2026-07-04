.syntax unified
.cpu cortex-a15
.arm

.section .text.start
.global _start
.type _start, %function

_start:
    ldr sp, =_stack_top

    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0

clear_bss:
    cmp r0, r1
    bhs call_main
    str r2, [r0], #4
    b clear_bss

call_main:
    bl main

hang:
    b hang
