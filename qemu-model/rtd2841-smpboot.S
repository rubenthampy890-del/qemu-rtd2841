/*
 * Linux direct-boot assistance for the RTD284x per-CPU mailbox contract.
 * This is source for rtd2841_smpboot[] in rtd2841.c, not a retail boot ROM.
 * No stack, MMU, cache, or interrupt-controller setup is needed here.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
.syntax unified
.arm
.text
.global _start
_start:
    mrc p15, 0, r0, c0, c0, 5
    and r0, r0, #0xff
    cmp r0, #1
    blo park
    cmp r0, #3
    bhi park
    movw r1, #0x500
    add r1, r1, r0, lsl #2
poll:
    ldr r2, [r1]
    cmp r2, #0
    bne released
    wfe
    b poll
released:
    dmb sy
    bx r2
park:
    wfe
    b park
