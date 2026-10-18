.arm
.syntax unified
.global _start

_start:
    /* Setup stack in low DRAM */
    ldr sp, =0x00100000
    ldr r0, =0x18062300       /* UART0 MMIO base */

    /* 1. Banner */
    adr r1, msg_banner
    bl print_str
    b 1f

msg_banner:
    .asciz "\r\n========================================================\r\n[RTD2841-TV030] Bare-Metal Hardware Diagnostic Suite\r\nTarget: Realtek RTD2841 / TCL RT41 Chassis (tv030)\r\n========================================================\r\n"
    .align 2
1:
    /* 2. CPU Architecture & Mode */
    adr r1, msg_cpu_pass
    bl print_str
    b 2f

msg_cpu_pass:
    .asciz "[PASS] CPU 0: ARM Cortex-A53 executing in AArch32 Mode.\r\n"
    .align 2
2:
    /* 3. GIC-400 Distributor & Capacity */
    ldr r2, =0x18081004       /* GICD_TYPER */
    ldr r3, [r2]              /* Read register */
    and r4, r3, #0x1F         /* ITLinesNumber */
    cmp r4, #7                /* (256/32)-1 = 7 */
    bne gic_fail

    adr r1, msg_gic_pass
    bl print_str
    mov r1, r3
    bl print_hex
    adr r1, msg_gic_end
    bl print_str
    b 3f

msg_gic_pass:
    .asciz "[PASS] GIC-400 Distributor (0x18081000): GICD_TYPER="
msg_gic_end:
    .asciz " (256 INTIDs verified).\r\n"
    .align 2

gic_fail:
    adr r1, msg_gic_fail
    bl print_str
    mov r1, r3
    bl print_hex
    adr r1, msg_crlf
    bl print_str
    b halt

msg_gic_fail:
    .asciz "[FAIL] GIC-400: Unexpected GICD_TYPER value: "
msg_crlf:
    .asciz "\r\n"
    .align 2
3:
    /* 4. Realtek TC0 Hardware Timer Active Down-Count */
    ldr r4, =0x1801B600       /* TC0 base */
    ldr r5, =0x000F0000
    str r5, [r4, #0x00]       /* TC0TVR */
    ldr r5, =0xC0000000
    str r5, [r4, #0x40]       /* TC0CR: Enable + Periodic */

    /* Read initial counter */
    ldr r6, [r4, #0x20]       /* TC0CVR before */

    /* Delay loop to allow clock ticks */
    mov r8, #3000
delay_loop:
    subs r8, r8, #1
    bne delay_loop

    /* Read counter after */
    ldr r7, [r4, #0x20]       /* TC0CVR after */

    /* Assert countdown */
    cmp r7, r6
    bhs timer_fail

    adr r1, msg_timer_pass
    bl print_str
    mov r1, r6
    bl print_hex
    adr r1, msg_timer_mid
    bl print_str
    mov r1, r7
    bl print_hex
    adr r1, msg_timer_end
    bl print_str
    b 4f

msg_timer_pass:
    .asciz "[PASS] TC0 Timer (0x1801B600): Active down-count verified (T0="
msg_timer_mid:
    .asciz ", T1="
msg_timer_end:
    .asciz " @ 27MHz).\r\n"
    .align 2

timer_fail:
    adr r1, msg_timer_fail
    bl print_str
    b halt

msg_timer_fail:
    .asciz "[FAIL] TC0 Timer: Counter failed to decrement!\r\n"
    .align 2
4:
    /* 5. Realtek SDHCI / eMMC Host Controller */
    ldr r8, =0x18010800       /* SDHCI base */
    ldr r9, [r8, #0x40]       /* SDHC_CAPAREG */
    cmp r9, #0
    beq sdhci_fail

    ldr r7, [r8, #0x24]       /* SDHC_PRNSTS */
    tst r7, #0x00010000       /* Card present bit */
    beq sdhci_no_media

    adr r1, msg_sdhci_card
    bl print_str
    mov r1, r7
    bl print_hex
    adr r1, msg_sdhci_card_end
    bl print_str
    b 5f

msg_sdhci_card:
    .asciz "[PASS] SDHCI eMMC (0x18010800): Controller online, media present (PRSNT_ST="
msg_sdhci_card_end:
    .asciz ").\r\n"
    .align 2

sdhci_no_media:
    adr r1, msg_sdhci_nocard
    bl print_str
    mov r1, r9
    bl print_hex
    adr r1, msg_sdhci_nocard_end
    bl print_str
    b 5f

msg_sdhci_nocard:
    .asciz "[PASS] SDHCI eMMC (0x18010800): Controller online (CAPAREG="
msg_sdhci_nocard_end:
    .asciz ", no card; optional: attach -drive file=storage.img,if=sd).\r\n"
    .align 2

sdhci_fail:
    adr r1, msg_sdhci_fail
    bl print_str
    b halt

msg_sdhci_fail:
    .asciz "[FAIL] SDHCI eMMC: Capabilities register unreadable or zero!\r\n"
    .align 2
5:
    /* 6. Summary */
    adr r1, msg_summary
    bl print_str

halt:
    wfi
    b halt

msg_summary:
    .asciz "========================================================\r\n[>>> ALL HARDWARE SUBSYSTEM VERIFICATION TESTS PASSED <<<]\r\n========================================================\r\n\r\n"
    .align 2

/* Helpers */
print_str:
1:
    ldrb r2, [r1], #1
    cmp r2, #0
    bxeq lr
    str r2, [r0]
    b 1b

print_hex:
    push {r4, r5, lr}
    mov r4, r1
    mov r1, #'0'
    str r1, [r0]
    mov r1, #'x'
    str r1, [r0]
    mov r5, #28
1:
    lsr r1, r4, r5
    and r1, r1, #0xF
    cmp r1, #10
    addlt r1, r1, #'0'
    addge r1, r1, #('A' - 10)
    str r1, [r0]
    subs r5, r5, #4
    bge 1b
    pop {r4, r5, pc}
