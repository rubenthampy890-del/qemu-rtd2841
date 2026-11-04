/*
 * Realtek RTD2841 IR Receiver Peripheral Model
 *
 * Implements the Realtek IRDA pulse decoder and status interface:
 * - MMIO window 0 at 0x18061400 (Control, Data, Payload, Status, Pulse config)
 * - MMIO window 1 at 0x18061000 (ISO_MISC_ISR bit 5 irda_int, ISO_MISC_SYSTEM)
 * - Hardware FIFO of depth 32 for raw and auto-formatted RCA codewords
 * - Dynamic scancode injection via QOM property and HMP/QMP commands
 *
 * Target: TCL Smart TV RT41 Chassis (profile tv030)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"

#if defined(QEMU_IN_TREE) || __has_include("hw/arm/rtd2841.h")
#include "hw/arm/rtd2841.h"
#else
#include "rtd2841.h"
#endif

#define RTD2841_IR_FIFO_SIZE        32
#define RTD2841_IR_MMIO_SIZE        0x00000200ULL /* 512 B */
#define RTD2841_ISO_MISC_MMIO_SIZE  0x00000020ULL /* 32 B */

/* Register offsets in IRDA window (0x18061400) */
#define REG_IRDA_PSR                0x00
#define REG_IRDA_PER                0x04
#define REG_IRDA_SF                 0x08
#define REG_IRDA_DPIR               0x0C
#define REG_IRDA_CR                 0x10
#define REG_IRDA_RP                 0x14
#define REG_IRDA_SR                 0x18
#define REG_IRDA_PSR_UP             0x30
#define REG_IRDA_PER_UP             0x34
#define REG_IRDA_CTRL_RC6           0x38
#define REG_IRDA_RP2                0x3C
#define REG_IRDA_EXT_RP_CF          0x44
#define REG_IRDA_EXT_RP_TIMING      0x48
#define REG_IRDA_EXT_RP_TIMING_UP   0x4C
#define REG_IRDA_HW_DECODE_SEL      0x50
#define REG_IRDA_HW_DECODE_READ     0x54
#define REG_IRDA_MASK_DATA_READ     0x58
#define REG_IRDA_HW_DECODE_DATA     0x5C
#define REG_IRDA_MASK_DATA          0x60
#define REG_IRDA_INT_CTRL           0x64
#define REG_IRDA_INT_CTRL1          0x68
#define REG_IRDA_COMP_TEST_MODE     0x6C
#define REG_IRDA_COMP_TEST_DATA7_L  0x70
#define REG_IRDA_COMP_TEST_DATA7_H  0x74
#define REG_IRDA_COMP_TEST_SEL      0x78
#define REG_IRDA_PSR_RC5_RC6        0xA0
#define REG_IRDA_PER_RC5_RC6        0xA4
#define REG_IRDA_PSR_UP_RC5_RC6     0xA8
#define REG_IRDA_PER_UP_RC5_RC6     0xAC
#define REG_IRDA_CR_RC5_RC6         0xB0
#define REG_IRDA_DPIR_RC5_RC6       0xB4
#define REG_IRDA_RPT_CTRL_RC5_RC6   0xB8

/* Register offsets in ISO_MISC window (0x18061000) */
#define REG_ISO_MISC_ISR            0x00
#define REG_ISO_MISC_DBG            0x08
#define REG_ISO_MISC_SYSTEM         0x0C
#define REG_ISO_MISC_DDC_CTRL       0x10
#define REG_ISO_MISC_DUMMY1         0x14
#define REG_ISO_MISC_DUMMY2         0x18

OBJECT_DECLARE_SIMPLE_TYPE(RTD2841IRState, RTD2841_IR)

struct RTD2841IRState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion irda_mmio;
    MemoryRegion iso_misc_mmio;
    qemu_irq irq;

    /* IRDA Registers */
    uint32_t psr;
    uint32_t per;
    uint32_t sf;
    uint32_t dpir;
    uint32_t cr;
    uint32_t rp;
    uint32_t sr;
    uint32_t psr_up;
    uint32_t per_up;
    uint32_t ctrl_rc6;
    uint32_t rp2;
    uint32_t ext_rp_cf;
    uint32_t ext_rp_timing;
    uint32_t ext_rp_timing_up;
    uint32_t hw_decode_sel;
    uint32_t hw_decode_read;
    uint32_t decode_mask_data_read;
    uint32_t hw_decode_data;
    uint32_t decode_mask_data;
    uint32_t int_ctrl;
    uint32_t int_ctrl1;
    uint32_t compare_test_mode;
    uint32_t compare_test_mode_data7_l;
    uint32_t compare_test_mode_data7_h;
    uint32_t compare_test_mode_sel;
    uint32_t psr_rc5_rc6;
    uint32_t per_rc5_rc6;
    uint32_t psr_up_rc5_rc6;
    uint32_t per_up_rc5_rc6;
    uint32_t cr_rc5_rc6;
    uint32_t dpir_rc5_rc6;
    uint32_t rpt_ctrl_rc5_rc6;

    /* ISO_MISC Registers */
    uint32_t iso_misc_isr;
    uint32_t iso_misc_dbg;
    uint32_t iso_misc_system;
    uint32_t iso_misc_ddc_ctrl;
    uint32_t iso_misc_dummy1;
    uint32_t iso_misc_dummy2;

    /* Internal Hardware FIFO & Packet Ownership */
    bool rp_unread;
    uint32_t fifo[RTD2841_IR_FIFO_SIZE];
    uint32_t fifo_head;
    uint32_t fifo_tail;
    uint32_t fifo_count;
};

static inline bool rtd2841_ir_is_rx_enabled(RTD2841IRState *s)
{
    /*
     * Main0 enabled via CR bit 8 (IRDA_IR_CR_main0_en_mask);
     * Main1 enabled via CR_RC5_RC6 bit 0 (IRDA_IR_CR_RC5_RC6_main1_en_mask).
     */
    return (s->cr_rc5_rc6 & 1) || (s->cr & (1 << 8));
}

static inline bool rtd2841_ir_is_irq_enabled(RTD2841IRState *s)
{
    /*
     * Hardware decode interrupt enable is in CR bit 10 (IRDA_IR_CR_ir_hwval_ie).
     */
    return (s->cr & (1 << 10)) != 0;
}

static void rtd2841_ir_update_irq(RTD2841IRState *s)
{
    bool rx_en = rtd2841_ir_is_rx_enabled(s);
    bool irq_en = rtd2841_ir_is_irq_enabled(s);
    bool pending = (s->iso_misc_isr & (1 << 5)) && (s->sr & 1);

    bool level = rx_en && irq_en && pending;
    qemu_set_irq(s->irq, level ? 1 : 0);
}

static void rtd2841_ir_check_fifo(RTD2841IRState *s)
{
    if (!rtd2841_ir_is_rx_enabled(s)) {
        return;
    }
    if (!s->rp_unread && s->fifo_count > 0) {
        s->rp = s->fifo[s->fifo_head];
        s->fifo_head = (s->fifo_head + 1) % RTD2841_IR_FIFO_SIZE;
        s->fifo_count--;
        s->rp_unread = true;
        s->sr |= (1 << 9) | 1; /* Main1 source + IRDVF */
        s->iso_misc_isr |= (1 << 5); /* ISO_MISC_ISR irda_int */
        rtd2841_ir_update_irq(s);
    }
}

void rtd2841_ir_inject_scancode(DeviceState *dev, uint32_t scancode)
{
    RTD2841IRState *s = RTD2841_IR(dev);
    uint32_t codeword;

    /* Drop scancodes when receiver is disabled */
    if (!rtd2841_ir_is_rx_enabled(s)) {
        return;
    }

    /*
     * If user supplied a raw single-byte / 12-bit scancode (e.g., 0x0B for KEY_ENTER,
     * 0xA6 for KEY_UP), format as standard RCA codeword expected by venus_ir_scancode_to_keycode:
     * ((~((value & 0xfff000) >> 12)) & 0x000fff) == (value & 0x000fff)
     */
    if (scancode <= 0x0FFF) {
        uint32_t b = scancode & 0x0FFF;
        codeword = ((~b & 0x0FFF) << 12) | b;
    } else {
        codeword = scancode;
    }

    /*
     * Separate packet storage from acknowledgement state:
     * If s->rp currently holds an unread packet, new packets must queue into the
     * FIFO so that subsequent injections never overwrite unread data.
     */
    if (s->rp_unread) {
        if (s->fifo_count < RTD2841_IR_FIFO_SIZE) {
            s->fifo[s->fifo_tail] = codeword;
            s->fifo_tail = (s->fifo_tail + 1) % RTD2841_IR_FIFO_SIZE;
            s->fifo_count++;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: IR FIFO overflow, dropping 0x%08x\n",
                          __func__, codeword);
        }
    } else {
        s->rp = codeword;
        s->rp_unread = true;
        s->sr |= (1 << 9) | 1; /* Main1 (RCA on tv030) + IRDVF */
        s->iso_misc_isr |= (1 << 5); /* ISO_MISC_ISR bit 5 irda_int */
        rtd2841_ir_update_irq(s);
    }
}

static void rtd2841_ir_set_inject_scancode(Object *obj, Visitor *v,
                                           const char *name, void *opaque,
                                           Error **errp)
{
    RTD2841IRState *s = RTD2841_IR(obj);
    uint32_t val;

    if (!visit_type_uint32(v, name, &val, errp)) {
        return;
    }
    rtd2841_ir_inject_scancode(DEVICE(s), val);
}

/* ------------------------------------------------------------------ */
/* IRDA MMIO Window (0x18061400)                                       */
/* ------------------------------------------------------------------ */

static uint64_t rtd2841_irda_read(void *opaque, hwaddr addr, unsigned size)
{
    RTD2841IRState *s = opaque;

    (void)size;
    switch (addr) {
    case REG_IRDA_PSR:
        return s->psr;
    case REG_IRDA_PER:
        return s->per;
    case REG_IRDA_SF:
        return s->sf;
    case REG_IRDA_DPIR:
        return s->dpir;
    case REG_IRDA_CR:
        return s->cr;
    case REG_IRDA_RP:
        {
            uint32_t r = s->rp;
            s->rp_unread = false;
            rtd2841_ir_check_fifo(s);
            return r;
        }
    case REG_IRDA_SR:
        return s->sr;
    case REG_IRDA_PSR_UP:
        return s->psr_up;
    case REG_IRDA_PER_UP:
        return s->per_up;
    case REG_IRDA_CTRL_RC6:
        return s->ctrl_rc6;
    case REG_IRDA_RP2:
        return s->rp2;
    case REG_IRDA_EXT_RP_CF:
        return s->ext_rp_cf;
    case REG_IRDA_EXT_RP_TIMING:
        return s->ext_rp_timing;
    case REG_IRDA_EXT_RP_TIMING_UP:
        return s->ext_rp_timing_up;
    case REG_IRDA_HW_DECODE_SEL:
        return s->hw_decode_sel;
    case REG_IRDA_HW_DECODE_READ:
        return s->hw_decode_read;
    case REG_IRDA_MASK_DATA_READ:
        return s->decode_mask_data_read;
    case REG_IRDA_HW_DECODE_DATA:
        return s->hw_decode_data;
    case REG_IRDA_MASK_DATA:
        return s->decode_mask_data;
    case REG_IRDA_INT_CTRL:
        return s->int_ctrl;
    case REG_IRDA_INT_CTRL1:
        return s->int_ctrl1;
    case REG_IRDA_COMP_TEST_MODE:
        return s->compare_test_mode;
    case REG_IRDA_COMP_TEST_DATA7_L:
        return s->compare_test_mode_data7_l;
    case REG_IRDA_COMP_TEST_DATA7_H:
        return s->compare_test_mode_data7_h;
    case REG_IRDA_COMP_TEST_SEL:
        return s->compare_test_mode_sel;
    case REG_IRDA_PSR_RC5_RC6:
        return s->psr_rc5_rc6;
    case REG_IRDA_PER_RC5_RC6:
        return s->per_rc5_rc6;
    case REG_IRDA_PSR_UP_RC5_RC6:
        return s->psr_up_rc5_rc6;
    case REG_IRDA_PER_UP_RC5_RC6:
        return s->per_up_rc5_rc6;
    case REG_IRDA_CR_RC5_RC6:
        return s->cr_rc5_rc6;
    case REG_IRDA_DPIR_RC5_RC6:
        return s->dpir_rc5_rc6;
    case REG_IRDA_RPT_CTRL_RC5_RC6:
        return s->rpt_ctrl_rc5_rc6;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unhandled IRDA register read at 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return 0;
    }
}

static void rtd2841_irda_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    RTD2841IRState *s = opaque;

    (void)size;
    switch (addr) {
    case REG_IRDA_PSR:
        s->psr = val;
        break;
    case REG_IRDA_PER:
        s->per = val;
        break;
    case REG_IRDA_SF:
        s->sf = val;
        break;
    case REG_IRDA_DPIR:
        s->dpir = val;
        break;
    case REG_IRDA_CR:
        s->cr = val;
        rtd2841_ir_check_fifo(s);
        rtd2841_ir_update_irq(s);
        break;
    case REG_IRDA_RP:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: write to read-only IR_RP: 0x%" PRIx64 "\n",
                      __func__, val);
        break;
    case REG_IRDA_SR:
        /* Writing bit 0 (or bit 1) clears IRDVF / repeat flag */
        if (val & 0x01) {
            s->sr &= ~0x03;
            s->iso_misc_isr &= ~(1 << 5);
            rtd2841_ir_update_irq(s);
        }
        break;
    case REG_IRDA_PSR_UP:
        s->psr_up = val;
        break;
    case REG_IRDA_PER_UP:
        s->per_up = val;
        break;
    case REG_IRDA_CTRL_RC6:
        s->ctrl_rc6 = val;
        break;
    case REG_IRDA_RP2:
        s->rp2 = val;
        break;
    case REG_IRDA_EXT_RP_CF:
        s->ext_rp_cf = val;
        break;
    case REG_IRDA_EXT_RP_TIMING:
        s->ext_rp_timing = val;
        break;
    case REG_IRDA_EXT_RP_TIMING_UP:
        s->ext_rp_timing_up = val;
        break;
    case REG_IRDA_HW_DECODE_SEL:
        s->hw_decode_sel = val;
        break;
    case REG_IRDA_HW_DECODE_READ:
        s->hw_decode_read = val;
        break;
    case REG_IRDA_MASK_DATA_READ:
        s->decode_mask_data_read = val;
        break;
    case REG_IRDA_HW_DECODE_DATA:
        s->hw_decode_data = val;
        break;
    case REG_IRDA_MASK_DATA:
        s->decode_mask_data = val;
        break;
    case REG_IRDA_INT_CTRL:
        s->int_ctrl = val;
        break;
    case REG_IRDA_INT_CTRL1:
        s->int_ctrl1 = val;
        break;
    case REG_IRDA_COMP_TEST_MODE:
        s->compare_test_mode = val;
        break;
    case REG_IRDA_COMP_TEST_DATA7_L:
        s->compare_test_mode_data7_l = val;
        break;
    case REG_IRDA_COMP_TEST_DATA7_H:
        s->compare_test_mode_data7_h = val;
        break;
    case REG_IRDA_COMP_TEST_SEL:
        s->compare_test_mode_sel = val;
        break;
    case REG_IRDA_PSR_RC5_RC6:
        s->psr_rc5_rc6 = val;
        break;
    case REG_IRDA_PER_RC5_RC6:
        s->per_rc5_rc6 = val;
        break;
    case REG_IRDA_PSR_UP_RC5_RC6:
        s->psr_up_rc5_rc6 = val;
        break;
    case REG_IRDA_PER_UP_RC5_RC6:
        s->per_up_rc5_rc6 = val;
        break;
    case REG_IRDA_CR_RC5_RC6:
        s->cr_rc5_rc6 = val;
        rtd2841_ir_check_fifo(s);
        rtd2841_ir_update_irq(s);
        break;
    case REG_IRDA_DPIR_RC5_RC6:
        s->dpir_rc5_rc6 = val;
        break;
    case REG_IRDA_RPT_CTRL_RC5_RC6:
        s->rpt_ctrl_rc5_rc6 = val;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unhandled IRDA register write at 0x%" HWADDR_PRIx
                      " val=0x%" PRIx64 "\n", __func__, addr, val);
        break;
    }
}

static const MemoryRegionOps rtd2841_irda_ops = {
    .read = rtd2841_irda_read,
    .write = rtd2841_irda_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* ------------------------------------------------------------------ */
/* ISO_MISC MMIO Window (0x18061000)                                  */
/* ------------------------------------------------------------------ */

static uint64_t rtd2841_iso_misc_read(void *opaque, hwaddr addr, unsigned size)
{
    RTD2841IRState *s = opaque;

    (void)size;
    switch (addr) {
    case REG_ISO_MISC_ISR:
        return s->iso_misc_isr;
    case REG_ISO_MISC_DBG:
        return s->iso_misc_dbg;
    case REG_ISO_MISC_SYSTEM:
        return s->iso_misc_system;
    case REG_ISO_MISC_DDC_CTRL:
        return s->iso_misc_ddc_ctrl;
    case REG_ISO_MISC_DUMMY1:
        return s->iso_misc_dummy1;
    case REG_ISO_MISC_DUMMY2:
        return s->iso_misc_dummy2;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unhandled ISO_MISC register read at 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return 0;
    }
}

static void rtd2841_iso_misc_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    RTD2841IRState *s = opaque;

    (void)size;
    switch (addr) {
    case REG_ISO_MISC_ISR:
        /* W1C on bit 5 */
        if (val & (1 << 5)) {
            s->iso_misc_isr &= ~(1 << 5);
            rtd2841_ir_update_irq(s);
        }
        break;
    case REG_ISO_MISC_DBG:
        s->iso_misc_dbg = val;
        break;
    case REG_ISO_MISC_SYSTEM:
        s->iso_misc_system = val;
        break;
    case REG_ISO_MISC_DDC_CTRL:
        s->iso_misc_ddc_ctrl = val;
        break;
    case REG_ISO_MISC_DUMMY1:
        s->iso_misc_dummy1 = val;
        break;
    case REG_ISO_MISC_DUMMY2:
        s->iso_misc_dummy2 = val;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unhandled ISO_MISC register write at 0x%" HWADDR_PRIx
                      " val=0x%" PRIx64 "\n", __func__, addr, val);
        break;
    }
}

static const MemoryRegionOps rtd2841_iso_misc_ops = {
    .read = rtd2841_iso_misc_read,
    .write = rtd2841_iso_misc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* ------------------------------------------------------------------ */
/* Device Lifecycle & Type Info                                       */
/* ------------------------------------------------------------------ */

static void rtd2841_ir_reset(DeviceState *dev)
{
    RTD2841IRState *s = RTD2841_IR(dev);

    s->psr = 0;
    s->per = 0;
    s->sf = 0;
    s->dpir = 0;
    s->cr = 0;
    s->rp = 0;
    s->sr = 0;
    s->psr_up = 0;
    s->per_up = 0;
    s->ctrl_rc6 = 0;
    s->rp2 = 0;
    s->ext_rp_cf = 0;
    s->ext_rp_timing = 0;
    s->ext_rp_timing_up = 0;
    s->hw_decode_sel = 0;
    s->hw_decode_read = 0;
    s->decode_mask_data_read = 0;
    s->hw_decode_data = 0;
    s->decode_mask_data = 0;
    s->int_ctrl = 0;
    s->int_ctrl1 = 0;
    s->compare_test_mode = 0;
    s->compare_test_mode_data7_l = 0;
    s->compare_test_mode_data7_h = 0;
    s->compare_test_mode_sel = 0;
    s->psr_rc5_rc6 = 0;
    s->per_rc5_rc6 = 0;
    s->psr_up_rc5_rc6 = 0;
    s->per_up_rc5_rc6 = 0;
    s->cr_rc5_rc6 = 0;
    s->dpir_rc5_rc6 = 0;
    s->rpt_ctrl_rc5_rc6 = 0;

    s->iso_misc_isr = 0;
    s->iso_misc_dbg = 0;
    s->iso_misc_system = 0;
    s->iso_misc_ddc_ctrl = 0;
    s->iso_misc_dummy1 = 0;
    s->iso_misc_dummy2 = 0;

    s->rp_unread = false;
    memset(s->fifo, 0, sizeof(s->fifo));
    s->fifo_head = 0;
    s->fifo_tail = 0;
    s->fifo_count = 0;

    qemu_set_irq(s->irq, 0);
}

static void rtd2841_ir_init(Object *obj)
{
    RTD2841IRState *s = RTD2841_IR(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->irda_mmio, obj, &rtd2841_irda_ops, s,
                          "rtd2841.irda", RTD2841_IR_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->irda_mmio);

    memory_region_init_io(&s->iso_misc_mmio, obj, &rtd2841_iso_misc_ops, s,
                          "rtd2841.iso_misc", RTD2841_ISO_MISC_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iso_misc_mmio);

    sysbus_init_irq(sbd, &s->irq);

    object_property_add(obj, "inject-scancode", "uint32",
                        NULL, rtd2841_ir_set_inject_scancode,
                        NULL, NULL);
}

DeviceState *rtd2841_ir_create(hwaddr irda_base, hwaddr iso_misc_base, qemu_irq irq)
{
    DeviceState *dev = qdev_new(TYPE_RTD2841_IR);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    sysbus_realize_and_unref(sbd, &error_fatal);
    sysbus_mmio_map(sbd, 0, irda_base);
    sysbus_mmio_map(sbd, 1, iso_misc_base);
    sysbus_connect_irq(sbd, 0, irq);

    return dev;
}

static const VMStateDescription vmstate_rtd2841_ir = {
    .name = "rtd2841_ir",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(psr, RTD2841IRState),
        VMSTATE_UINT32(per, RTD2841IRState),
        VMSTATE_UINT32(sf, RTD2841IRState),
        VMSTATE_UINT32(dpir, RTD2841IRState),
        VMSTATE_UINT32(cr, RTD2841IRState),
        VMSTATE_UINT32(rp, RTD2841IRState),
        VMSTATE_UINT32(sr, RTD2841IRState),
        VMSTATE_UINT32(psr_up, RTD2841IRState),
        VMSTATE_UINT32(per_up, RTD2841IRState),
        VMSTATE_UINT32(ctrl_rc6, RTD2841IRState),
        VMSTATE_UINT32(rp2, RTD2841IRState),
        VMSTATE_UINT32(ext_rp_cf, RTD2841IRState),
        VMSTATE_UINT32(ext_rp_timing, RTD2841IRState),
        VMSTATE_UINT32(ext_rp_timing_up, RTD2841IRState),
        VMSTATE_UINT32(hw_decode_sel, RTD2841IRState),
        VMSTATE_UINT32(hw_decode_read, RTD2841IRState),
        VMSTATE_UINT32(decode_mask_data_read, RTD2841IRState),
        VMSTATE_UINT32(hw_decode_data, RTD2841IRState),
        VMSTATE_UINT32(decode_mask_data, RTD2841IRState),
        VMSTATE_UINT32(int_ctrl, RTD2841IRState),
        VMSTATE_UINT32(int_ctrl1, RTD2841IRState),
        VMSTATE_UINT32(compare_test_mode, RTD2841IRState),
        VMSTATE_UINT32(compare_test_mode_data7_l, RTD2841IRState),
        VMSTATE_UINT32(compare_test_mode_data7_h, RTD2841IRState),
        VMSTATE_UINT32(compare_test_mode_sel, RTD2841IRState),
        VMSTATE_UINT32(psr_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(per_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(psr_up_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(per_up_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(cr_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(dpir_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(rpt_ctrl_rc5_rc6, RTD2841IRState),
        VMSTATE_UINT32(iso_misc_isr, RTD2841IRState),
        VMSTATE_UINT32(iso_misc_dbg, RTD2841IRState),
        VMSTATE_UINT32(iso_misc_system, RTD2841IRState),
        VMSTATE_UINT32(iso_misc_ddc_ctrl, RTD2841IRState),
        VMSTATE_UINT32(iso_misc_dummy1, RTD2841IRState),
        VMSTATE_UINT32(iso_misc_dummy2, RTD2841IRState),
        VMSTATE_BOOL(rp_unread, RTD2841IRState),
        VMSTATE_UINT32_ARRAY(fifo, RTD2841IRState, RTD2841_IR_FIFO_SIZE),
        VMSTATE_UINT32(fifo_head, RTD2841IRState),
        VMSTATE_UINT32(fifo_tail, RTD2841IRState),
        VMSTATE_UINT32(fifo_count, RTD2841IRState),
        VMSTATE_END_OF_LIST()
    }
};

static void rtd2841_ir_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    (void)data;
    device_class_set_legacy_reset(dc, rtd2841_ir_reset);
    dc->vmsd = &vmstate_rtd2841_ir;
    dc->desc = "Realtek RTD2841 IR Receiver Controller";
}

static const TypeInfo rtd2841_ir_info = {
    .name          = TYPE_RTD2841_IR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RTD2841IRState),
    .instance_init = rtd2841_ir_init,
    .class_init    = rtd2841_ir_class_init,
};

static void rtd2841_ir_register_types(void)
{
    type_register_static(&rtd2841_ir_info);
}

type_init(rtd2841_ir_register_types)
