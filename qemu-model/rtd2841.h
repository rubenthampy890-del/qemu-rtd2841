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
#include "net/net.h"

#define TYPE_RTD2841_SOC "rtd2841-soc"
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841SocState, RTD2841_SOC)

#define TYPE_RTD2841_MACHINE MACHINE_TYPE_NAME("rtd2841-tv030")
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841MachineState, RTD2841_MACHINE)

#define TYPE_RTD2841_ETH "rtd2841-eth"
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841EthState, RTD2841_ETH)

/* SoC Hardware Architecture Parameters */
#define RTD2841_NUM_CPUS            4
#define RTD2841_RAM_BASE            0x00000000ULL
#define RTD2841_RAM_SIZE            (1024ULL * 1024ULL * 1024ULL) /* 1 GB */

/* SMP Secondary CPU Jump Address Mailbox in Low DRAM */
#define RTD2841_SMP_CPU1_JUMP_ADDR  0x00000504ULL
#define RTD2841_SMP_CPU2_JUMP_ADDR  0x00000508ULL
#define RTD2841_SMP_CPU3_JUMP_ADDR  0x0000050CULL

/* ARM Generic Interrupt Controller (GIC-400 / GICv2) */
#define RTD2841_GIC_DIST_BASE       0x18081000ULL
#define RTD2841_GIC_DIST_SIZE       0x00001000ULL           /* 4 KB */
#define RTD2841_GIC_CPU_BASE        0x18082000ULL
#define RTD2841_GIC_CPU_SIZE        0x00001000ULL           /* 4 KB */
#define RTD2841_GIC_NUM_IRQS        256                     /* 256 INTIDs */

/* ARM Architected Timer PPI Lines (INTID 16..31) */
#define RTD2841_IRQ_PPI_SEC_PHYS_TIMER  29 /* GIC PPI 13: Secure Phys Timer */
#define RTD2841_IRQ_PPI_NONSEC_PHYS_TMR 30 /* GIC PPI 14: Non-Sec Phys Timer */
#define RTD2841_IRQ_PPI_HYP_TIMER       26 /* GIC PPI 10: Hyp Timer */
#define RTD2841_IRQ_PPI_VIRT_TIMER      27 /* GIC PPI 11: Virt Timer */

/* Serial Console (16550A UART0) */
#define RTD2841_UART0_BASE          0x18062300ULL
#define RTD2841_UART0_SIZE          0x00001000ULL           /* 4 KB */
#define RTD2841_UART0_SPI           25                      /* SPI 25 */
#define RTD2841_UART0_IRQ           (32 + RTD2841_UART0_SPI) /* GIC INTID 57 */
#define RTD2841_UART0_CLK           27000000                /* 27 MHz ref */
#define RTD2841_UART0_BAUDBASE      (RTD2841_UART0_CLK / 16)/* 1687500 Hz */
#define RTD2841_UART0_REG_SHIFT     2                       /* 4-byte stride */

/* Realtek Gigabit Ethernet Controller (RTL8168GU) */
#define RTD2841_ETH_BASE            0x18016000ULL
#define RTD2841_ETH_SIZE            0x00001000ULL           /* 4 KB */
#define RTD2841_ETH_SPI             20                      /* SPI 20 */
#define RTD2841_ETH_IRQ             (32 + RTD2841_ETH_SPI)  /* GIC INTID 52 */
#define RTD2841_ETH_NUM_DESC        256                     /* Ring limit */

/* Realtek RBUS Peripheral MMIO Apertures & Stubs */
#define RTD2841_RBUS_BASE           0x18000000ULL
#define RTD2841_RBUS_SIZE           0x00400000ULL           /* 4 MB window */

#define RTD2841_TMU_BASE            0x18000000ULL
#define RTD2841_TMU_SIZE            0x00000400ULL           /* 1 KB TMU */
#define RTD2841_SYSREG_BASE         0x18000400ULL
#define RTD2841_SYSREG_SIZE         0x00000C00ULL           /* 3 KB SYSREG */
#define RTD2841_EMMC_BASE           0x18010800ULL
#define RTD2841_EMMC_SIZE           0x00000400ULL           /* 1 KB */
#define RTD2841_EMMC_SPI            18                      /* SPI 18 -> 50 */
#define RTD2841_SDIO_BASE           0x18010C00ULL
#define RTD2841_SDIO_SIZE           0x00000400ULL           /* 1 KB */
#define RTD2841_UART1_3_BASE        0x1801B100ULL
#define RTD2841_UART1_3_SIZE        0x00000300ULL           /* 768 B MISC */
#define RTD2841_TIMER_BASE          0x1801B600ULL
#define RTD2841_TIMER_SIZE          0x00000200ULL           /* 512 B Timers */
#define RTD2841_TIMER_SPI           2                       /* SPI 2 -> 34 */
#define RTD2841_WDOG_SPI            0                       /* SPI 0 -> 32 */

DeviceState *rtd2841_timer_create(hwaddr base, qemu_irq irq);
DeviceState *rtd2841_wdog_create(hwaddr base);
#define RTD2841_GPU_BASE            0x18040000ULL
#define RTD2841_GPU_SIZE            0x00004000ULL           /* 16 KB GPU */
#define RTD2841_ARM_TSG_BASE        0x1805C800ULL
#define RTD2841_ARM_TSG_SIZE        0x00000100ULL           /* 256 B TSG */
#define RTD2841_WDOG_BASE           0x18062200ULL
#define RTD2841_WDOG_SIZE           0x00000100ULL           /* 256 B WDOG */
#define RTD2841_SCU_BASE            0x18080000ULL
#define RTD2841_SCU_SIZE            0x00001000ULL           /* 4 KB SCU */

#define RTD2841_VDEC_MEM_BASE       0x10000000ULL
#define RTD2841_VDEC_MEM_SIZE       0x06000000ULL           /* 96 MB Carveout */

/* Simple Framebuffer (1920x1080 @ 32bpp, 8 MB Carveout at 992 MB offset) */
#define RTD2841_FB_BASE             0x3E000000ULL
#define RTD2841_FB_SIZE             0x00800000ULL           /* 8 MB */
#define RTD2841_FB_WIDTH            1920
#define RTD2841_FB_HEIGHT           1080
#define RTD2841_FB_STRIDE           (RTD2841_FB_WIDTH * 4)

#include "ui/console.h"

/* Backward compatibility / abstraction shims for QEMU GraphicConsole APIs */
typedef GraphicHwOps GraphicHardwarePageOps;

#ifndef graphic_console_init
#define graphic_console_init(dev, head, ops, opaque) \
    qemu_graphic_console_create(dev, head, ops, opaque)
#endif

#ifndef dpy_gfx_replace_surface
#define dpy_gfx_replace_surface(con, surface) \
    qemu_console_set_surface(con, surface)
#endif

#ifndef dpy_gfx_update
#define dpy_gfx_update(con, x, y, w, h) \
    qemu_console_update(con, x, y, w, h)
#endif

/*
 * GICv2 GPIO Input calculation macros for QEMU arm_gic:
 * - Inputs [0 .. num_irq - 33] are Shared Peripheral Interrupts (SPI).
 * - Inputs [num_irq - 32 .. end] are per-CPU interrupts (SGI, PPI).
 */
#define RTD2841_GIC_SPI_TO_GPIO(spi)        (spi)
#define RTD2841_GIC_PPI_TO_GPIO(cpu, intid) \
    ((RTD2841_GIC_NUM_IRQS - 32) + ((cpu) * 32) + (intid))

struct RTD2841EthState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion mmio;
    NICState *nic;
    NICConf conf;
    qemu_irq irq;

    uint8_t mac[6];
    uint8_t mar[8];
    uint8_t chipcmd;
    uint16_t intr_mask;
    uint16_t intr_status;
    uint32_t tx_config;
    uint32_t rx_config;
    uint8_t cfg9346;
    uint8_t config[6];
    uint16_t multi_intr;
    uint32_t phyar;
    uint32_t eriar;
    uint32_t eridr;
    uint32_t ephyar;
    uint32_t csiar;
    uint32_t csidr;
    uint16_t rx_max_size;
    uint16_t cplus_cmd;
    uint16_t intr_mitigate;
    uint32_t misc;

    uint64_t tx_desc_base;
    uint64_t rx_desc_base;
    uint32_t cur_tx;
    uint32_t cur_rx;

    uint16_t phy_regs[32];
};

struct RTD2841SocState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    ARMCPU cpu[RTD2841_NUM_CPUS];
    GICState gic;
    RTD2841EthState eth;
};

struct RTD2841MachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/

    RTD2841SocState soc;
    struct arm_boot_info boot_info;
    QemuConsole *console;
    MemoryRegion fb_ram;
    bool fb_invalidate;
};

#endif /* HW_ARM_RTD2841_H */
