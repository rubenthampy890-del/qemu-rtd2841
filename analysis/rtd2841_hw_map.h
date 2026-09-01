/*
 * Realtek RTD2841 / RTK284x Hardware Specification Map
 * Target: TCL 40" Smart TV (Chassis: RT41DT, Board: tv030, Client: TCL-AP-RT41DT-S1)
 * Purpose: Machine Definition Header for QEMU System Emulation (hw/arm/rtd2841.c)
 *
 * Source Evidence:
 *   - TCL GPL Linux 4.4 Kernel: arch/arm/boot/dts/rtd284x.dts
 *   - Machine Support Code: arch/arm/mach-rtd284x/
 *       * platsmp.c, common.c, platform.c, timer.c, serial.c, io.c
 *       * include/mach/system.h, io.h, iomap.h, irqs.h, rtk_platform.h
 *   - Register Definitions: arch/arm/mach-rtd284x/include/rbus/
 *       * iso_misc_off_uart_reg.h, misc_uart_reg.h, wdog_reg.h, timer_reg.h
 *   - TCL Board Configuration: analysis/track3_tclconfig/model/model_AP_ID43.ini
 */

#ifndef RTD2841_HW_MAP_H
#define RTD2841_HW_MAP_H

#include <stdint.h>

/* ============================================================================
 * 1. CPU CLUSTER ARCHITECTURE & SMP BOOTSTRAP
 * ============================================================================
 * Realtek RTD2841 integrates a quad-core ARM Cortex-A53 MPCore cluster.
 * DT Reference: arch/arm/boot/dts/rtd284x.dts (cluster0, core0..core3)
 * Bootloader / SMP Reference: arch/arm/mach-rtd284x/platsmp.c & system.h
 */
#define RTD2841_CPU_TYPE                "cortex-a53"
#define RTD2841_SMP_NUM_CPUS            4

/* SMP Secondary CPU Jump Address Mailbox in Physical SRAM / Low DRAM */
#define RTD2841_SMP_CPU1_JUMP_ADDR      0x00000504  /* DUMMY_REG_FOR_2ND_CPU_JUMPADDRESS */
#define RTD2841_SMP_CPU2_JUMP_ADDR      0x00000508  /* DUMMY_REG_FOR_3RD_CPU_JUMPADDRESS */
#define RTD2841_SMP_CPU3_JUMP_ADDR      0x0000050C  /* DUMMY_REG_FOR_4TH_CPU_JUMPADDRESS */

/* TrustZone / PSCI SMP Enable Method */
#define RTD2841_SMP_ENABLE_METHOD       "psci"
#define RTD2841_OPTEE_SMC_METHOD        "smc"

/* Clocks & Reference Frequencies */
#define RTD2841_OSC_CLOCK_FREQ          27000000ULL /* 27 MHz reference crystal */
#define RTD2841_TIMER_CLOCK_FREQ        27000000ULL /* 27 MHz system timer clock */
#define RTD2841_UART_CLOCK_FREQ         27000000ULL /* 27 MHz UART baud base */
#define RTD2841_SDIO_MAX_FREQ           37000000ULL /* 37 MHz SDIO clock */


/* ============================================================================
 * 2. PHYSICAL DRAM LAYOUT (1024 MB / 1 GB FOR TV030 2K-MODEL)
 * ============================================================================
 * DRAM starts at physical 0x00000000 (CONFIG_PHYS_OFFSET = 0x00000000).
 * Kernel virtual map: PAGE_OFFSET = 0xC0000000.
 */
#define RTD2841_DRAM_BASE               0x00000000ULL
#define RTD2841_DRAM_SIZE               0x40000000ULL /* 1024 MB Physical DRAM */
#define RTD2841_PAGE_OFFSET             0xC0000000ULL


/* ============================================================================
 * 3. DEDICATED MEMORY CARVEOUTS & HARDWARE RESERVATIONS
 * ============================================================================
 * Sourced from arch/arm/mach-rtd284x/platform.c (carvedout_buf) & common.c.
 * TV030 board profile enforces CONFIG_REALTEK_2K_MODEL_ENABLED=y and CONFIG_OPTEE=y.
 * Note: Zero address overlap exists across any reserved ranges.
 */

/* 3.1 Low Memory Bootcode Data & SMP Mailbox */
#define RTD2841_CARVEOUT_BOOTCODE_BASE  0x00000000ULL
#define RTD2841_CARVEOUT_BOOTCODE_SIZE  0x00100000ULL /* 1 MB */

/* 3.2 DTV Demodulator Firmware & Buffers */
#define RTD2841_CARVEOUT_DEMOD_BASE     0x02000000ULL
#define RTD2841_CARVEOUT_DEMOD_SIZE     0x00800000ULL /* 8 MB */

/* 3.3 Audio DSP / Video DSP DMEM Reservation */
#define RTD2841_CARVEOUT_AV_DMEM_BASE   0x10000000ULL
#define RTD2841_CARVEOUT_AV_DMEM_SIZE   0x00100000ULL /* 1 MB Audio/Video DMEM */

/* 3.4 OP-TEE TrustZone Secure OS (CARVEDOUT_K_OS) */
/* 15 MB for 2K-Model profile; 24 MB for standard 4K profile */
#define RTD2841_CARVEOUT_OPTEE_BASE     0x16000000ULL
#define RTD2841_CARVEOUT_OPTEE_SIZE     0x00F00000ULL /* 15 MB Secure Carveout */

/* 3.5 SoC Internal Register Bus (RBUS MMIO Area) */
#define RTD2841_RBUS_BASE_PHYS          0x18000000ULL
#define RTD2841_RBUS_BASE_SIZE          0x00400000ULL /* 4 MB MMIO Window */
#define RTD2841_RBUS_BASE_VIRT          0xFE000000ULL /* Kernel static unmapped virtual */
#define RTD2841_RBUS_BASE_VIRT_OLD      0xB8000000ULL /* Legacy Realtek 0xB8xxxxxx */

/* 3.6 Mali GPU Firmware Buffer */
#define RTD2841_CARVEOUT_GPU_FW_BASE    0x1A480000ULL
#define RTD2841_CARVEOUT_GPU_FW_SIZE    0x00080000ULL /* 512 KB */

/* 3.7 Audio / Video Co-Processor OS (A1/A2/V1/V2 Firmware) */
#define RTD2841_CARVEOUT_AV_OS_BASE     0x1A500000ULL
#define RTD2841_CARVEOUT_AV_OS_SIZE     0x00900000ULL /* 9 MB */

/* 3.8 Inter-Processor RPC / Mailbox Hardware Buffer */
#define RTD2841_RPC_BASE_PHYS           0x1B180000ULL
#define RTD2841_RPC_BASE_SIZE           0x00080000ULL /* 512 KB */

/* 3.9 Hardware Video Decoder Ringbuffer (CARVEDOUT_VDEC_RINGBUF) */
#define RTD2841_VDEC_RINGBUF_BASE       0x1B200000ULL
#define RTD2841_VDEC_RINGBUF_SIZE       0x01000000ULL /* 16 MB Video Bitstream Buffer */

/* 3.10 Early Boot Diagnostic Log Buffer */
#define RTD2841_LOGBUF_BASE             0x1CA00000ULL
#define RTD2841_LOGBUF_SIZE             0x00080000ULL /* 512 KB */

/* 3.11 SoC Boot ROM Mirror */
#define RTD2841_ROMCODE_BASE            0x1FC00000ULL
#define RTD2841_ROMCODE_SIZE            0x00008000ULL /* 32 KB */

/* 3.12 Persistent RAM / RAMOOPS (PSTORE) */
#define RTD2841_RAMOOPS_BASE            0x1FD00000ULL
#define RTD2841_RAMOOPS_SIZE            0x00010000ULL /* 64 KB */

/* 3.13 Hardware Video Decoder Framebuffer (CARVEDOUT_VDEC_VBM) */
/* 28 MB in 2K-Model profile; 128 MB in 4K profile */
#define RTD2841_VDEC_VBM_BASE           0x20000000ULL
#define RTD2841_VDEC_VBM_SIZE           0x01C00000ULL /* 28 MB Decoded Frame Buffer */

/* 3.14 Picture Scaler Engine Carveout (VIP + OverDrive) */
/* Scaler M-Domain: 0 MB in 2K frame-sync mode */
/* Scaler VIP: 8 MB at 0x21C00000 */
/* Scaler OD (OverDrive): 4 MB at 0x22400000 */
#define RTD2841_SCALER_VIP_BASE         0x21C00000ULL
#define RTD2841_SCALER_VIP_SIZE         0x00800000ULL /* 8 MB */
#define RTD2841_SCALER_OD_BASE          0x22400000ULL
#define RTD2841_SCALER_OD_SIZE          0x00400000ULL /* 4 MB */
#define RTD2841_SCALER_MODULE_BASE      RTD2841_SCALER_VIP_BASE
#define RTD2841_SCALER_MODULE_SIZE      (RTD2841_SCALER_VIP_SIZE + RTD2841_SCALER_OD_SIZE) /* 12 MB */


/* ============================================================================
 * 4. ARM GENERIC INTERRUPT CONTROLLER (GIC-400 / CORTEX-A9 GIC)
 * ============================================================================
 * DTS Node: gic: interrupt-controller@0x18081000
 * Compatible: "realtek,cortex-a9-gic", standard ARM GIC architecture.
 * Reference: arch/arm/boot/dts/rtd284x.dts line 150
 *            arch/arm/mach-rtd284x/include/mach/system.h line 47-48
 */
#define RTD2841_SCU_BASE                0x18080000ULL
#define RTD2841_SCU_SIZE                0x00001000ULL /* 4 KB */

#define RTD2841_GIC_DIST_BASE           0x18081000ULL /* GICD (Distributor) */
#define RTD2841_GIC_DIST_SIZE           0x00001000ULL /* 4 KB */

#define RTD2841_GIC_CPU_BASE            0x18082000ULL /* GICC (CPU Interface) */
#define RTD2841_GIC_CPU_SIZE            0x00001000ULL /* 4 KB */

#define RTD2841_GIC_NR_IRQS             256


/* ============================================================================
 * 5. MMIO PERIPHERAL BLOCKS (INTERNAL REGISTER BUS - RBUS)
 * ============================================================================
 * Physical RBUS base: 0x18000000
 * Realtek internal registers are accessed using 32-bit words (reg-shift = 2).
 */

/* 5.1 Thermal Management Unit (TMU) & Hardware Monitor */
/* DTS Reference: thermal@0x18000000, compatible = "rtk_rtk299s-tmu" */
#define RTD2841_TMU_BASE                0x18000000ULL
#define RTD2841_TMU_SIZE                0x00000400ULL /* 1 KB */

/* 5.2 eMMC / SDHCI Storage Controller */
/* DTS Reference: emmc@0x18010800, compatible = "realtek,rtk-emmc" */
#define RTD2841_EMMC_BASE               0x18010800ULL
#define RTD2841_EMMC_SIZE               0x00000400ULL /* 1 KB */

/* 5.3 SDIO Storage Controller */
/* DTS Reference: sdio@0x18010c00, compatible = "realtek,rtk-sdio" */
#define RTD2841_SDIO_BASE               0x18010C00ULL
#define RTD2841_SDIO_SIZE               0x00000400ULL /* 1 KB */

/* 5.4 Secondary UARTs (MISC Subsystem) */
/* Register Definitions: rbus/misc_uart_reg.h */
#define RTD2841_UART1_BASE              0x1801B100ULL /* MISC_UART_U1RBR_THR_DLL */
#define RTD2841_UART1_SIZE              0x00000100ULL
#define RTD2841_UART2_BASE              0x1801B200ULL /* MISC_UART_U2RBR_THR_DLL */
#define RTD2841_UART2_SIZE              0x00000100ULL
#define RTD2841_UART3_BASE              0x1801B300ULL /* MISC_UART_U3RBR_THR_DLL */
#define RTD2841_UART3_SIZE              0x00000100ULL

/* 5.5 Realtek Hardware Timers (8 Timers: TC0..TC7) */
/* Register Definitions: rbus/timer_reg.h (TIMER_TC0TVR_reg = 0xB801B600) */
#define RTD2841_TIMER_BASE              0x1801B600ULL
#define RTD2841_TIMER_SIZE              0x00000200ULL /* 512 B */

/* 5.6 Mali-T6xx / Midgard GPU */
/* DTS Reference: gpu@0x18040000, compatible = "arm,malit6xx", "arm,mali-midgard" */
#define RTD2841_GPU_BASE                0x18040000ULL
#define RTD2841_GPU_SIZE                0x00004000ULL /* 16 KB */

/* 5.7 ARM Architected Timer CoreSight Timestamp Generator Control */
/* Reference: arch/arm/mach-rtd284x/timer.c line 27-28 */
#define RTD2841_ARM_TSG_BASE            0x1805C800ULL /* WRAP_A7_APB_TSG_EN0 */
#define RTD2841_ARM_TSG_SIZE            0x00000100ULL

/* 5.8 Hardware Watchdog Timer */
/* Register Definitions: rbus/wdog_reg.h (WDOG_MSK_ISR = 0xB8062200) */
#define RTD2841_WDOG_BASE               0x18062200ULL
#define RTD2841_WDOG_SIZE               0x00000100ULL

/* 5.9 Primary Serial Console (UART0 / ttyS0) */
/* DTS Reference: uart0: serial@18062300, compatible = "realtek,rtd29xx-uart" */
/* Register Definitions: rbus/iso_misc_off_uart_reg.h */
#define RTD2841_UART0_BASE              0x18062300ULL /* ISO_MISC_OFF_UART_U0RBR_THR_DLL */
#define RTD2841_UART0_SIZE              0x00001000ULL /* 4 KB */
#define RTD2841_UART0_REG_SHIFT         2             /* 32-bit registers (UPIO_MEM32) */
#define RTD2841_UART0_REG_WIDTH         4             /* 4 bytes per register */


/* ============================================================================
 * 6. INTERRUPT MAPPING TABLE (GIC-400 IRQ LINES)
 * ============================================================================
 * Reference: arch/arm/mach-rtd284x/include/mach/irqs.h
 * GIC Hardware Convention:
 *   - IRQs  0..15 : SGI (Software Generated Interrupts)
 *   - IRQs 16..31 : PPI (Private Peripheral Interrupts)
 *   - IRQs 32..255: SPI (Shared Peripheral Interrupts, SPI N -> GIC 32 + N)
 */

/* 6.1 ARM Architected Timer PPI Lines */
#define RTD2841_IRQ_PPI_SEC_PHYS_TIMER  29 /* GIC PPI 13: Secure Physical Timer */
#define RTD2841_IRQ_PPI_NONSEC_PHYS_TMR 30 /* GIC PPI 14: Non-Secure Physical Timer */
#define RTD2841_IRQ_PPI_HYP_TIMER       26 /* GIC PPI 10: Hypervisor Timer */
#define RTD2841_IRQ_PPI_VIRT_TIMER      27 /* GIC PPI 11: Virtual Timer (or 26) */

/* 6.2 Shared Peripheral Interrupts (SPI Lines) */
#define RTD2841_IRQ_WDOG_NMI            32 /* SPI  0: Watchdog NMI */
#define RTD2841_IRQ_RTC                 33 /* SPI  1: Real-Time Clock */
#define RTD2841_IRQ_TIMER               34 /* SPI  2: Hardware GP Timer 2 (SCPU) */
#define RTD2841_IRQ_CPU2CPU             35 /* SPI  3: ACPU/VCPU/VCPU2 to SCPU */
#define RTD2841_IRQ_DCSYS               36 /* SPI  4: DCSYS-PHY / SB2 */
#define RTD2841_IRQ_AUDIO_DMA           37 /* SPI  5: Audio DMA Engine */
#define RTD2841_IRQ_IMDOMAIN            38 /* SPI  6: VODMA / DDC / I/M-Domain */
#define RTD2841_IRQ_DDOMAIN             39 /* SPI  7: DCT / UZ / OSD / TVE */
#define RTD2841_IRQ_AXI                 40 /* SPI  8: AXI Bus Monitor */
#define RTD2841_IRQ_VDEC                41 /* SPI  9: Video Decoder Engine */
#define RTD2841_IRQ_VBI                 42 /* SPI 10: Video Blanking Interval */
#define RTD2841_IRQ_SE                  43 /* SPI 11: Security Engine (Crypto/DMA) */
#define RTD2841_IRQ_MCP                 44 /* SPI 12: Media Co-Processor */
#define RTD2841_IRQ_MD                  45 /* SPI 13: Memory Demux */
#define RTD2841_IRQ_GPU                 46 /* SPI 14: Mali GPU Core IRQ */
#define RTD2841_IRQ_IFDEMOD             47 /* SPI 15: Intermediate Frequency Demod */
#define RTD2841_IRQ_GPU_PMU             48 /* SPI 16: Mali GPU Power Management */
#define RTD2841_IRQ_NF                  49 /* SPI 17: NAND Flash Controller */
#define RTD2841_IRQ_CR_EMMC             50 /* SPI 18: Card Reader / eMMC Controller */
#define RTD2841_IRQ_MISC                51 /* SPI 19: MISC (UART1..3 / GPIO / I2C) */
#define RTD2841_IRQ_ETH                 52 /* SPI 20: Ethernet MAC */
#define RTD2841_IRQ_USB                 53 /* SPI 21: USB Controller */
#define RTD2841_IRQ_TP                  54 /* SPI 22: Transport Processor */
#define RTD2841_IRQ_HDMI                55 /* SPI 23: HDMI Rx Controller */
#define RTD2841_IRQ_SDIO                56 /* SPI 24: SDIO Controller */
#define RTD2841_IRQ_UART0_CEC           57 /* SPI 25: UART0 Console & CEC Controller */
#define RTD2841_IRQ_GPU_JOB             58 /* SPI 26: Mali GPU Job Management */
#define RTD2841_IRQ_GPU_MMU             59 /* SPI 27: Mali GPU MMU Fault */
#define RTD2841_IRQ_MEMC                61 /* SPI 29: Memory Controller */
#define RTD2841_IRQ_PMU_CPU0            62 /* SPI 30: ARM Performance Monitor Core 0 */
#define RTD2841_IRQ_PMU_CPU1            63 /* SPI 31: ARM Performance Monitor Core 1 */
#define RTD2841_IRQ_PMU_CPU2            64 /* SPI 32: ARM Performance Monitor Core 2 */
#define RTD2841_IRQ_PMU_CPU3            65 /* SPI 33: ARM Performance Monitor Core 3 */
#define RTD2841_IRQ_DDEMOD              70 /* SPI 38: Digital Demodulator */
#define RTD2841_IRQ_THERMAL             73 /* SPI 41: Thermal Sensor Alarm */


/* ============================================================================
 * 7. UART0 REGISTER OFFSETS (STANDARD 8250 REGISTER SET WITH 32-BIT STRIDE)
 * ============================================================================
 * Register stride = 4 bytes (shift = 2)
 */
#define RTD2841_UART_RBR                0x00 /* Receive Buffer Register (Read) */
#define RTD2841_UART_THR                0x00 /* Transmitter Holding Register (Write) */
#define RTD2841_UART_DLL                0x00 /* Divisor Latch LSB (DLAB=1) */
#define RTD2841_UART_IER                0x04 /* Interrupt Enable Register */
#define RTD2841_UART_DLH                0x04 /* Divisor Latch MSB (DLAB=1) */
#define RTD2841_UART_IIR                0x08 /* Interrupt Identification Register (Read) */
#define RTD2841_UART_FCR                0x08 /* FIFO Control Register (Write) */
#define RTD2841_UART_LCR                0x0C /* Line Control Register */
#define RTD2841_UART_MCR                0x10 /* Modem Control Register */
#define RTD2841_UART_LSR                0x14 /* Line Status Register */
#define RTD2841_UART_MSR                0x18 /* Modem Status Register */
#define RTD2841_UART_SCR                0x1C /* Scratchpad Register */

#endif /* RTD2841_HW_MAP_H */
