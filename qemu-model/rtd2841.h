/*
 * Realtek RTD2841 SoC & Machine Model Definitions
 * Target: TCL Smart TV RT41 Chassis (profile tv030)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_ARM_RTD2841_H
#define HW_ARM_RTD2841_H

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/boards.h"
#include "hw/arm/boot.h"
#include "hw/intc/arm_gic.h"
#include "target/arm/cpu.h"
#include "qom/object.h"

#define TYPE_RTD2841_SOC "rtd2841-soc"
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841SocState, RTD2841_SOC)

#define TYPE_RTD2841_MACHINE MACHINE_TYPE_NAME("rtd2841-tv030")
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841MachineState, RTD2841_MACHINE)

/* SoC Hardware Architecture Parameters */
#define RTD2841_NUM_CPUS            4
#define RTD2841_RAM_BASE            0x00000000ULL
#define RTD2841_RAM_SIZE            (1024ULL * 1024ULL * 1024ULL) /* 1 GB physical DRAM */

/* SMP Secondary CPU Jump Address Mailbox in Physical SRAM / Low DRAM */
#define RTD2841_SMP_CPU1_JUMP_ADDR  0x00000504ULL
#define RTD2841_SMP_CPU2_JUMP_ADDR  0x00000508ULL
#define RTD2841_SMP_CPU3_JUMP_ADDR  0x0000050CULL

/* ARM Generic Interrupt Controller (GIC-400 / GICv2) */
#define RTD2841_GIC_DIST_BASE       0x18081000ULL
#define RTD2841_GIC_DIST_SIZE       0x00001000ULL           /* 4 KB */
#define RTD2841_GIC_CPU_BASE        0x18082000ULL
#define RTD2841_GIC_CPU_SIZE        0x00001000ULL           /* 4 KB */
#define RTD2841_GIC_NUM_IRQS        256                     /* Realtek HW GIC: 256 INTIDs */

/* ARM Architected Timer PPI Lines (INTID 16..31) */
#define RTD2841_IRQ_PPI_SEC_PHYS_TIMER  29 /* GIC PPI 13: Secure Physical Timer (INTID 29) */
#define RTD2841_IRQ_PPI_NONSEC_PHYS_TMR 30 /* GIC PPI 14: Non-Secure Physical Timer (INTID 30) */
#define RTD2841_IRQ_PPI_HYP_TIMER       26 /* GIC PPI 10: Hypervisor Timer (INTID 26) */
#define RTD2841_IRQ_PPI_VIRT_TIMER      27 /* GIC PPI 11: Virtual Timer (INTID 27) */

/* Serial Console (16550A UART0) */
#define RTD2841_UART0_BASE          0x18062300ULL
#define RTD2841_UART0_SIZE          0x00001000ULL           /* 4 KB */
#define RTD2841_UART0_SPI           25                      /* SPI 25 */
#define RTD2841_UART0_IRQ           (32 + RTD2841_UART0_SPI) /* GIC INTID 57 */
#define RTD2841_UART0_CLK           27000000                /* 27 MHz reference clock */
#define RTD2841_UART0_BAUDBASE      (RTD2841_UART0_CLK / 16)/* 1687500 Hz */
#define RTD2841_UART0_REG_SHIFT     2                       /* 32-bit registers (4-byte stride) */

/* Realtek RBUS Peripheral MMIO Apertures & Stubs */
#define RTD2841_RBUS_BASE           0x18000000ULL
#define RTD2841_RBUS_SIZE           0x00400000ULL           /* 4 MB MMIO Window */

#define RTD2841_TMU_BASE            0x18000000ULL
#define RTD2841_TMU_SIZE            0x00000400ULL           /* 1 KB: TMU dual-block (0x0 + 0x200) */
#define RTD2841_SYSREG_BASE         0x18000400ULL
#define RTD2841_SYSREG_SIZE         0x00000C00ULL           /* 3 KB: Clock, Reset, Pinmux (0x180004e4) */
#define RTD2841_EMMC_BASE           0x18010800ULL
#define RTD2841_EMMC_SIZE           0x00000400ULL           /* 1 KB */
#define RTD2841_EMMC_SPI            28                      /* SPI 28 -> GIC INTID 60 */
#define RTD2841_SDIO_BASE           0x18010C00ULL
#define RTD2841_SDIO_SIZE           0x00000400ULL           /* 1 KB */
#define RTD2841_UART1_3_BASE        0x1801B100ULL
#define RTD2841_UART1_3_SIZE        0x00000300ULL           /* 768 B: MISC UART1, UART2, UART3 */
#define RTD2841_TIMER_BASE          0x1801B600ULL
#define RTD2841_TIMER_SIZE          0x00000200ULL           /* 512 B: TC0..TC7 Hardware Timers */
#define RTD2841_TIMER_SPI           2                       /* SPI 2 -> GIC INTID 34 */
#define RTD2841_WDOG_SPI            0                       /* SPI 0 -> GIC INTID 32 */

DeviceState *rtd2841_timer_create(hwaddr base, qemu_irq irq);
DeviceState *rtd2841_wdog_create(hwaddr base);
#define RTD2841_GPU_BASE            0x18040000ULL
#define RTD2841_GPU_SIZE            0x00004000ULL           /* 16 KB: Mali-T6xx GPU */
#define RTD2841_ARM_TSG_BASE        0x1805C800ULL
#define RTD2841_ARM_TSG_SIZE        0x00000100ULL           /* 256 B: ARM Arch Timer APB TSG EN0/1 */
#define RTD2841_WDOG_BASE           0x18062200ULL
#define RTD2841_WDOG_SIZE           0x00000100ULL           /* 256 B: Watchdog Timer */
#define RTD2841_SCU_BASE            0x18080000ULL
#define RTD2841_SCU_SIZE            0x00001000ULL           /* 4 KB: SCU / Realtek System Control */

#define RTD2841_VDEC_MEM_BASE       0x10000000ULL
#define RTD2841_VDEC_MEM_SIZE       0x06000000ULL           /* 96 MB DRAM Carveout */

/*
 * GICv2 GPIO Input calculation macros for QEMU arm_gic:
 * - Inputs [0 .. num_irq - 33] are Shared Peripheral Interrupts (SPI 0 .. num_irq - 33).
 * - Inputs [num_irq - 32 .. end] are per-CPU interrupts (SGI 0..15, PPI 16..31).
 *   For CPU i, INTID p (16..31) is at: (num_irq - 32) + (i * 32) + p.
 */
#define RTD2841_GIC_SPI_TO_GPIO(spi)        (spi)
#define RTD2841_GIC_PPI_TO_GPIO(cpu, intid) ((RTD2841_GIC_NUM_IRQS - 32) + ((cpu) * 32) + (intid))

struct RTD2841SocState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    ARMCPU cpu[RTD2841_NUM_CPUS];
    GICState gic;
};

struct RTD2841MachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/

    RTD2841SocState soc;
    struct arm_boot_info boot_info;
};

#endif /* HW_ARM_RTD2841_H */
