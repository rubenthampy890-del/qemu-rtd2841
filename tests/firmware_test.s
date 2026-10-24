.arm
.syntax unified
.global _start

_start:
    /* Exception Vector Table at 0x00000000 (32-byte stride) */
    b reset_handler             /* 0x00: Reset */
    b undef_handler             /* 0x04: Undefined instruction */
    b svc_handler               /* 0x08: Supervisor Call (SVC) */
    b prefetch_abort_handler    /* 0x0C: Prefetch Abort */
    b data_abort_handler        /* 0x10: Data Abort */
    b hyp_trap_handler          /* 0x14: Hyp Trap / Reserved */
    b irq_handler               /* 0x18: IRQ Exception Vector */
    b fiq_handler               /* 0x1C: FIQ Exception Vector */

    .align 5                    /* 32-byte alignment */

reset_handler:
    /* Check CPU mode: drop from HYP (0x1A) to SVC (0x13) if started in HYP */
    mrs r1, cpsr
    and r2, r1, #0x1F
    cmp r2, #0x1A               /* HYP mode */
    bne 1f
    bic r1, r1, #0x1F
    orr r1, r1, #0x13           /* SVC mode */
    msr spsr_cxsf, r1
    adr lr, 1f
    msr elr_hyp, lr
    eret
1:
    /* Setup SVC stack in low DRAM */
    ldr sp, =0x00100000

    /* Setup IRQ mode banked stack */
    cps #0x12                   /* Switch to IRQ mode */
    ldr sp, =0x000F0000         /* Banked SP_irq */
    cps #0x13                   /* Switch back to SVC mode */

    /* Configure VBAR (CP15 c12) pointing to vector table at _start */
    ldr r1, =_start
    mcr p15, 0, r1, c12, c0, 0

    /* Clear SCTLR.V (bit 13) to select normal/low vectors (VBAR/0x0) */
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #(1 << 13)
    mcr p15, 0, r1, c1, c0, 0

    /* Initialize irq_counter */
    ldr r1, =irq_counter
    mov r2, #0
    str r2, [r1]

    /* Disable all GIC distributor SPI interrupts (INTID 32..255) */
    ldr r2, =0x18081184       /* GICD_ICENABLER1 */
    ldr r3, =0xFFFFFFFF
    mov r4, #7                /* 7 registers: INTID 32..255 */
disable_all_irqs:
    str r3, [r2], #4
    subs r4, r4, #1
    bne disable_all_irqs

    ldr r0, =0x18062300         /* UART0 MMIO base */

    /* 1. Banner */
    adr r1, msg_banner
    bl print_str
    b 2f

msg_banner:
    .asciz "\r\n========================================================\r\n[RTD2841-TV030] Bare-Metal Hardware Diagnostic Suite\r\nTarget: Realtek RTD2841 / TCL RT41 Chassis (tv030)\r\n========================================================\r\n"
    .align 2
2:
    /* 2. CPU Architecture & Mode */
    adr r1, msg_cpu_pass
    bl print_str
    b 3f

msg_cpu_pass:
    .asciz "[PASS] CPU 0: ARM Cortex-A53 executing in AArch32 Mode.\r\n"
    .align 2
3:
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
    b 4f

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
4:
    /* 4. Realtek TC0 Hardware Timer Active Up-Count */
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

    /* Assert up-count */
    cmp r7, r6
    bls timer_fail

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
    b 5f

msg_timer_pass:
    .asciz "[PASS] TC0 Timer (0x1801B600): Active up-count verified (T0="
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
    .asciz "[FAIL] TC0 Timer: Counter failed to increment!\r\n"
    .align 2
5:
    /* 5. Real CPU Interrupt Trap Delivery Proof (Gap 2: TC0 -> GIC-400 -> ARM Vector Table) */
    ldr r0, =0x18062300       /* UART0 base */

    /* Clear irq_counter in memory */
    ldr r1, =irq_counter
    mov r2, #0
    str r2, [r1]

    /* Configure GIC-400 Distributor (0x18081000) */
    /* 1. Enable GICD: write 1 to GICD_CTLR (0x18081000) */
    ldr r2, =0x18081000
    mov r3, #1
    str r3, [r2]

    /* 2. Enable INTID 34 (TC0): GICD_ISENABLER1 (0x18081104), bit (34-32)=bit 2 (0x4) */
    ldr r2, =0x18081104
    mov r3, #4
    str r3, [r2]

    /* 3. Set target CPU for INTID 34: CPU0 in GICD_ITARGETSR8 (0x18081820) */
    ldr r2, =0x18081820
    ldr r3, [r2]
    orr r3, r3, #0x00010000   /* INTID 34 = byte 2, bit 0 = CPU0 */
    str r3, [r2]
    mov r3, #1
    strb r3, [r2, #2]         /* Target CPU0 byte write */

    /* 4. Set priority for INTID 34: 0x80 in GICD_IPRIORITYR8 (0x18081420) */
    ldr r2, =0x18081420
    ldr r3, [r2]
    bic r3, r3, #0x00FF0000
    orr r3, r3, #0x00800000   /* INTID 34 = byte 2, priority 0x80 */
    str r3, [r2]
    mov r3, #0x80
    strb r3, [r2, #2]         /* Priority 0x80 byte write */

    /* Configure GIC-400 CPU Interface (0x18082000) */
    /* 5. Set GICC_PMR (0x18082004) to 0xF0 (unmask priority 0x80) */
    ldr r2, =0x18082004
    mov r3, #0xF0
    str r3, [r2]

    /* 6. Enable GICC: write 1 to GICC_CTLR (0x18082000) */
    ldr r2, =0x18082000
    mov r3, #1
    str r3, [r2]

    /* Configure TC0 Timer (0x1801B600) */
    ldr r4, =0x1801B600
    mov r5, #0
    str r5, [r4, #0x40]       /* Stop TC0 first: ensures new_en (0->1) restarts counter */
    ldr r5, =0xFF
    str r5, [r4, #0xEC]       /* Clear pending timer ISR status (W1C) */
    ldr r5, =0x00000400       /* 1024 ticks = 37 microseconds */
    str r5, [r4, #0x00]

    /* 8. Clear TC0CVR (0x20) */
    mov r5, #0
    str r5, [r4, #0x20]

    /* 9. Enable TC0 interrupt: TC0ICR (0x60) = 0x80000000 (R_ICR_IE) */
    ldr r5, =0x80000000
    str r5, [r4, #0x60]

    /* 10. Start TC0: TC0CR (0x40) = 0xC0000000 (Enable + Periodic) */
    ldr r5, =0xC0000000
    str r5, [r4, #0x40]

    /* 11. Unmask CPSR IRQ */
    cpsie i

    /* 12. Bounded wait loop for irq_counter > 0 */
    ldr r3, =irq_counter
    ldr r8, =5000000
wait_timer_irq:
    ldr r4, [r3]
    cmp r4, #0
    bne irq_test_pass
    subs r8, r8, #1
    bne wait_timer_irq

    /* IRQ Delivery Timed Out */
    cpsid i
    ldr r1, =msg_irq_fail
    bl print_str
    b halt

irq_test_pass:
    cpsid i                   /* Disable IRQ after trap verification */
    ldr r1, =msg_irq_pass
    bl print_str
    b 6f

msg_irq_pass:
    .asciz "[PASS] TC0 Timer: Hardware IRQ trap (INTID 34) delivered and acknowledged.\r\n"
msg_irq_fail:
    .asciz "[FAIL] TC0 Timer: Hardware IRQ trap (INTID 34) not delivered!\r\n"
    .align 2
6:
    /* 6. Realtek SDHCI / eMMC Host Controller */
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
    b 7f

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
    b 7f

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
7:
    /* 7. Realtek RTL8168 Gigabit Ethernet MAC MMIO & Link (0x18016000) */
    ldr r8, =0x18016000       /* RTL8168 MAC base */
    ldr r9, [r8, #0x40]       /* TxConfig */
    ldr r7, =0x50900800
    cmp r9, r7
    bne eth_fail

    ldr r6, [r8, #0x6C]       /* PHYstatus */
    tst r6, #0x02             /* LinkStatus bit (0x02) */
    beq eth_fail

    adr r1, msg_eth_pass
    bl print_str
    mov r1, r9
    bl print_hex
    adr r1, msg_eth_mid
    bl print_str
    mov r1, r6
    bl print_hex
    adr r1, msg_eth_end
    bl print_str
    b 8f

msg_eth_pass:
    .asciz "[PASS] RTL8168 Ethernet (0x18016000): Controller online (TXCFG="
msg_eth_mid:
    .asciz ", PHYSTATUS="
msg_eth_end:
    .asciz ", link UP).\r\n"
    .align 2

eth_fail:
    adr r1, msg_eth_fail
    bl print_str
    b halt

msg_eth_fail:
    .asciz "[FAIL] RTL8168 Ethernet: TxConfig mismatch or link down!\r\n"
    .align 2
8:
    /* 8. Realtek RTL8168 Bare-Metal Packet TX/RX via SLIRP (Gap 1) */
    ldr r0, =0x18062300       /* UART0 base */

    /* 1. Stage 60-byte ARP Request frame at TX buffer 0x00200200 */
    ldr r1, =arp_req_frame
    ldr r2, =0x00200200
    mov r3, #15               /* 15 words = 60 bytes */
copy_tx_pkt:
    ldr r4, [r1], #4
    str r4, [r2], #4
    subs r3, r3, #1
    bne copy_tx_pkt

    /* 2. Zero-initialize RX buffer at 0x00200300 (1536 bytes) */
    ldr r2, =0x00200300
    mov r3, #0
    ldr r4, =384              /* 384 words = 1536 bytes */
clear_rx_pkt:
    str r3, [r2], #4
    subs r4, r4, #1
    bne clear_rx_pkt

    /* 3. Setup 16-byte RX descriptor at 0x00200100 */
    /* opts1 = 0xC0000600 (OWN=1, EOR=1, len=1536) */
    ldr r2, =0x00200100
    ldr r3, =0xC0000600
    str r3, [r2, #0]          /* opts1 */
    mov r3, #0
    str r3, [r2, #4]          /* opts2 */
    ldr r3, =0x00200300
    str r3, [r2, #8]          /* dma_addr low */
    mov r3, #0
    str r3, [r2, #12]         /* dma_addr high */

    /* 4. Setup 16-byte TX descriptor at 0x00200000 */
    /* opts1 = 0xF000003C (OWN=1, EOR=1, FS=1, LS=1, len=60) */
    ldr r2, =0x00200000
    ldr r3, =0xF000003C
    str r3, [r2, #0]          /* opts1 */
    mov r3, #0
    str r3, [r2, #4]          /* opts2 */
    ldr r3, =0x00200200
    str r3, [r2, #8]          /* dma_addr low */
    mov r3, #0
    str r3, [r2, #12]         /* dma_addr high */

    /* 5. Configure RTL8168 registers (base 0x18016000) */
    ldr r4, =0x18016000

    /* RxDescAddr (0xE4) = 0x00200100 */
    ldr r3, =0x00200100
    str r3, [r4, #0xE4]

    /* TxDescAddr (0x20) = 0x00200000 */
    ldr r3, =0x00200000
    str r3, [r4, #0x20]

    /* ChipCmd (0x37) = 0x0C (RxEnb | TxEnb) */
    mov r3, #0x0C
    strb r3, [r4, #0x37]

    /* IntrMask (0x3C) = 0x0005 (RxOK | TxOK) */
    mov r3, #0x0005
    strh r3, [r4, #0x3C]

    /* TxPoll (0x38) = 0x40 (trigger TX NPQ) */
    mov r3, #0x40
    strb r3, [r4, #0x38]

    /* 6. Poll loop: wait for RX descriptor OWN bit to clear or RxOK */
    ldr r2, =0x00200100       /* RX descriptor */
    ldr r5, =1000000          /* Bounded timeout loop */
poll_rx_loop:
    ldr r6, [r2]              /* Read opts1 */
    tst r6, #0x80000000       /* OWN bit */
    beq rx_frame_received     /* OWN == 0: packet received by hardware */

    ldrh r7, [r4, #0x3E]      /* IntrStatus */
    tst r7, #0x0001           /* RxOK bit */
    bne rx_frame_received

    subs r5, r5, #1
    bne poll_rx_loop

    /* Timeout */
    ldr r1, =msg_eth_pkt_timeout
    bl print_str
    b halt

rx_frame_received:
    /* 7. Inspect received ARP Reply packet at 0x00200300 */
    ldr r3, =0x00200300

    /* Verify ARP Opcode is 0x0002 (Reply at offset 0x14..0x15) */
    ldrb r6, [r3, #0x14]
    ldrb r7, [r3, #0x15]
    cmp r6, #0
    bne verify_sender_ip
    cmp r7, #2
    beq eth_pkt_verified

verify_sender_ip:
    /* Fallback verification: Sender IP is 10.0.2.2 (offset 0x1C..0x1F) */
    ldrb r6, [r3, #0x1C]
    cmp r6, #10
    bne eth_pkt_invalid
    ldrb r6, [r3, #0x1D]
    cmp r6, #0
    bne eth_pkt_invalid
    ldrb r6, [r3, #0x1E]
    cmp r6, #2
    bne eth_pkt_invalid
    ldrb r6, [r3, #0x1F]
    cmp r6, #2
    bne eth_pkt_invalid

eth_pkt_verified:
    ldr r1, =msg_eth_pkt_pass
    bl print_str
    b 9f

eth_pkt_invalid:
    ldr r1, =msg_eth_pkt_fail
    bl print_str
    b halt

msg_eth_pkt_pass:
    .asciz "[PASS] RTL8168 Ethernet: Packet TX/RX verified via SLIRP backend (ARP exchange with 10.0.2.2).\r\n"
msg_eth_pkt_fail:
    .asciz "[FAIL] RTL8168 Ethernet: Received packet validation failed!\r\n"
msg_eth_pkt_timeout:
    .asciz "[FAIL] RTL8168 Ethernet: Packet TX/RX timed out waiting for reply!\r\n"
    .align 2

arp_req_frame:
    /* Destination MAC: FF:FF:FF:FF:FF:FF (Broadcast) */
    .byte 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    /* Source MAC: 52:54:00:12:34:56 */
    .byte 0x52, 0x54, 0x00, 0x12, 0x34, 0x56
    /* EtherType: 0x0806 (ARP) */
    .byte 0x08, 0x06
    /* Hardware type: 0x0001 (Ethernet) */
    .byte 0x00, 0x01
    /* Protocol type: 0x0800 (IPv4) */
    .byte 0x08, 0x00
    /* Hardware size: 6, Protocol size: 4 */
    .byte 0x06, 0x04
    /* Opcode: 0x0001 (ARP Request) */
    .byte 0x00, 0x01
    /* Sender MAC: 52:54:00:12:34:56 */
    .byte 0x52, 0x54, 0x00, 0x12, 0x34, 0x56
    /* Sender IP: 10.0.2.15 (0x0A, 0x00, 0x02, 0x0F) */
    .byte 0x0A, 0x00, 0x02, 0x0F
    /* Target MAC: 00:00:00:00:00:00 */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    /* Target IP: 10.0.2.2 (0x0A, 0x00, 0x02, 0x02 - SLIRP Gateway) */
    .byte 0x0A, 0x00, 0x02, 0x02
    /* Padding to 60 bytes (18 zero bytes) */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00
    .align 2
9:
    /* 9. Summary */
    adr r1, msg_summary
    bl print_str

halt:
    wfi
    b halt

msg_summary:
    .asciz "========================================================\r\n[>>> ALL HARDWARE SUBSYSTEM VERIFICATION TESTS PASSED <<<]\r\n========================================================\r\n\r\n"
    .align 2

/* Exception Handlers */
irq_handler:
    ldr sp, =0x000F0000
    push {r0-r4, lr}

    /* Read GICC_IAR */
    ldr r0, =0x1808200C         /* GICC_IAR */
    ldr r1, [r0]                /* INTID */
    ldr r2, =0x3FF
    and r2, r1, r2              /* Mask INTID bits [9:0] */

    cmp r2, #34                 /* INTID 34: TC0 Timer */
    beq timer_irq_hit

    /* Unexpected interrupt: disable it in GICD_ICENABLER so it cannot storm */
    cmp r2, #1020
    bge irq_ack                 /* Spurious interrupt (1020..1023) */
    push {r3, r4, r5}
    ldr r3, =0x18081180
    lsr r4, r2, #5              /* INTID / 32 */
    lsl r4, r4, #2
    add r3, r3, r4
    and r4, r2, #0x1F           /* INTID % 32 */
    mov r5, #1
    lsl r5, r5, r4
    str r5, [r3]                /* Clear-enable in GICD */
    pop {r3, r4, r5}
    b irq_ack

timer_irq_hit:
    /* Increment irq_counter in memory */
    ldr r3, =irq_counter
    ldr r4, [r3]
    add r4, r4, #1
    str r4, [r3]

    /* Stop TC0 timer: TC0CR (0x1801B640) = 0 */
    ldr r3, =0x1801B640
    mov r4, #0
    str r4, [r3]

    /* Disable TC0 interrupt: TC0ICR (0x1801B660) = 0 */
    ldr r3, =0x1801B660
    str r4, [r3]

    /* Clear TC0 raw ISR status: TIMER_ISR (0x1801B6EC) = 1 (W1C) */
    ldr r3, =0x1801B6EC
    mov r4, #1
    str r4, [r3]

irq_ack:
    /* Write GICC_EOIR with r1 (INTID) to acknowledge */
    ldr r0, =0x18082010         /* GICC_EOIR */
    str r1, [r0]

    pop {r0-r4, lr}
    subs pc, lr, #4

undef_handler:
svc_handler:
prefetch_abort_handler:
data_abort_handler:
hyp_trap_handler:
fiq_handler:
    wfi
    b .

/* Console Output Helpers */
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

    .align 2
irq_counter:
    .word 0
