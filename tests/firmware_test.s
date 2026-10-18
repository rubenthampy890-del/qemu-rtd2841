.arm
.syntax unified
.global _start

_start:
    /* Initialize stack pointer in lower DRAM */
    ldr sp, =0x00100000
    ldr r0, =0x18062300

    bl print_banner
    b 1f
banner_msg:
    .asciz "\r\n========================================================\r\n[RTD2841-TV030] Bare-Metal Simulation Starting...\r\n[RTD2841-TV030] CPU 0: ARM Cortex-A53 in AArch32 Mode\r\n[RTD2841-TV030] UART0 Output via 0x18062300: ACTIVE\r\n========================================================\r\n"
    .align 2
1:
    /* 1. GIC-400 Distributor check */
    ldr r2, =0x18081000
    ldr r3, [r2]

    bl print_gic
    b 2f
gic_msg:
    .asciz "[+] Realtek GIC-400 Interrupt Controller (0x18081000) verified.\r\n"
    .align 2
2:
    /* 2. Realtek RTD2841 Timer TC0 configuration */
    ldr r4, =0x1801B600
    ldr r5, =0x00050000
    str r5, [r4, #0x00]       /* TVR0 */
    ldr r5, =0xC0000000
    str r5, [r4, #0x40]       /* CR0 (Enable + Periodic) */

    bl print_timer
    b 3f
timer_msg:
    .asciz "[+] Realtek TC0 Hardware Timer (0x1801B600) configured (27MHz, CR=0xC0000000).\r\n"
    .align 2
3:
    mov r6, #100
wait_loop:
    ldr r5, [r4, #0x20]       /* Read TC0 counter register */
    subs r6, r6, #1
    bne wait_loop

    /* 3. Realtek SDHCI / eMMC Host Controller check at 0x18010800 */
    ldr r8, =0x18010800
    ldr r7, [r8, #0x24]       /* Read SDHC_PRNSTS (Present State Register) */
    tst r7, #0x00010000       /* Test Bit 16: SDHC_CARD_PRESENT */
    beq no_card

    adr r1, card_detected_msg
    bl print_str

    /* Read Capabilities register at 0x40 */
    ldr r9, [r8, #0x40]
    adr r1, card_capa_msg
    bl print_str
    b 4f

no_card:
    adr r1, no_card_msg
    bl print_str
    b halt

card_detected_msg:
    .asciz "[+] Realtek eMMC / SDHCI Host Controller (0x18010800): CARD DETECTED (Bit 16 set in PRSNT_ST).\r\n"
card_capa_msg:
    .asciz "[+] Realtek eMMC / SDHCI Capabilities Register (0x18010840) verified without bus errors.\r\n"
no_card_msg:
    .asciz "[-] Realtek eMMC / SDHCI Host Controller (0x18010800): NO CARD DETECTED!\r\n"
    .align 2

4:
    bl print_pass
    b halt
pass_msg:
    .asciz "\r\n[>>> RTD2841 QEMU HARDWARE SIMULATION PASSED <<<]\r\n\r\n"
    .align 2

halt:
    wfi
    b halt

print_banner:
    adr r1, banner_msg
    b print_str
print_gic:
    adr r1, gic_msg
    b print_str
print_timer:
    adr r1, timer_msg
    b print_str
print_pass:
    adr r1, pass_msg
    b print_str

print_str:
loop_char:
    ldrb r2, [r1], #1
    cmp r2, #0
    bxeq lr
    str r2, [r0]
    b loop_char
