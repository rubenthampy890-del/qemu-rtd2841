/*
 * Realtek RTD2841 SoC & TCL Smart TV Board Model (tv030)
 *
 * Emulates the Realtek RTD2841 (RTK284x) SoC featuring:
 * - Quad-Core ARM Cortex-A53 running in AArch32 mode (tv030 chassis)
 * - 1 GB Physical DRAM mapped at 0x00000000
 * - ARM Generic Interrupt Controller (GIC-400 / GICv2) at 0x18081000/0x18082000 (256 IRQs)
 * - 16550A-compatible UART0 console at 0x18062300 (mapped to serial stdio)
 * - PSCI firmware SMP power-state coordination interface (SMC conduit)
 * - Secondary CPU start-powered-off coordination and jump mailbox at 0x504
 * - Stubbed memory apertures for hardware peripherals (TMU, SYSREG, eMMC, SDIO, MISC, Timers, GPU, TSG, WDOG, SCU)
 *
 * Target Firmware: TCL RT41 Chassis (TCL Android / Linux 4.4.3 TV firmware)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "hw/core/sysbus.h"
#include "hw/arm/boot.h"
#include "hw/core/boards.h"
#include "hw/char/serial.h"
#include "hw/char/serial-mm.h"
#include "hw/intc/arm_gic.h"
#include "hw/misc/unimp.h"
#include "hw/core/loader.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/cpu.h"
#include "system/reset.h"
#include "system/runstate.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "system/blockdev.h"
#include "hw/sd/sdhci.h"
#include "hw/sd/sd.h"
#include "ui/input.h"

#if defined(QEMU_IN_TREE) || __has_include("hw/arm/rtd2841.h")
#include "hw/arm/rtd2841.h"
#else
#include "rtd2841.h"
#endif

static void rtd2841_soc_init(Object *obj)
{
    RTD2841SocState *s = RTD2841_SOC(obj);
    int i;

    /* Initialize 4x Cortex-A53 CPU cores */
    for (i = 0; i < RTD2841_NUM_CPUS; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cpu[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
    }

    /* Initialize ARM GIC-400 (GICv2) interrupt controller */
    object_initialize_child(obj, "gic", &s->gic, TYPE_ARM_GIC);
}

static void rtd2841_soc_realize(DeviceState *dev, Error **errp)
{
    RTD2841SocState *s = RTD2841_SOC(dev);
    DeviceState *gicdev;
    SysBusDevice *gicsbd;
    int i;

    /*
     * 1. Configure and realize Cortex-A53 quad-core CPUs
     */
    for (i = 0; i < RTD2841_NUM_CPUS; i++) {
        Object *cpuobj = OBJECT(&s->cpu[i]);

        /*
         * Force Cortex-A53 into AArch32 mode (32-bit execution state).
         * Matches the Realtek RTD2841 firmware architecture and kernel config.
         */
        if (object_property_find(cpuobj, "aarch64")) {
            object_property_set_bool(cpuobj, "aarch64", false, &error_fatal);
        }

        /* Set MPIDR multiprocessor affinity: Cluster 0, Core ID i */
        object_property_set_int(cpuobj, "mp-affinity",
                                arm_build_mp_affinity(i, RTD2841_NUM_CPUS),
                                &error_fatal);

        /* Reset exception vectors at 0x00000000 (Low Vectors) */
        if (object_property_find(cpuobj, "reset-hivecs")) {
            object_property_set_bool(cpuobj, "reset-hivecs", false, &error_fatal);
        }

        /*
         * Secondary cores must start in powered-off state under PSCI.
         * Core 0 boots directly; cores 1..3 wait for PSCI CPU_ON invocation.
         */
        if (i > 0) {
            object_property_set_bool(cpuobj, "start-powered-off", true,
                                     &error_fatal);
        }

        /* Realize core */
        if (!qdev_realize(DEVICE(cpuobj), NULL, errp)) {
            return;
        }
    }

    /*
     * 2. Configure and realize ARM GIC-400 (GICv2)
     * Realtek RTD2841 integrates GIC-400 configured for 256 interrupt lines.
     */
    gicdev = DEVICE(&s->gic);
    gicsbd = SYS_BUS_DEVICE(&s->gic);

    qdev_prop_set_uint32(gicdev, "num-cpu", RTD2841_NUM_CPUS);
    qdev_prop_set_uint32(gicdev, "num-irq", RTD2841_GIC_NUM_IRQS);
    qdev_prop_set_uint32(gicdev, "revision", 2);
    qdev_prop_set_bit(gicdev, "has-security-extensions", false);

    if (!sysbus_realize(gicsbd, errp)) {
        return;
    }

    /* Map GIC Distributor (GICD) at 0x18081000 */
    sysbus_mmio_map(gicsbd, 0, RTD2841_GIC_DIST_BASE);

    /* Map GIC CPU Interface (GICC) at 0x18082000 */
    sysbus_mmio_map(gicsbd, 1, RTD2841_GIC_CPU_BASE);

    /* Wire GIC interrupt outputs to CPU cores */
    for (i = 0; i < RTD2841_NUM_CPUS; i++) {
        DeviceState *cpudev = DEVICE(&s->cpu[i]);

        sysbus_connect_irq(gicsbd, i,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(gicsbd, i + RTD2841_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(gicsbd, i + 2 * RTD2841_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(gicsbd, i + 3 * RTD2841_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));

        /*
         * Connect ARM generic architecture timer interrupts to GIC PPIs:
         * - GTIMER_PHYS (Non-secure EL1 physical timer): INTID 30 (PPI 14)
         * - GTIMER_VIRT (Virtual timer):                 INTID 27 (PPI 11)
         * - GTIMER_HYP  (Hypervisor EL2 physical timer): INTID 26 (PPI 10)
         * - GTIMER_SEC  (Secure EL1 physical timer):     INTID 29 (PPI 13)
         */
        qdev_connect_gpio_out(cpudev, GTIMER_PHYS,
                              qdev_get_gpio_in(gicdev,
                                  RTD2841_GIC_PPI_TO_GPIO(i, RTD2841_IRQ_PPI_NONSEC_PHYS_TMR)));
        qdev_connect_gpio_out(cpudev, GTIMER_VIRT,
                              qdev_get_gpio_in(gicdev,
                                  RTD2841_GIC_PPI_TO_GPIO(i, RTD2841_IRQ_PPI_VIRT_TIMER)));
        qdev_connect_gpio_out(cpudev, GTIMER_HYP,
                              qdev_get_gpio_in(gicdev,
                                  RTD2841_GIC_PPI_TO_GPIO(i, RTD2841_IRQ_PPI_HYP_TIMER)));
        qdev_connect_gpio_out(cpudev, GTIMER_SEC,
                              qdev_get_gpio_in(gicdev,
                                  RTD2841_GIC_PPI_TO_GPIO(i, RTD2841_IRQ_PPI_SEC_PHYS_TIMER)));
    }

    /*
     * 3. 16550A UART0 mapping
     * Register Base: 0x18062300, 32-bit registers (4-byte stride: shift = 2)
     * Clock: 27 MHz (baud base = 27000000 / 16 = 1687500)
     * IRQ: SPI 25 (GIC INTID 57)
     * Wired to serial_hd(0) for stdio console communication
     */
    serial_mm_init(get_system_memory(),
                   RTD2841_UART0_BASE,
                   RTD2841_UART0_REG_SHIFT,
                   qdev_get_gpio_in(gicdev, RTD2841_GIC_SPI_TO_GPIO(RTD2841_UART0_SPI)),
                   RTD2841_UART0_BAUDBASE,
                   serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);

    /*
     * 4. Stub hardware peripheral apertures
     * Realtek RBUS memory space covers internal peripheral registers.
     * We map a comprehensive 128MB background aperture (0x18000000-0x20000000)
     * at priority -1000 so unmodeled register reads return 0 without raising bus aborts.
     */
    create_unimplemented_device("rtd2841.soc_peripherals", 0x18000000, 0x08000000);
    create_unimplemented_device("rtd2841.tmu", RTD2841_TMU_BASE, RTD2841_TMU_SIZE);
    create_unimplemented_device("rtd2841.sysreg", RTD2841_SYSREG_BASE, RTD2841_SYSREG_SIZE);
    /*
     * Realtek eMMC / SDHCI Host Controller
     * Register Base: 0x18010800 (RTD2841_EMMC_BASE)
     * IRQ: SPI 28 (GIC INTID 60)
     */
    {
        DeviceState *sdhsidev = qdev_new(TYPE_SYSBUS_SDHCI);
        DriveInfo *di;
        BlockBackend *blk;
        DeviceState *carddev;

        sysbus_realize_and_unref(SYS_BUS_DEVICE(sdhsidev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(sdhsidev), 0, RTD2841_EMMC_BASE);
        sysbus_connect_irq(SYS_BUS_DEVICE(sdhsidev), 0,
                           qdev_get_gpio_in(gicdev, RTD2841_GIC_SPI_TO_GPIO(RTD2841_EMMC_SPI)));

        di = drive_get(IF_SD, 0, 0);
        blk = di ? blk_by_legacy_dinfo(di) : NULL;
        carddev = qdev_new(TYPE_SD_CARD);
        qdev_prop_set_drive_err(carddev, "drive", blk, &error_fatal);
        qdev_realize_and_unref(carddev, qdev_get_child_bus(sdhsidev, "sd-bus"), &error_fatal);
    }
    create_unimplemented_device("rtd2841.sdio", RTD2841_SDIO_BASE, RTD2841_SDIO_SIZE);
    create_unimplemented_device("rtd2841.uart1_3", RTD2841_UART1_3_BASE, RTD2841_UART1_3_SIZE);
    rtd2841_timer_create(RTD2841_TIMER_BASE,
                         qdev_get_gpio_in(gicdev, RTD2841_GIC_SPI_TO_GPIO(RTD2841_TIMER_SPI)));
    create_unimplemented_device("rtd2841.gpu", RTD2841_GPU_BASE, RTD2841_GPU_SIZE);
    create_unimplemented_device("rtd2841.arm_tsg", RTD2841_ARM_TSG_BASE, RTD2841_ARM_TSG_SIZE);
    rtd2841_wdog_create(RTD2841_WDOG_BASE);
    create_unimplemented_device("rtd2841.scu", RTD2841_SCU_BASE, RTD2841_SCU_SIZE);
}

static void rtd2841_soc_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    (void)data;
    dc->realize = rtd2841_soc_realize;
    dc->user_creatable = false;
}

static void rtd2841_fb_invalidate(void *opaque)
{
    RTD2841MachineState *ms = opaque;
    ms->fb_invalidate = true;
}

static bool rtd2841_fb_update(void *opaque)
{
    RTD2841MachineState *ms = opaque;
    uint8_t *ptr;
    DisplaySurface *surface;

    ptr = memory_region_get_ram_ptr(&ms->fb_ram);
    if (!ptr) {
        return true;
    }

    surface = qemu_console_surface(ms->console);
    if (!surface || surface_data(surface) != ptr) {
        surface = qemu_create_displaysurface_from(RTD2841_FB_WIDTH,
                                                  RTD2841_FB_HEIGHT,
                                                  PIXMAN_x8r8g8b8,
                                                  RTD2841_FB_STRIDE,
                                                  ptr);
        dpy_gfx_replace_surface(ms->console, surface);
    }

    ms->fb_invalidate = false;
    dpy_gfx_update(ms->console, 0, 0, RTD2841_FB_WIDTH, RTD2841_FB_HEIGHT);
    return true;
}

static const GraphicHardwarePageOps rtd2841_fb_ops = {
    .invalidate = rtd2841_fb_invalidate,
    .gfx_update = rtd2841_fb_update,
};

static void rtd2841_kbd_event(DeviceState *dev, QemuConsole *src, QemuInputEvent *evt)
{
    (void)dev;
    (void)src;
    if (evt->type != INPUT_EVENT_KIND_KEY) {
        return;
    }
    if (!evt->key.down) {
        return;
    }

    int qcode = qemu_input_linux_to_qcode(evt->key.key);
    char c = 0;
    const char *seq = NULL;

    switch (qcode) {
    case Q_KEY_CODE_1: c = '1'; break;
    case Q_KEY_CODE_2: c = '2'; break;
    case Q_KEY_CODE_3: c = '3'; break;
    case Q_KEY_CODE_4: c = '4'; break;
    case Q_KEY_CODE_5: c = '5'; break;
    case Q_KEY_CODE_6: c = '6'; break;
    case Q_KEY_CODE_LEFT:
    case Q_KEY_CODE_H:
        seq = "\033[D";
        break;
    case Q_KEY_CODE_RIGHT:
    case Q_KEY_CODE_L:
        seq = "\033[C";
        break;
    case Q_KEY_CODE_UP:
    case Q_KEY_CODE_K:
        seq = "\033[A";
        break;
    case Q_KEY_CODE_DOWN:
    case Q_KEY_CODE_J:
        seq = "\033[B";
        break;
    case Q_KEY_CODE_RET:
    case Q_KEY_CODE_KP_ENTER:
        c = '\r';
        break;
    case Q_KEY_CODE_B: c = 'b'; break;
    case Q_KEY_CODE_T: c = 't'; break;
    case Q_KEY_CODE_C: c = 'c'; break;
    case Q_KEY_CODE_Q: c = 'q'; break;
    case Q_KEY_CODE_BACKSPACE: c = 0x7f; break;
    default:
        break;
    }

    Chardev *chr = serial_hd(0);
    if (chr) {
        if (seq) {
            qemu_chr_be_write(chr, (const uint8_t *)seq, strlen(seq));
        } else if (c) {
            qemu_chr_be_write(chr, (const uint8_t *)&c, 1);
        }
    }
}

static const QemuInputHandler rtd2841_kbd_handler = {
    .name  = "rtd2841-kbd",
    .mask  = INPUT_EVENT_MASK_KEY,
    .event = rtd2841_kbd_event,
};

static void rtd2841_tv030_init(MachineState *machine)
{
    RTD2841MachineState *ms = RTD2841_MACHINE(machine);

    /* Validate RAM allocation */
    if (machine->ram_size < RTD2841_RAM_SIZE) {
        error_report("WARNING: RTD2841 tv030 profile expects 1024 MB RAM (configured: %" PRIu64 " MB)",
                     machine->ram_size / MiB);
    }

    /* Instantiate and realize RTD2841 SoC */
    object_initialize_child(OBJECT(machine), "soc", &ms->soc, TYPE_RTD2841_SOC);
    sysbus_realize(SYS_BUS_DEVICE(&ms->soc), &error_fatal);

    /* Map 1GB Physical DRAM at 0x00000000 with priority -1 (background RAM) */
    memory_region_add_subregion_overlap(get_system_memory(), RTD2841_RAM_BASE, machine->ram, -1);

    /*
     * Simple Framebuffer Carveout: 8 MB physical DRAM buffer at 0x3E000000.
     * Mapped with priority 1 to overlay background RAM at 992 MB offset.
     */
    memory_region_init_ram(&ms->fb_ram, NULL, "rtd2841.fb",
                           RTD2841_FB_SIZE, &error_fatal);
    memory_region_add_subregion_overlap(get_system_memory(), RTD2841_FB_BASE,
                                        &ms->fb_ram, 1);

    /* Initialize QEMU GraphicConsole (1920x1080 @ 32 bpp ARGB8888) */
    ms->console = graphic_console_init(NULL, 0, &rtd2841_fb_ops, ms);
    {
        uint8_t *fb_ptr = memory_region_get_ram_ptr(&ms->fb_ram);
        uint32_t *pixels = (uint32_t *)fb_ptr;
        size_t total_pixels = RTD2841_FB_WIDTH * RTD2841_FB_HEIGHT;
        size_t p;

        /* Default initialization: opaque dark slate background */
        for (p = 0; p < total_pixels; p++) {
            pixels[p] = 0xFF141824;
        }

        DisplaySurface *surface = qemu_create_displaysurface_from(
            RTD2841_FB_WIDTH, RTD2841_FB_HEIGHT,
            PIXMAN_x8r8g8b8, RTD2841_FB_STRIDE, fb_ptr);
        dpy_gfx_replace_surface(ms->console, surface);
    }

    /* Register Cocoa GUI Keyboard input handler (translates window keys to UART serial) */
    (void)qemu_input_handler_register(NULL, &rtd2841_kbd_handler);

    /* Configure ARM Linux bootloader parameters */
    ms->boot_info.ram_size = machine->ram_size;
    ms->boot_info.board_id = -1; /* -1 tells QEMU to use Device Tree booting */
    ms->boot_info.loader_start = RTD2841_RAM_BASE;
    ms->boot_info.psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    ms->boot_info.smp_bootreg_addr = RTD2841_SMP_CPU1_JUMP_ADDR;

    if (machine->firmware) {
        /* Raw bootloader / firmware image loaded via -bios */
        int fw_size = load_image_targphys(machine->firmware, RTD2841_RAM_BASE,
                                          machine->ram_size, NULL);
        if (fw_size < 0) {
            error_report("Failed to load firmware binary '%s'", machine->firmware);
            exit(1);
        }
    } else {
        /* Standard kernel image loaded via -kernel */
        arm_load_kernel(ARM_CPU(&ms->soc.cpu[0]), machine, &ms->boot_info);
    }
}

static void rtd2841_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    (void)data;
    mc->desc = "Realtek RTD2841 SoC / TCL Smart TV (board tv030)";
    mc->alias = "rtd2841";
    mc->init = rtd2841_tv030_init;
    mc->max_cpus = RTD2841_NUM_CPUS;
    mc->min_cpus = 1;
    mc->default_cpus = RTD2841_NUM_CPUS;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a53");
    mc->default_ram_size = RTD2841_RAM_SIZE;
    mc->default_ram_id = "rtd2841.ram";
    mc->ignore_memory_transaction_failures = true;
}

static const TypeInfo rtd2841_types[] = {
    {
        .name = TYPE_RTD2841_SOC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RTD2841SocState),
        .instance_init = rtd2841_soc_init,
        .class_init = rtd2841_soc_class_init,
    },
    {
        .name = TYPE_RTD2841_MACHINE,
        .parent = TYPE_MACHINE,
        .instance_size = sizeof(RTD2841MachineState),
        .class_init = rtd2841_machine_class_init,
    },
};

DEFINE_TYPES(rtd2841_types)
