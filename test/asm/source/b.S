    .syntax unified
    .arch armv7e-m

    .section .data
    .comm johan, 8
    .long 60
    .long 5
    

    .text
    .thumb
    .thumb_func
    .align 1
bar:
    ldr r1, =johan
    bl foo
    .pool
    .size   bar,.-bar

    .end
