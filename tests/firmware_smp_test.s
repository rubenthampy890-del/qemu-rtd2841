/* Exercise the direct-kernel mailbox monitor without proprietary firmware. */
.syntax unified
.arm
.text
.global _start
_start:
    /* Only CPU0 may enter here. Other CPUs must still be in their monitors. */
    mrc p15, 0, r0, c0, c0, 5
    and r0, r0, #0xff
    cmp r0, #0
    bne fail
    ldr sp, =0x100000
    ldr r4, =0x500
    mov r5, #1
check_reset:
    ldr r0, [r4, r5, lsl #2]
    cmp r0, #0
    bne fail
    add r5, r5, #1
    cmp r5, #4
    blo check_reset
    ldr r1, =msg_reset
    bl print
    mov r5, #1
release:
    /* All not-yet-released CPUs must still have zero result slots. */
    ldr r6, =results
    mov r7, r5
check_parked:
    ldr r0, [r6, r7, lsl #2]
    cmp r0, #0
    bne fail
    add r7, r7, #1
    cmp r7, #4
    blo check_parked
    ldr r0, =secondary
    str r0, [r4, r5, lsl #2]
    dsb sy
    sev
wait_result:
    ldr r0, [r6, r5, lsl #2]
    cmp r0, #0
    bne check_result
    wfe
    b wait_result
check_result:
    dmb sy
    cmp r0, r5
    bne fail
    /* Check that a release did not start any other waiting CPU. */
    add r7, r5, #1
check_others:
    cmp r7, #4
    bhs next_cpu
    ldr r0, [r6, r7, lsl #2]
    cmp r0, #0
    bne fail
    add r7, r7, #1
    b check_others
next_cpu:
    add r5, r5, #1
    cmp r5, #4
    blo release
    ldr r1, =msg_pass
    bl print
park:
    wfe
    b park
secondary:
    mrc p15, 0, r0, c0, c0, 5
    and r0, r0, #0xff
    cmp r0, #1
    blo fail
    cmp r0, #3
    bhi fail
    ldr r1, =results
    str r0, [r1, r0, lsl #2]
    dsb sy
    sev
    b park
fail:
    ldr r1, =msg_fail
    bl print
    b park
print:
    ldr r2, =0x18062300
1:
    ldrb r0, [r1], #1
    cmp r0, #0
    bxeq lr
2:
    ldr r3, [r2, #0x14]
    tst r3, #0x20
    beq 2b
    str r0, [r2]
    b 1b
.align 2
results:
    .word 0, 0, 0, 0
msg_reset:
    .asciz "[SMP] All secondary mailboxes clear on reset\r\n"
msg_pass:
    .asciz "[SMP] Separate CPU1 CPU2 CPU3 releases verified\r\n"
msg_fail:
    .asciz "[FAIL] SMP mailbox or CPU identity mismatch\r\n"
