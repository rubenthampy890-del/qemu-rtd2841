/*
 * firmware_fb_test.s - AArch32 Bare-Metal Framebuffer Smoke-Test Payload
 * Target: Realtek RTD2841 / TCL RT41 Chassis (tv030 board profile)
 *
 * Hardware Specification:
 * - CPU: ARM Cortex-A53 in AArch32 mode (core 0)
 * - DRAM: 1024 MB starting at 0x00000000
 * - Stack: DRAM 0x10000000
 * - Framebuffer: 0x3E000000 (8 MB carveout, 1920x1080 @ 32bpp ARGB8888/XRGB8888)
 * - UART0: 0x18062300 (16550A compatible, 32-bit registers, 4-byte stride)
 * - TC0 Timer: 0x1801B600 (Realtek hardware countdown timer)
 *
 * Sequence:
 * 1. Low vector table setup at 0x00000000 with fault traps.
 * 2. Step a: Initialize stack pointer sp to 0x10000000 in DRAM.
 * 3. Step b: Configure 16550A UART0 at 0x18062300 and print start banner.
 * 4. Step c: Render 8 vertical SMPTE color bars across 1920x1080 @ 32bpp (0x3E000000 - 0x3E7E9000).
 * 5. Step d: Issue DSB and ISB memory barrier instructions.
 * 6. Verify framebuffer memory readback matches rendered pattern.
 * 7. Step e: Print completion telemetry to UART0.
 * 8. Step f: Configure TC0 hardware timer and enter heartbeat verification loop.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

.arm
.syntax unified
.cpu cortex-a53
.fpu neon-vfpv4

.section .text.startup, "ax", %progbits
.global _start

_start:
    /* Low Exception Vector Table (mapped at 0x00000000) */
    b reset_entry             /* 0x00: Reset Vector */
    b handle_undef            /* 0x04: Undefined Instruction */
    b handle_svc              /* 0x08: Supervisor Call (SVC) */
    b handle_pabort           /* 0x0C: Prefetch Abort */
    b handle_dabort           /* 0x10: Data Abort */
    b .                       /* 0x14: Reserved / Hypervisor Trap */
    b handle_irq              /* 0x18: IRQ */
    b handle_fiq              /* 0x1C: FIQ */

reset_entry:
    /* ------------------------------------------------------------------
     * Step a: Initialize Stack Pointer in valid DRAM
     * ------------------------------------------------------------------ */
    ldr sp, =0x10000000

    /* ------------------------------------------------------------------
     * Step b: Initialize UART0 (0x18062300) and print start banner
     * ------------------------------------------------------------------ */
    ldr r0, =0x18062300
    bl uart_init

    ldr r0, =0x18062300
    adr r1, msg_start
    bl uart_puts

    /* ------------------------------------------------------------------
     * Step c: Render 8 vertical color bars across 1920x1080 @ 32bpp
     * Framebuffer memory span: 0x3E000000 to 0x3E7E9000 (8,294,400 bytes)
     * Resolution: 1920 width x 1080 height
     * Stride: 7680 bytes (1920 * 4)
     * Bar width: 240 columns per bar (1920 / 8)
     * Format: ARGB8888/XRGB8888 (White, Yellow, Cyan, Green, Magenta, Red, Blue, Black)
     * ------------------------------------------------------------------ */
    ldr r3, =0x3E000000       /* Framebuffer start address */
    ldr r4, =1080             /* 1080 rows */

row_loop:
    adr r5, color_bars_table  /* Pointer to 8 color values */
    mov r6, #8                /* 8 color bars */

bar_loop:
    ldr r7, [r5], #4          /* Current bar ARGB8888 color value */
    mov r8, #240              /* 240 pixels per bar */

pixel_loop:
    str r7, [r3], #4          /* Write 32-bit pixel and advance */
    subs r8, r8, #1
    bne pixel_loop

    subs r6, r6, #1
    bne bar_loop

    subs r4, r4, #1
    bne row_loop

    /* ------------------------------------------------------------------
     * Step d: Memory barrier instructions to ensure DRAM write visibility
     * ------------------------------------------------------------------ */
    dsb sy
    isb sy

    /* ------------------------------------------------------------------
     * Hardware Verification: Read back rendered framebuffer to confirm
     * physical DRAM integrity and write-retention across all 8 bars.
     * ------------------------------------------------------------------ */
    ldr r3, =0x3E000000       /* Framebuffer start address */
    ldr r4, =1080             /* 1080 rows */

verify_row_loop:
    adr r5, color_bars_table  /* Pointer to 8 expected color values */
    mov r6, #8                /* 8 color bars */

verify_bar_loop:
    ldr r7, [r5], #4          /* Expected bar color value */
    mov r8, #240              /* 240 pixels per bar */

verify_pixel_loop:
    ldr r2, [r3], #4          /* Read back 32-bit pixel */
    cmp r2, r7                /* Compare with expected value */
    bne fb_verify_failed
    subs r8, r8, #1
    bne verify_pixel_loop

    subs r6, r6, #1
    bne verify_bar_loop

    subs r4, r4, #1
    bne verify_row_loop

    /* ------------------------------------------------------------------
     * Step e: Print pattern rendered telemetry message over UART0
     * ------------------------------------------------------------------ */
    ldr r0, =0x18062300
    adr r1, msg_rendered
    bl uart_puts

    /* ------------------------------------------------------------------
     * Step f: Configure Realtek TC0 hardware timer and loop decrementing
     * Base: 0x1801B600
     * TVR0 (0x00): Target / reload value (0x00100000)
     * CVR0 (0x20): Current running decrementing counter
     * CR0  (0x40): Control register (bit 31: Enable, bit 30: Periodic)
     * ------------------------------------------------------------------ */
    ldr r4, =0x1801B600
    ldr r5, =0x00100000
    str r5, [r4, #0x00]       /* Set TVR0 reload value */
    ldr r5, =0xC0000000
    str r5, [r4, #0x40]       /* CR0: Enable (0x80000000) + Periodic (0x40000000) */

    /* Read initial counter value */
    ldr r6, [r4, #0x20]
    mov r7, #0                /* r7 = confirmed heartbeat decrement count */
    ldr r8, =1000000          /* r8 = timeout watchdog counter */

idle_loop:
    /* Read current counter and verify active hardware decrement */
    ldr r5, [r4, #0x20]
    cmp r5, r6
    bne 1f

    /* Counter unchanged; decrement timeout watchdog */
    subs r8, r8, #1
    beq timer_heartbeat_failed
    b idle_loop

1:
    /* Heartbeat confirmed: counter decremented */
    mov r6, r5                /* Update previous counter value */
    add r7, r7, #1            /* Track confirmed heartbeat decrement */
    ldr r8, =1000000          /* Reset timeout watchdog */
    b idle_loop

/* ======================================================================
 * Error Handlers
 * ====================================================================== */
fb_verify_failed:
    ldr r0, =0x18062300
    adr r1, msg_fb_fail
    bl uart_puts
    b halt

timer_heartbeat_failed:
    ldr r0, =0x18062300
    adr r1, msg_timer_fail
    bl uart_puts
    b halt

handle_undef:
    ldr r0, =0x18062300
    adr r1, msg_err_undef
    bl uart_puts
    b halt

handle_svc:
    ldr r0, =0x18062300
    adr r1, msg_err_svc
    bl uart_puts
    b halt

handle_pabort:
    ldr r0, =0x18062300
    adr r1, msg_err_pabort
    bl uart_puts
    b halt

handle_dabort:
    ldr r0, =0x18062300
    adr r1, msg_err_dabort
    bl uart_puts
    b halt

handle_irq:
handle_fiq:
    b halt

halt:
    wfi
    b halt

/* ======================================================================
 * UART0 Subroutines (16550A standard, 32-bit stride / shift=2)
 * ====================================================================== */
uart_init:
    /* r0 = UART0 Base (0x18062300) */
    mov r1, #0x00
    str r1, [r0, #0x04]       /* IER: Disable interrupts */

    mov r1, #0x80
    str r1, [r0, #0x0C]       /* LCR: Enable DLAB (Divisor Latch Access) */

    mov r1, #15
    str r1, [r0, #0x00]       /* DLL: 27MHz / (16 * 115200) = 14.648 -> divisor 15 */
    mov r1, #0x00
    str r1, [r0, #0x04]       /* DLH: 0 */

    mov r1, #0x03
    str r1, [r0, #0x0C]       /* LCR: 8 data bits, 1 stop bit, no parity (8N1), DLAB=0 */

    mov r1, #0x07
    str r1, [r0, #0x08]       /* FCR: Enable FIFO, clear RX/TX FIFOs */

    mov r1, #0x03
    str r1, [r0, #0x10]       /* MCR: Assert DTR and RTS */
    bx lr

uart_putc:
    /* r0 = UART0 Base, r1 = character byte */
1:
    ldr r2, [r0, #0x14]       /* LSR: Line Status Register */
    tst r2, #0x20             /* THRE: Transmitter Holding Register Empty (bit 5) */
    beq 1b
    str r1, [r0, #0x00]       /* THR: Transmit Data */
    bx lr

uart_puts:
    /* r0 = UART0 Base, r1 = string pointer */
    push {r4, lr}
    mov r4, r1
2:
    ldrb r1, [r4], #1
    cmp r1, #0
    popeq {r4, pc}
    bl uart_putc
    b 2b

/* ======================================================================
 * Data Tables & String Literals
 * ====================================================================== */
.align 2
color_bars_table:
    .word 0x00FFFFFF          /* 0: White   (R=255, G=255, B=255) */
    .word 0x00FFFF00          /* 1: Yellow  (R=255, G=255, B=0)   */
    .word 0x0000FFFF          /* 2: Cyan    (R=0,   G=255, B=255) */
    .word 0x0000FF00          /* 3: Green   (R=0,   G=255, B=0)   */
    .word 0x00FF00FF          /* 4: Magenta (R=255, G=0,   B=255) */
    .word 0x00FF0000          /* 5: Red     (R=255, G=0,   B=0)   */
    .word 0x000000FF          /* 6: Blue    (R=0,   G=0,   B=255) */
    .word 0x00000000          /* 7: Black   (R=0,   G=0,   B=0)   */

msg_start:
    .asciz "[+] RTD2841 Framebuffer Test Starting...\r\n"

.align 2
msg_rendered:
    .asciz "[+] Framebuffer test pattern rendered (1920x1080 @ 32bpp). Entering idle loop.\r\n"

.align 2
msg_fb_fail:
    .asciz "[-] ERROR: Framebuffer verification failed!\r\n"

.align 2
msg_timer_fail:
    .asciz "[-] ERROR: TC0 timer heartbeat failure!\r\n"

.align 2
msg_err_undef:
    .asciz "[-] FATAL: Undefined instruction exception!\r\n"

.align 2
msg_err_svc:
    .asciz "[-] FATAL: Unhandled SVC exception!\r\n"

.align 2
msg_err_pabort:
    .asciz "[-] FATAL: Prefetch abort exception!\r\n"

.align 2
msg_err_dabort:
    .asciz "[-] FATAL: Data abort exception!\r\n"

.align 2
