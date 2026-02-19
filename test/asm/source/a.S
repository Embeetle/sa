    .syntax unified
    .arch armv7e-m
    .johan // intentional error

    .section .bss
    .comm johan, 8
    .long 0
    
    .text
    .thumb
    .thumb_func
    .align 1
    .globl foo
    .type foo, %function
foo:
SystemInit: 

    subs r3, r3, r2
    ble .L_loop1_done
.L_loop1:
    subs r3, r3, #4
    ldr r0, [r1,r3]
    str r0, [r2,r3]
    bgt .L_loop1
.L_loop1_done:
    ldr r1, =__bss_start__
    ldr r2, =__bss_end__
    movs r0, 0
    subs r2, r2, r1
    ble .L_loop3_done
.L_loop3:
    subs r2, r2, #4
    str r0, [r1, r2]
    bgt .L_loop3
.L_loop3_done:
    bl SystemInit
    .pool
    .size   foo,.-foo

    .section ".text"
    .weak   NMI_Handler
    .type   NMI_Handler, %function
NMI_Handler:
    b       .
    .size   NMI_Handler, . - NMI_Handler
    .weak   HardFault_Handler
    .type   HardFault_Handler, %function
HardFault_Handler:
    b       .
    .size   HardFault_Handler, . - HardFault_Handler
  .end
