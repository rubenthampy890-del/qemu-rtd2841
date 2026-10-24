/*
 * Realtek RTD2841 SoC & TCL Smart TV Board Model (tv030)
 *
 * Emulates the Realtek RTD2841 (RTK284x) SoC featuring:
 * - Quad-Core ARM Cortex-A53 running in AArch32 mode (tv030 chassis)
 * - 1 GB Physical DRAM mapped at 0x00000000
 * - ARM Generic Interrupt Controller (GIC-400 / GICv2) at 0x18081000 (256 IRQs)
 * - 16550A-compatible UART0 console at 0x18062300 (mapped to serial stdio)
 * - Realtek Gigabit Ethernet MAC (RTL8168GU) at 0x18016000 (GIC SPI 20 / ID 52)
 * - Realtek generic-sdhci eMMC storage controller at 0x18010800 (GIC SPI 18)
 * - PSCI firmware SMP power-state coordination interface (SMC conduit)
 * - Secondary CPU start-powered-off coordination and jump mailbox at 0x504
 * - Stubbed memory apertures for hardware peripherals (TMU, SYSREG, SDIO, etc.)
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
#if __has_include("system/address-spaces.h")
#include "system/reset.h"
#include "system/runstate.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "system/blockdev.h"
#else
#include "sysemu/reset.h"
#include "sysemu/runstate.h"
#include "sysemu/address-spaces.h"
#include "sysemu/sysemu.h"
#include "sysemu/blockdev.h"
#endif
#include "hw/sd/sdhci.h"
#include "hw/sd/sd.h"
#include "ui/input.h"
#include "hw/core/irq.h"
#include "net/net.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"

#if defined(QEMU_IN_TREE) || __has_include("hw/arm/rtd2841.h")
#include "hw/arm/rtd2841.h"
#else
#include "rtd2841.h"
#endif

/* ------------------------------------------------------------------ */
/* Realtek Gigabit Ethernet Controller (RTL8168GU)                     */
/* ------------------------------------------------------------------ */

#define RTK_DESC_OWN        (1U << 31)
#define RTK_DESC_EOR        (1U << 30)
#define RTK_DESC_FS         (1U << 29)
#define RTK_DESC_LS         (1U << 28)
#define RTK_DESC_LEN_MASK   0x00003fffU

static void rtd2841_eth_update_irq(RTD2841EthState *s)
{
    int level = !!(s->intr_status & s->intr_mask);
    qemu_set_irq(s->irq, level);
}

static void rtd2841_eth_tx(RTD2841EthState *s)
{
    uint8_t pkt_buf[4096];
    size_t pkt_len = 0;
    int max_desc = RTD2841_ETH_NUM_TX_DESC;
    bool sent_any = false;

    if (!(s->chipcmd & 0x04) || !s->tx_desc_base) {
        return;
    }

    while (max_desc-- > 0) {
        hwaddr desc_addr;
        uint32_t opts1, opts1_le;
        uint64_t dma_addr;
        uint32_t frag_len;

        s->cur_tx %= RTD2841_ETH_NUM_TX_DESC;
        desc_addr = s->tx_desc_base + (s->cur_tx * 16);
        if (desc_addr == 0 || desc_addr >= RTD2841_RAM_SIZE - 16) {
            break;
        }

        address_space_read(&address_space_memory, desc_addr,
                           MEMTXATTRS_UNSPECIFIED, &opts1, 4);
        opts1 = le32_to_cpu(opts1);

        if (!(opts1 & RTK_DESC_OWN)) {
            break;
        }

        address_space_read(&address_space_memory, desc_addr + 8,
                           MEMTXATTRS_UNSPECIFIED, &dma_addr, 8);
        dma_addr = le64_to_cpu(dma_addr);

        frag_len = opts1 & RTK_DESC_LEN_MASK;

        if (dma_addr == 0 || dma_addr >= RTD2841_RAM_SIZE ||
            frag_len > RTD2841_RAM_SIZE - dma_addr) {
            break;
        }
        if (opts1 & RTK_DESC_FS) {
            pkt_len = 0;
        }

        if (pkt_len + frag_len <= sizeof(pkt_buf)) {
            address_space_read(&address_space_memory, dma_addr,
                               MEMTXATTRS_UNSPECIFIED,
                               pkt_buf + pkt_len, frag_len);
            pkt_len += frag_len;
        }

        if ((opts1 & RTK_DESC_LS) && pkt_len > 0) {
            if (s->nic) {
                qemu_send_packet(qemu_get_queue(s->nic), pkt_buf, pkt_len);
                sent_any = true;
            }
            pkt_len = 0;
        }

        opts1 &= ~RTK_DESC_OWN;
        opts1_le = cpu_to_le32(opts1);
        address_space_write(&address_space_memory, desc_addr,
                            MEMTXATTRS_UNSPECIFIED, &opts1_le, 4);

        if (opts1 & RTK_DESC_EOR) {
            s->cur_tx = 0;
        } else {
            s->cur_tx = (s->cur_tx + 1) % RTD2841_ETH_NUM_TX_DESC;
        }
    }

    if (sent_any) {
        s->intr_status |= 0x0004; /* TxOK */
        rtd2841_eth_update_irq(s);
    }
}

static bool rtd2841_eth_can_receive(NetClientState *nc)
{
    RTD2841EthState *s = qemu_get_nic_opaque(nc);
    hwaddr desc_addr;
    uint32_t opts1;

    if (!(s->chipcmd & 0x08) || !s->rx_desc_base) {
        return false;
    }

    s->cur_rx %= RTD2841_ETH_NUM_RX_DESC;
    desc_addr = s->rx_desc_base + (s->cur_rx * 16);
    if (desc_addr == 0 || desc_addr >= RTD2841_RAM_SIZE - 16) {
        return false;
    }
    address_space_read(&address_space_memory, desc_addr,
                       MEMTXATTRS_UNSPECIFIED, &opts1, 4);
    opts1 = le32_to_cpu(opts1);

    return (opts1 & RTK_DESC_OWN) != 0;
}

static ssize_t rtd2841_eth_receive(NetClientState *nc, const uint8_t *buf,
                                   size_t size)
{
    RTD2841EthState *s = qemu_get_nic_opaque(nc);
    hwaddr desc_addr;
    uint32_t opts1;
    uint64_t dma_addr;
    uint32_t eor;
    uint32_t opts1_le, opts2_le;
    uint32_t buf_sz;

    if (!(s->chipcmd & 0x08) || !s->rx_desc_base) {
        return -1;
    }
    if (size == 0) {
        return 0;
    }

    s->cur_rx %= RTD2841_ETH_NUM_RX_DESC;
    desc_addr = s->rx_desc_base + (s->cur_rx * 16);
    if (desc_addr == 0 || desc_addr >= RTD2841_RAM_SIZE - 16) {
        return -1;
    }
    address_space_read(&address_space_memory, desc_addr,
                       MEMTXATTRS_UNSPECIFIED, &opts1, 4);
    opts1 = le32_to_cpu(opts1);

    if (!(opts1 & RTK_DESC_OWN)) {
        s->intr_status |= 0x0080; /* Tx/Rx DescUnavail */
        rtd2841_eth_update_irq(s);
        return 0;
    }

    /* Clamp incoming packet length to descriptor buffer size */
    buf_sz = opts1 & RTK_DESC_LEN_MASK;
    if (buf_sz == 0) {
        buf_sz = s->rx_max_size ? s->rx_max_size : 16384;
    }
    if (buf_sz > RTK_DESC_LEN_MASK - 4) {
        buf_sz = RTK_DESC_LEN_MASK - 4;
    }
    if (size > buf_sz) {
        size = buf_sz;
    }

    address_space_read(&address_space_memory, desc_addr + 8,
                       MEMTXATTRS_UNSPECIFIED, &dma_addr, 8);
    dma_addr = le64_to_cpu(dma_addr);

    if (dma_addr == 0 || dma_addr >= RTD2841_RAM_SIZE ||
        size > RTD2841_RAM_SIZE - dma_addr) {
        return 0;
    }

    address_space_write(&address_space_memory, dma_addr,
                        MEMTXATTRS_UNSPECIFIED, buf, size);

    eor = opts1 & RTK_DESC_EOR;
    /* Realtek driver subtracts 4 for FCS: pkt_size = (status & 0x3fff) - 4 */
    opts1 = eor | RTK_DESC_FS | RTK_DESC_LS | ((size + 4) & RTK_DESC_LEN_MASK);
    opts1_le = cpu_to_le32(opts1);
    address_space_write(&address_space_memory, desc_addr,
                        MEMTXATTRS_UNSPECIFIED, &opts1_le, 4);

    opts2_le = 0;
    address_space_write(&address_space_memory, desc_addr + 4,
                        MEMTXATTRS_UNSPECIFIED, &opts2_le, 4);

    if (eor) {
        s->cur_rx = 0;
    } else {
        s->cur_rx = (s->cur_rx + 1) % RTD2841_ETH_NUM_RX_DESC;
    }

    s->intr_status |= 0x0001; /* RxOK */
    rtd2841_eth_update_irq(s);

    return size;
}

static void rtd2841_eth_set_link_status(NetClientState *nc)
{
    RTD2841EthState *s = qemu_get_nic_opaque(nc);

    if (nc->link_down) {
        s->phy_regs[1] &= ~0x0004; /* Link Down */
    } else {
        s->phy_regs[1] |= 0x0004;  /* Link Up */
    }
    s->intr_status |= 0x0020; /* LinkChg */
    rtd2841_eth_update_irq(s);
}

static void rtd2841_eth_cleanup(NetClientState *nc)
{
    RTD2841EthState *s = qemu_get_nic_opaque(nc);
    s->nic = NULL;
}

static uint64_t rtd2841_eth_read(void *opaque, hwaddr addr, unsigned size)
{
    RTD2841EthState *s = opaque;
    uint64_t val = 0;

    /* Handle MAC address registers: 0x00 .. 0x05 */
    if (addr < 6) {
        int copy_len = MIN((int)size, 6 - (int)addr);
        for (int i = 0; i < copy_len; i++) {
            val |= ((uint64_t)s->mac[addr + i]) << (i * 8);
        }
        return val;
    }

    /* Handle Multicast filter registers: 0x08 .. 0x0F */
    if (addr >= 0x08 && addr + size <= 0x10) {
        for (unsigned i = 0; i < size; i++) {
            val |= ((uint64_t)s->mar[addr - 0x08 + i]) << (i * 8);
        }
        return val;
    }

    switch (addr) {
    case 0x20: /* TxDescStartAddrLow */
        val = (uint32_t)s->tx_desc_base;
        break;
    case 0x24: /* TxDescStartAddrHigh */
        val = (uint32_t)(s->tx_desc_base >> 32);
        break;
    case 0x37: /* ChipCmd */
        val = s->chipcmd;
        break;
    case 0x38: /* TxPoll */
        val = 0;
        break;
    case 0x3C: /* IntrMask */
        val = s->intr_mask;
        break;
    case 0x3E: /* IntrStatus */
        val = s->intr_status;
        break;
    case 0x40: /* TxConfig */
        val = s->tx_config;
        break;
    case 0x44: /* RxConfig */
        val = s->rx_config;
        break;
    case 0x4C: /* RxMissed */
        val = 0;
        break;
    case 0x50: /* Cfg9346 */
        val = s->cfg9346;
        break;
    case 0x51 ... 0x56: /* Config0..Config5 */
        val = s->config[addr - 0x51];
        break;
    case 0x5C: /* MultiIntr */
        val = s->multi_intr;
        break;
    case 0x60: /* PHYAR */
        val = s->phyar;
        break;
    case 0x64: /* CSIDR */
        val = s->csidr;
        break;
    case 0x68: /* CSIAR */
        val = s->csiar;
        break;
    case 0x6C: /* PHYstatus */
        val = 0x13; /* _1000bpsF | LinkStatus | FullDup */
        break;
    case 0x70: /* ERIDR */
        val = s->eridr;
        break;
    case 0x74: /* ERIAR */
        val = s->eriar;
        break;
    case 0x80: /* EPHYAR */
        val = s->ephyar;
        break;
    case 0xD3: /* MCU */
        val = 0x32; /* LINK_LIST_RDY | RXTX_EMPTY */
        break;
    case 0xDA: /* RxMaxSize */
        val = s->rx_max_size;
        break;
    case 0xE0: /* CPlusCmd */
        val = s->cplus_cmd;
        break;
    case 0xE2: /* IntrMitigate */
        val = s->intr_mitigate;
        break;
    case 0xE4: /* RxDescAddrLow */
        val = (uint32_t)s->rx_desc_base;
        break;
    case 0xE8: /* RxDescAddrHigh */
        val = (uint32_t)(s->rx_desc_base >> 32);
        break;
    case 0xF0: /* MISC */
        val = s->misc;
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

static void rtd2841_eth_reset(DeviceState *dev);

static void rtd2841_eth_write(void *opaque, hwaddr addr, uint64_t val,
                              unsigned size)
{
    RTD2841EthState *s = opaque;

    /* Handle MAC address registers: 0x00 .. 0x05 */
    if (addr < 6) {
        int copy_len = MIN((int)size, 6 - (int)addr);
        for (int i = 0; i < copy_len; i++) {
            s->mac[addr + i] = (val >> (i * 8)) & 0xff;
        }
        return;
    }

    /* Handle Multicast filter registers: 0x08 .. 0x0F */
    if (addr >= 0x08 && addr + size <= 0x10) {
        for (unsigned i = 0; i < size; i++) {
            s->mar[addr - 0x08 + i] = (val >> (i * 8)) & 0xff;
        }
        return;
    }

    switch (addr) {
    case 0x20: /* TxDescStartAddrLow */
        s->tx_desc_base = (s->tx_desc_base & 0xffffffff00000000ULL) |
                          (uint32_t)val;
        s->cur_tx = 0;
        break;
    case 0x24: /* TxDescStartAddrHigh */
        s->tx_desc_base = (s->tx_desc_base & 0x00000000ffffffffULL) |
                          ((uint64_t)(uint32_t)val << 32);
        break;
    case 0x37: /* ChipCmd */
        if (val & 0x10) { /* CmdReset: self-clearing */
            rtd2841_eth_reset(DEVICE(s));
        } else {
            s->chipcmd = (uint8_t)val & 0x0C; /* Preserve RxEnb / TxEnb */
        }
        if ((s->chipcmd & 0x08) && s->nic) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        break;
    case 0x38: /* TxPoll */
        if (val & 0xC0) { /* NPQ | HPQ */
            rtd2841_eth_tx(s);
        }
        break;
    case 0x3C: /* IntrMask */
        s->intr_mask = (uint16_t)val;
        rtd2841_eth_update_irq(s);
        if (s->nic) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        break;
    case 0x3E: /* IntrStatus */
        s->intr_status &= ~(uint16_t)val;
        rtd2841_eth_update_irq(s);
        if (s->nic) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        break;
    case 0x40: /* TxConfig */
        /* Preserve 8168GU identification (0x50900000) & TXCFG_EMPTY (0x800) */
        s->tx_config = 0x50900800U | ((uint32_t)val & 0x000ff7ffU);
        break;
    case 0x44: /* RxConfig */
        s->rx_config = (uint32_t)val;
        break;
    case 0x4C: /* RxMissed */
        break;
    case 0x50: /* Cfg9346 */
        s->cfg9346 = (uint8_t)val;
        break;
    case 0x51 ... 0x56: /* Config0..Config5 */
        s->config[addr - 0x51] = (uint8_t)val;
        break;
    case 0x5C: /* MultiIntr */
        s->multi_intr = (uint16_t)val;
        break;
    case 0x60: /* PHYAR */
        if (val & 0x80000000U) {
            /* Write command: clear bit 31 immediately */
            uint32_t reg = (val >> 16) & 0x1f;
            s->phy_regs[reg] = val & 0xffff;
            s->phyar = val & 0x7fffffffU;
        } else {
            /* Read command: set bit 31 immediately and return reg data */
            uint32_t reg = (val >> 16) & 0x1f;
            uint16_t data = s->phy_regs[reg];
            s->phyar = 0x80000000U | (val & 0x001f0000U) | data;
        }
        break;
    case 0x64: /* CSIDR */
        s->csidr = (uint32_t)val;
        break;
    case 0x68: /* CSIAR */
        if (val & 0x80000000U) {
            s->csiar = val & ~0x80000000U;
        } else {
            s->csiar = val | 0x80000000U;
        }
        break;
    case 0x70: /* ERIDR */
        s->eridr = (uint32_t)val;
        break;
    case 0x74: /* ERIAR */
        if (val & 0x80000000U) {
            s->eriar = val & ~0x80000000U;
        } else {
            s->eriar = val | 0x80000000U;
        }
        break;
    case 0x80: /* EPHYAR */
        if (val & 0x80000000U) {
            s->ephyar = val & ~0x80000000U;
        } else {
            s->ephyar = val | 0x80000000U;
        }
        break;
    case 0xDA: /* RxMaxSize */
        s->rx_max_size = (uint16_t)val;
        break;
    case 0xE0: /* CPlusCmd */
        s->cplus_cmd = (uint16_t)val;
        break;
    case 0xE2: /* IntrMitigate */
        s->intr_mitigate = (uint16_t)val;
        break;
    case 0xE4: /* RxDescAddrLow */
        s->rx_desc_base = (s->rx_desc_base & 0xffffffff00000000ULL) |
                          (uint32_t)val;
        s->cur_rx = 0;
        break;
    case 0xE8: /* RxDescAddrHigh */
        s->rx_desc_base = (s->rx_desc_base & 0x00000000ffffffffULL) |
                          ((uint64_t)(uint32_t)val << 32);
        break;
    case 0xF0: /* MISC */
        s->misc = (uint32_t)val;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps rtd2841_eth_ops = {
    .read = rtd2841_eth_read,
    .write = rtd2841_eth_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static NetClientInfo rtd2841_eth_net_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = rtd2841_eth_can_receive,
    .receive = rtd2841_eth_receive,
    .link_status_changed = rtd2841_eth_set_link_status,
    .cleanup = rtd2841_eth_cleanup,
};

static void rtd2841_eth_reset(DeviceState *dev)
{
    RTD2841EthState *s = RTD2841_ETH(dev);

    s->chipcmd = 0x01; /* RxBufEmpty */
    s->intr_mask = 0;
    s->intr_status = 0;
    s->tx_config = 0x50900800U; /* Realtek 8168GU + TXCFG_EMPTY */
    s->rx_config = 0;
    s->cfg9346 = 0;
    memset(s->config, 0, sizeof(s->config));
    s->multi_intr = 0;
    s->phyar = 0;
    s->eriar = 0;
    s->eridr = 0;
    s->ephyar = 0;
    s->csiar = 0;
    s->csidr = 0;
    s->rx_max_size = 16384;
    s->cplus_cmd = 0;
    s->intr_mitigate = 0;
    s->misc = 0;
    s->tx_desc_base = 0;
    s->rx_desc_base = 0;
    s->cur_tx = 0;
    s->cur_rx = 0;

    memset(s->phy_regs, 0, sizeof(s->phy_regs));
    s->phy_regs[0] = 0x1140; /* MII_BMCR */
    s->phy_regs[1] = 0x796d; /* MII_BMSR */
    s->phy_regs[2] = 0x001c; /* MII_PHYSID1 */
    s->phy_regs[3] = 0xc912; /* MII_PHYSID2 */
    s->phy_regs[4] = 0x01e1; /* MII_ADVERTISE */
    s->phy_regs[5] = 0xcde1; /* MII_LPA */
    s->phy_regs[9] = 0x0e00; /* MII_CTRL1000 */
    s->phy_regs[10] = 0x3800; /* MII_STAT1000 */
    s->phy_regs[15] = 0x3000; /* MII_ESTATUS */
    /* rtl_phy_status_cond checks (reg 0x10 & 3) == 3 */
    s->phy_regs[16] = 0x0013;

    qemu_set_irq(s->irq, 0);
}

static void rtd2841_eth_realize(DeviceState *dev, Error **errp)
{
    RTD2841EthState *s = RTD2841_ETH(dev);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    memcpy(s->mac, s->conf.macaddr.a, 6);

    s->nic = qemu_new_nic(&rtd2841_eth_net_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id,
                          &dev->mem_reentrancy_guard, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static void rtd2841_eth_init(Object *obj)
{
    RTD2841EthState *s = RTD2841_ETH(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->mmio, obj, &rtd2841_eth_ops, s,
                          TYPE_RTD2841_ETH, RTD2841_ETH_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);
}

static const Property rtd2841_eth_properties[] = {
    DEFINE_NIC_PROPERTIES(RTD2841EthState, conf),
};

static const VMStateDescription vmstate_rtd2841_eth = {
    .name = "rtd2841_eth",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8_ARRAY(mac, RTD2841EthState, 6),
        VMSTATE_UINT8_ARRAY(mar, RTD2841EthState, 8),
        VMSTATE_UINT8(chipcmd, RTD2841EthState),
        VMSTATE_UINT16(intr_mask, RTD2841EthState),
        VMSTATE_UINT16(intr_status, RTD2841EthState),
        VMSTATE_UINT32(tx_config, RTD2841EthState),
        VMSTATE_UINT32(rx_config, RTD2841EthState),
        VMSTATE_UINT8(cfg9346, RTD2841EthState),
        VMSTATE_UINT8_ARRAY(config, RTD2841EthState, 6),
        VMSTATE_UINT16(multi_intr, RTD2841EthState),
        VMSTATE_UINT32(phyar, RTD2841EthState),
        VMSTATE_UINT32(eriar, RTD2841EthState),
        VMSTATE_UINT32(eridr, RTD2841EthState),
        VMSTATE_UINT32(ephyar, RTD2841EthState),
        VMSTATE_UINT32(csiar, RTD2841EthState),
        VMSTATE_UINT32(csidr, RTD2841EthState),
        VMSTATE_UINT16(rx_max_size, RTD2841EthState),
        VMSTATE_UINT16(cplus_cmd, RTD2841EthState),
        VMSTATE_UINT16(intr_mitigate, RTD2841EthState),
        VMSTATE_UINT32(misc, RTD2841EthState),
        VMSTATE_UINT64(tx_desc_base, RTD2841EthState),
        VMSTATE_UINT64(rx_desc_base, RTD2841EthState),
        VMSTATE_UINT32(cur_tx, RTD2841EthState),
        VMSTATE_UINT32(cur_rx, RTD2841EthState),
        VMSTATE_UINT16_ARRAY(phy_regs, RTD2841EthState, 32),
        VMSTATE_END_OF_LIST()
    }
};

static void rtd2841_eth_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    (void)data;
    dc->realize = rtd2841_eth_realize;
    device_class_set_props(dc, rtd2841_eth_properties);
    device_class_set_legacy_reset(dc, rtd2841_eth_reset);
    dc->vmsd = &vmstate_rtd2841_eth;
    dc->desc = "Realtek RTD2841 Gigabit Ethernet Controller (RTL8168GU)";
}

/* ------------------------------------------------------------------ */
/* SoC Core & Board Model                                             */
/* ------------------------------------------------------------------ */

static void rtd2841_soc_init(Object *obj)
{
    RTD2841SocState *s = RTD2841_SOC(obj);
    int i;

    for (i = 0; i < RTD2841_NUM_CPUS; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cpu[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
    }

    object_initialize_child(obj, "gic", &s->gic, TYPE_ARM_GIC);
    object_initialize_child(obj, "eth", &s->eth, TYPE_RTD2841_ETH);
}

static void rtd2841_soc_realize(DeviceState *dev, Error **errp)
{
    RTD2841SocState *s = RTD2841_SOC(dev);
    DeviceState *gicdev;
    SysBusDevice *gicsbd;
    int i;

    /* Realtek Android TV firmware runs strictly in 32-bit AArch32 mode */
    for (i = 0; i < RTD2841_NUM_CPUS; i++) {
        Object *cpuobj = OBJECT(&s->cpu[i]);

        if (object_property_find(cpuobj, "aarch64")) {
            object_property_set_bool(cpuobj, "aarch64", false, &error_fatal);
        }

        object_property_set_int(cpuobj, "mp-affinity",
                                arm_build_mp_affinity(i, RTD2841_NUM_CPUS),
                                &error_fatal);

        if (object_property_find(cpuobj, "reset-hivecs")) {
            object_property_set_bool(cpuobj, "reset-hivecs", false,
                                     &error_fatal);
        }

        /* Secondary cores hold until released by PSCI CPU_ON */
        if (i > 0) {
            object_property_set_bool(cpuobj, "start-powered-off", true,
                                     &error_fatal);
        }

        if (!qdev_realize(DEVICE(cpuobj), NULL, errp)) {
            return;
        }
    }

    /* GIC-400 (GICv2) - 256 IRQ lines per DTS */
    gicdev = DEVICE(&s->gic);
    gicsbd = SYS_BUS_DEVICE(&s->gic);

    qdev_prop_set_uint32(gicdev, "num-cpu", RTD2841_NUM_CPUS);
    qdev_prop_set_uint32(gicdev, "num-irq", RTD2841_GIC_NUM_IRQS);
    qdev_prop_set_uint32(gicdev, "revision", 2);
    qdev_prop_set_bit(gicdev, "has-security-extensions", false);

    if (!sysbus_realize(gicsbd, errp)) {
        return;
    }

    sysbus_mmio_map(gicsbd, 0, RTD2841_GIC_DIST_BASE);
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

        /* Generic timer PPI connections */
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

    /* 16550A UART0 @ 0x18062300, 27MHz clock, 4-byte stride (shift=2) */
    serial_mm_init(get_system_memory(),
                   RTD2841_UART0_BASE,
                   RTD2841_UART0_REG_SHIFT,
                   qdev_get_gpio_in(gicdev,
                       RTD2841_GIC_SPI_TO_GPIO(RTD2841_UART0_SPI)),
                   RTD2841_UART0_BAUDBASE,
                   serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);

    /* 4MB RBUS window (iomap.h) mapped as background catch-all */
    create_unimplemented_device("rtd2841.soc_peripherals",
                                RTD2841_RBUS_BASE, RTD2841_RBUS_SIZE);
    create_unimplemented_device("rtd2841.tmu",
                                RTD2841_TMU_BASE, RTD2841_TMU_SIZE);
    create_unimplemented_device("rtd2841.sysreg",
                                RTD2841_SYSREG_BASE, RTD2841_SYSREG_SIZE);

    /* Realtek Gigabit Ethernet Controller @ 0x18016000 */
    qemu_configure_nic_device(DEVICE(&s->eth), true, NULL);
    if (!s->eth.conf.peers.ncs[0]) {
        NetClientState *netdev = qemu_find_netdev("net0");
        if (netdev) {
            s->eth.conf.peers.ncs[0] = netdev;
        }
    }
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->eth), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->eth), 0, RTD2841_ETH_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth), 0,
                       qdev_get_gpio_in(gicdev,
                           RTD2841_GIC_SPI_TO_GPIO(RTD2841_ETH_SPI)));

    /* eMMC / SDHCI controller @ 0x18010800 */
    {
        DeviceState *sdhsidev = qdev_new(TYPE_SYSBUS_SDHCI);
        DriveInfo *di;
        BlockBackend *blk;
        DeviceState *carddev;

        sysbus_realize_and_unref(SYS_BUS_DEVICE(sdhsidev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(sdhsidev), 0, RTD2841_EMMC_BASE);
        sysbus_connect_irq(SYS_BUS_DEVICE(sdhsidev), 0,
                           qdev_get_gpio_in(gicdev,
                               RTD2841_GIC_SPI_TO_GPIO(RTD2841_EMMC_SPI)));

        di = drive_get(IF_SD, 0, 0);
        blk = di ? blk_by_legacy_dinfo(di) : NULL;
        carddev = qdev_new(TYPE_SD_CARD);
        qdev_prop_set_drive_err(carddev, "drive", blk, &error_fatal);
        qdev_realize_and_unref(carddev,
                               qdev_get_child_bus(sdhsidev, "sd-bus"),
                               &error_fatal);
    }
    create_unimplemented_device("rtd2841.sdio",
                                RTD2841_SDIO_BASE, RTD2841_SDIO_SIZE);
    create_unimplemented_device("rtd2841.uart1_3",
                                RTD2841_UART1_3_BASE, RTD2841_UART1_3_SIZE);
    rtd2841_timer_create(RTD2841_TIMER_BASE,
                         qdev_get_gpio_in(gicdev,
                             RTD2841_GIC_SPI_TO_GPIO(RTD2841_TIMER_SPI)));
    create_unimplemented_device("rtd2841.gpu",
                                RTD2841_GPU_BASE, RTD2841_GPU_SIZE);
    create_unimplemented_device("rtd2841.arm_tsg",
                                RTD2841_ARM_TSG_BASE, RTD2841_ARM_TSG_SIZE);
    rtd2841_wdog_create(RTD2841_WDOG_BASE);
    create_unimplemented_device("rtd2841.scu",
                                RTD2841_SCU_BASE, RTD2841_SCU_SIZE);
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

static void rtd2841_kbd_event(DeviceState *dev, QemuConsole *src,
                              QemuInputEvent *evt)
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

    if (machine->ram_size < RTD2841_RAM_SIZE) {
        error_report("warning: tv030 profile expects 1024 MB RAM (got: %"
                     PRIu64 " MB)", machine->ram_size / MiB);
    }

    object_initialize_child(OBJECT(machine), "soc", &ms->soc,
                            TYPE_RTD2841_SOC);
    sysbus_realize(SYS_BUS_DEVICE(&ms->soc), &error_fatal);

    memory_region_add_subregion_overlap(get_system_memory(),
                                        RTD2841_RAM_BASE,
                                        machine->ram, -1);

    /* Framebuffer carveout alias @ 0x3E000000 (8MB into machine->ram) */
    memory_region_init_alias(&ms->fb_ram, OBJECT(machine), "rtd2841.fb",
                             machine->ram, RTD2841_FB_BASE, RTD2841_FB_SIZE);
    memory_region_add_subregion_overlap(get_system_memory(),
                                        RTD2841_FB_BASE,
                                        &ms->fb_ram, 1);

    ms->console = graphic_console_init(NULL, 0, &rtd2841_fb_ops, ms);
    {
        uint8_t *fb_ptr = memory_region_get_ram_ptr(&ms->fb_ram);
        uint32_t *pixels = (uint32_t *)fb_ptr;
        size_t total_pixels = RTD2841_FB_WIDTH * RTD2841_FB_HEIGHT;
        size_t p;

        for (p = 0; p < total_pixels; p++) {
            pixels[p] = 0xFF141824;
        }

        DisplaySurface *surface = qemu_create_displaysurface_from(
            RTD2841_FB_WIDTH, RTD2841_FB_HEIGHT,
            PIXMAN_x8r8g8b8, RTD2841_FB_STRIDE, fb_ptr);
        dpy_gfx_replace_surface(ms->console, surface);
    }

    /* Direct key events to UART console */
    (void)qemu_input_handler_register(NULL, &rtd2841_kbd_handler);

    ms->boot_info.ram_size = machine->ram_size;
    ms->boot_info.board_id = -1;
    ms->boot_info.loader_start = RTD2841_RAM_BASE;
    ms->boot_info.psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    ms->boot_info.smp_bootreg_addr = RTD2841_SMP_CPU1_JUMP_ADDR;

    if (machine->firmware) {
        int fw_size = load_image_targphys(machine->firmware,
                                          RTD2841_RAM_BASE,
                                          machine->ram_size, NULL);
        if (fw_size < 0) {
            error_report("Failed to load firmware binary '%s'",
                         machine->firmware);
            exit(1);
        }
    } else {
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
        .name = TYPE_RTD2841_ETH,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RTD2841EthState),
        .instance_init = rtd2841_eth_init,
        .class_init = rtd2841_eth_class_init,
    },
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
