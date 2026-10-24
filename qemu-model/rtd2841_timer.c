/*
 * Realtek RTD2841 / RTK284x Hardware Timer & Watchdog Controller model (tv030)
 *
 * Register ground truth (Linux 4.4 rtk2841 tree, <rbus/timer_reg.h> semantics):
 *   Aperture:   0x1801B600..0x1801B6FF (RBUS, 32-bit register bus)
 *   Clock:      TIMERINFO_BASE_CLOCK = 27000000 (27 MHz)
 *   GIC IRQ:    SPI 2 -> GIC INTID 34 (IRQ_TIMER, "gp timer", level)
 *
 * Register layout (flat, NOT interleaved per-timer):
 *   0x00..0x1C : TIMER_TC0TVR .. TC7TVR    (target / reload value, 8x u32)
 *   0x20..0x3C : TIMER_TC0CVR .. TC7CVR    (current running counter, 8x u32)
 *   0x40..0x5C : TIMER_TC0CR  .. TC7CR     (control)
 *   0x60..0x7C : TIMER_TC0ICR .. TC7ICR    (interrupt control)
 *   0xE8       : TIMER_UMSK_ISR            (raw IRQ status, bits [7:0] =
 *                                           TC7..TC0 raw w/ TC0->bit0)
 *   0xEC       : TIMER_ISR                 (masked IRQ status, W1C per driver:
 *                                           rtd_outl(TIMER_ISR, 1<<id))
 *
 * CR  bits: [31] tc_en (1=enable)
 *           [30] tc_mode (1=periodic, 0=counter/one-shot)
 *           [29] tc_pause
 * ICR bits: [31] tc_ie
 *
 * Timer behaviour: 32-bit hardware up-counter modeled via ptimer countdown.
 * Software CVR read returns (limit - count) so Linux sched_clock sees a
 * monotonically increasing counter. On match/underflow raw ISR bit latches.
 *
 * Watchdog companion block (stub, prevents reboot loops on driver probe):
 *   0x18062200..0x180622FF
 *   0x00 WDOG_MSK_ISR  [8] wdog_int (W1C, never set by this stub)
 *   0x04 WDOG_TCWCR    wden[7:0] wdc[11:8] nmic[15:12] wdie[16] im_wdog_rst[31]
 *   0x08 WDOG_TCWTR    wdclr (clear watchdog counter; accepted, no-op)
 *   0x0C WDOG_TCWNMI   NMI mux selection
 *   0x10 WDOG_TCWOV    overflow/prescaler compare value
 *   The watchdog accepts all configuration writes but never fires NMI/IRQ or
 *   a system reset; guests that enable + keep it kicked or disarm it will
 *   observe a bark-free timer exactly as intended for bring-up emulation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "hw/core/ptimer.h"
#include "hw/core/qdev.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "hw/arm/rtd2841.h"

#define RTD2841_TIMER_CLK       27000000ULL /* TIMERINFO_BASE_CLOCK */
#define RTD2841_TIMER_N         8           /* TC0..TC7 */
#define RTD2841_TIMER_IRQ       34          /* GIC INTID 34 (SPI 2) */

/* ------------------------------------------------------------------ */
/* Timer controller                                                    */
/* ------------------------------------------------------------------ */

#define TYPE_RTD2841_TIMER "rtd2841-timer"
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841TimerState, RTD2841_TIMER)

/* CR (0x40 + 4*id) bits */
#define R_CR_ENABLE             (1u << 31)
#define R_CR_PERIOD             (1u << 30)
#define R_CR_PAUSE              (1u << 29)

/* ICR (0x60 + 4*id) bits */
#define R_ICR_IE                (1u << 31)

/* Aperture offsets (flat arrays) */
#define R_TVR0                  0x00
#define R_CVR0                  0x20
#define R_CR0                   0x40
#define R_ICR0                  0x60
#define R_UMSK_ISR              0xE8
#define R_ISR                   0xEC

#define R_TC_MASK               0xFFu      /* ISR bits [7:0] = TC0..TC7 */

typedef struct Rtd2841TimerCb {
    RTD2841TimerState *parent;
    int id;
} Rtd2841TimerCb;

struct RTD2841TimerState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    qemu_irq irq;

    ptimer_state    *timer[RTD2841_TIMER_N];
    Rtd2841TimerCb   cb[RTD2841_TIMER_N];

    uint32_t tvr[RTD2841_TIMER_N];   /* target / auto-reload               */
    uint32_t cr[RTD2841_TIMER_N];    /* control (tc_en/tc_mode/tc_pause)   */
    uint32_t icr[RTD2841_TIMER_N];   /* interrupt ctrl (tc_ie)             */
    uint32_t isr;                    /* raw timer expiry flags, TC0->bit0  */
    bool oneshot_pending[RTD2841_TIMER_N];
};

static bool rtd2841_running(RTD2841TimerState *s, int id)
{
    return (s->cr[id] & R_CR_ENABLE) && !(s->cr[id] & R_CR_PAUSE);
}

/* masked = raw AND tc_ie, TC0..TC7 -> bit0..bit7 */
static uint32_t rtd2841_isr_gated_mask(RTD2841TimerState *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; i < RTD2841_TIMER_N; i++) {
        if (s->icr[i] & R_ICR_IE) {
            m |= (1u << i);
        }
    }
    return m;
}

static void rtd2841_timer_update_irq(RTD2841TimerState *s)
{
    uint32_t pending = s->isr;
    int i;
    int level;

    for (i = 0; i < RTD2841_TIMER_N; i++) {
        if (!(s->icr[i] & R_ICR_IE)) {
            pending &= ~(1u << i);
        }
    }
    level = (pending != 0);
    qemu_set_irq(s->irq, level);
}

/* ptimer expiry callback: latch raw status, announce on the shared line */
static void rtd2841_timer_expire(void *opaque)
{
    Rtd2841TimerCb *c = opaque;
    RTD2841TimerState *s = c->parent;
    int id = c->id;
    ptimer_state *t = s->timer[id];

    if (s->cr[id] & R_CR_PERIOD) {
        s->isr |= (1u << id);
        rtd2841_timer_update_irq(s);
    } else if (s->oneshot_pending[id]) {
        s->oneshot_pending[id] = false;
        s->isr |= (1u << id);
        rtd2841_timer_update_irq(s);

        /*
         * One-shot timer expired: in hardware, CVR does not freeze.
         * The counter continues counting up monotonically (modulo 2^32).
         * Keep ptimer running with 32-bit rollover so CVR advances.
         * Note: Callback is already inside an active transaction.
         */
        uint64_t limit = ptimer_get_limit(t);
        uint32_t cur_val = (uint32_t)limit;
        uint64_t next_count = 0xFFFFFFFFULL - cur_val;
        if (next_count == 0) {
            next_count = 0xFFFFFFFFULL;
        }
        ptimer_set_limit(t, 0xFFFFFFFFULL, 0);
        ptimer_set_count(t, next_count);
        ptimer_run(t, 0);
    }
}

/* Start the hardware counter: preload CVR from TVR, then run.
   one-shot when tc_mode == 0, periodic (auto-reload from TVR) when 1. */
static void rtd2841_timer_start(RTD2841TimerState *s, int id,
                                bool reload_from_tvr)
{
    ptimer_state *t = s->timer[id];
    bool periodic = (s->cr[id] & R_CR_PERIOD) != 0;
    uint32_t count = s->tvr[id];

    ptimer_transaction_begin(t);
    if (reload_from_tvr) {
        if (count == 0) {
            count = 0xFFFFFFFF;
        }
        ptimer_set_limit(t, count, 1);
        ptimer_set_count(t, count);
    } else {
        if (ptimer_get_count(t) == 0) {
            ptimer_set_count(t, count ? count : 0xFFFFFFFF);
        }
    }
    s->oneshot_pending[id] = !periodic;
    ptimer_run(t, periodic ? 0 : 1);
    ptimer_transaction_commit(t);
}

static void rtd2841_timer_halt(RTD2841TimerState *s, int id)
{
    s->oneshot_pending[id] = false;
    ptimer_transaction_begin(s->timer[id]);
    ptimer_stop(s->timer[id]);
    ptimer_transaction_commit(s->timer[id]);
}

static uint64_t rtd2841_timer_read(void *opaque, hwaddr addr, unsigned size)
{
    RTD2841TimerState *s = RTD2841_TIMER(opaque);
    unsigned id;

    if (addr < 0x20) {                      /* TIMER_TCxTVR */
        id = addr / 4;
        return s->tvr[id];
    } else if (addr < 0x40) {               /* TIMER_TCxCVR : live counter */
        uint64_t limit, count;
        id = (addr - R_CVR0) / 4;
        limit = ptimer_get_limit(s->timer[id]);
        count = ptimer_get_count(s->timer[id]);
        return (uint32_t)(limit - count);
    } else if (addr < 0x60) {               /* TIMER_TCxCR */
        id = (addr - R_CR0) / 4;
        return s->cr[id];
    } else if (addr < 0x80) {               /* TIMER_TCxICR */
        id = (addr - R_ICR0) / 4;
        return s->icr[id];
    } else if (addr == R_UMSK_ISR) {        /* raw status */
        return s->isr;
    } else if (addr == R_ISR) {             /* masked (tc_ie gated) status */
        return s->isr & rtd2841_isr_gated_mask(s);
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: unimplemented read @ 0x%" HWADDR_PRIX "\n",
                  __func__, addr);
    return 0;
}

static void rtd2841_timer_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
    RTD2841TimerState *s = RTD2841_TIMER(opaque);
    unsigned id;

    if (addr < 0x20) {                      /* TIMER_TCxTVR */
        id = addr / 4;
        s->tvr[id] = val;
        /*
         * Program the reload target. If the counter is already on the bus
         * (tc_en=1, e.g. oneshot re-arm path used by cevt_set_next_event)
         * re-arm it so the next underflow happens (tvr - cvr) ticks later:
         * the kernel computes tvr = cvr + evt, hence remaining = evt.
         */
        {
            ptimer_state *t = s->timer[id];
            if (rtd2841_running(s, id)) {
                if (s->cr[id] & R_CR_PERIOD) {
                    uint32_t period = val ? val : 0xFFFFFFFF;
                    ptimer_transaction_begin(t);
                    ptimer_set_limit(t, period, 1);
                    ptimer_set_count(t, period);
                    ptimer_run(t, 0);
                    ptimer_transaction_commit(t);
                } else {
                    uint64_t limit = ptimer_get_limit(t);
                    uint64_t count = ptimer_get_count(t);
                    uint32_t cur = (uint32_t)(limit - count);
                    uint32_t next = (uint32_t)(val - cur);
                    uint64_t new_limit;

                    if (next == 0) {
                        next = 1;
                    }
                    new_limit = (uint64_t)cur + next;

                    s->oneshot_pending[id] = true;

                    ptimer_transaction_begin(t);
                    ptimer_set_limit(t, new_limit, 0);
                    ptimer_set_count(t, next);
                    ptimer_run(t, 1);
                    ptimer_transaction_commit(t);
                }
            } else {
                ptimer_transaction_begin(t);
                if (s->cr[id] & R_CR_PERIOD) {
                    ptimer_set_limit(t, val ? val : 0xFFFFFFFFULL, 0);
                } else {
                    ptimer_set_limit(t, 0xFFFFFFFFULL, 0);
                }
                ptimer_transaction_commit(t);
            }
        }
        return;
    } else if (addr < 0x40) {               /* TIMER_TCxCVR */
        ptimer_state *t;
        uint32_t v = (uint32_t)val;
        id = (addr - R_CVR0) / 4;
        t = s->timer[id];
        ptimer_transaction_begin(t);
        if (rtd2841_running(s, id) && !(s->cr[id] & R_CR_PERIOD) && s->oneshot_pending[id]) {
            uint32_t next = (uint32_t)(s->tvr[id] - v);
            uint64_t new_limit;
            if (next == 0) {
                next = 1;
            }
            new_limit = (uint64_t)v + next;
            ptimer_set_limit(t, new_limit, 0);
            ptimer_set_count(t, next);
            ptimer_run(t, 1);
        } else if (s->cr[id] & R_CR_PERIOD) {
            uint64_t limit = s->tvr[id] ? s->tvr[id] : 0xFFFFFFFFULL;
            ptimer_set_limit(t, limit, 0);
            ptimer_set_count(t, (limit >= v) ? (limit - v) : 0);
        } else {
            ptimer_set_limit(t, 0xFFFFFFFFULL, 0);
            ptimer_set_count(t, 0xFFFFFFFFULL - v);
        }
        ptimer_transaction_commit(t);
        return;
    } else if (addr < 0x60) {               /* TIMER_TCxCR */
        uint32_t old, new_en, old_en;

        id = (addr - R_CR0) / 4;
        old = s->cr[id];
        s->cr[id] = val;

        new_en = val & R_CR_ENABLE;
        old_en = old & R_CR_ENABLE;

        if (new_en && !old_en) {
            rtd2841_timer_start(s, id, true);       /* preload cvr = tvr */
        } else if (new_en && !(val & R_CR_PAUSE)) {
            if (old & R_CR_PAUSE) {
                rtd2841_timer_start(s, id, false);  /* resume: no reload */
            }
        } else if (old_en && !(new_en)) {
            rtd2841_timer_halt(s, id);
        }
        if (new_en && (val & R_CR_PAUSE) && !(old & R_CR_PAUSE)) {
            rtd2841_timer_halt(s, id);              /* pause in flight */
        }
        return;
    } else if (addr < 0x80) {               /* TIMER_TCxICR */
        id = (addr - R_ICR0) / 4;
        s->icr[id] = val;
        rtd2841_timer_update_irq(s);
        return;
    } else if (addr == R_UMSK_ISR || addr == R_ISR) {
        /* Kernel ack: clear_irq_reason(id) => rtd_outl(TIMER_ISR, 1<<id).
           W1C on the raw/status bits (TC0..TC7 -> bit0..bit7). */
        s->isr &= ~(uint32_t)(val & R_TC_MASK);
        rtd2841_timer_update_irq(s);
        return;
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: unimplemented write @ 0x%" HWADDR_PRIX " = 0x%08"
                  PRIX64 "\n", __func__, addr, val);
}

static const MemoryRegionOps rtd2841_timer_ops = {
    .read  = rtd2841_timer_read,
    .write = rtd2841_timer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void rtd2841_timer_reset(DeviceState *dev)
{
    RTD2841TimerState *s = RTD2841_TIMER(dev);
    int i;

    for (i = 0; i < RTD2841_TIMER_N; i++) {
        rtd2841_timer_halt(s, i);
        s->tvr[i] = 0;
        s->cr[i] = 0;
        s->icr[i] = 0;
        s->oneshot_pending[i] = false;
        ptimer_transaction_begin(s->timer[i]);
        ptimer_set_limit(s->timer[i], 0xFFFFFFFFULL, 1);
        ptimer_set_count(s->timer[i], 0xFFFFFFFFULL);
        ptimer_transaction_commit(s->timer[i]);
    }
    s->isr = 0;
    rtd2841_timer_update_irq(s);
}

static void rtd2841_timer_realize(DeviceState *dev, Error **errp)
{
    RTD2841TimerState *s = RTD2841_TIMER(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    int i;

    for (i = 0; i < RTD2841_TIMER_N; i++) {
        s->cb[i].parent = s;
        s->cb[i].id = i;

        s->timer[i] = ptimer_init(rtd2841_timer_expire, &s->cb[i],
                                  PTIMER_POLICY_NO_COUNTER_ROUND_DOWN);

        ptimer_transaction_begin(s->timer[i]);
        ptimer_set_freq(s->timer[i], (uint32_t)RTD2841_TIMER_CLK);
        ptimer_set_limit(s->timer[i], 0xFFFFFFFFULL, 1);
        ptimer_set_count(s->timer[i], 0xFFFFFFFFULL);
        ptimer_transaction_commit(s->timer[i]);
    }

    memory_region_init_io(&s->iomem, OBJECT(s), &rtd2841_timer_ops, s,
                          "rtd2841.timer", RTD2841_TIMER_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void rtd2841_timer_finalize(Object *obj)
{
    RTD2841TimerState *s = RTD2841_TIMER(obj);
    int i;

    for (i = 0; i < RTD2841_TIMER_N; i++) {
        if (s->timer[i]) {
            ptimer_free(s->timer[i]);
            s->timer[i] = NULL;
        }
    }
}

static void rtd2841_timer_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    (void)data;
    dc->realize = rtd2841_timer_realize;
    device_class_set_legacy_reset(dc, rtd2841_timer_reset);
    dc->desc = "Realtek RTD2841 HW timer block (8x 27 MHz countdowners)";
    dc->vmsd = NULL;
}

/* ------------------------------------------------------------------ */
/* Watchdog controller (bring-up stub)                                 */
/* ------------------------------------------------------------------ */

#define TYPE_RTD2841_WDOG "rtd2841-wdog"
OBJECT_DECLARE_SIMPLE_TYPE(RTD2841WdogState, RTD2841_WDOG)

/* WDOG register offsets (0x18062200 aperture) */
#define R_WDOG_MSK_ISR          0x00
#define R_WDOG_TCWCR            0x04
#define R_WDOG_TCWTR            0x08
#define R_WDOG_TCWNMI           0x0C
#define R_WDOG_TCWOV            0x10

/* TCWCR bitfields */
#define R_WDOG_WDEN             0x000000FFu /* counter enable [7:0]  */
#define R_WDOG_WDC              0x00000F00u /* divider        [11:8] */
#define R_WDOG_NMICE            0x0000F000u /* NMI ctrl       [15:12]*/
#define R_WDOG_WDIE             0x00010000u /* int enable     [16]   */
#define R_WDOG_IM_WDOG_RST      0x80000000u /* im_reset       [31]   */

/* MSK_ISR fields */
#define R_WDOG_INT              (1u << 8)

struct RTD2841WdogState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;

    /* Output lines exist so the machine may wire them later; this stub
       never drives any of them. */
    qemu_irq irq;   /* SPI 0 (INTID 32), watchdog NMI source line  */
    qemu_irq frq;   /* watchdog FastIRQ feedback (unused)          */
    qemu_irq rst;   /* system reset request line (unused)          */

    uint32_t msk_isr;
    uint32_t tcwcr;
    uint32_t tcwnmi;
    uint32_t tcwov;
    bool announced;
};

static uint64_t rtd2841_wdog_read(void *opaque, hwaddr addr, unsigned size)
{
    RTD2841WdogState *s = RTD2841_WDOG(opaque);

    switch (addr) {
    case R_WDOG_MSK_ISR:
        /* wdog_int (bit 8) never asserted by this stub */
        return s->msk_isr & ~R_WDOG_INT;
    case R_WDOG_TCWCR:
        return s->tcwcr;
    case R_WDOG_TCWTR:
        return 0;
    case R_WDOG_TCWNMI:
        return s->tcwnmi;
    case R_WDOG_TCWOV:
        return s->tcwov;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unimplemented read @ 0x%" HWADDR_PRIX "\n",
                      __func__, addr);
        return 0;
    }
}

static void rtd2841_wdog_write(void *opaque, hwaddr addr, uint64_t val,
                               unsigned size)
{
    RTD2841WdogState *s = RTD2841_WDOG(opaque);

    switch (addr) {
    case R_WDOG_MSK_ISR:
        s->msk_isr &= ~(uint32_t)(val & R_WDOG_INT);    /* W1C */
        return;
    case R_WDOG_TCWCR:
        s->tcwcr = val;
        if ((val & R_WDOG_WDEN) && !s->announced) {
            s->announced = true;
            qemu_log_mask(LOG_UNIMP,
                          "rtd2841-wdog: watchdog arm seen (TCWCR=0x%08"
                          PRIX64 ", divider=0x%03x, WDIE=%u, im_rst=%u) - "
                          "stub never fires, no reset loop possible\n",
                          val,
                          (unsigned)((val & R_WDOG_WDC) >> 8),
                          (unsigned)!!(val & R_WDOG_WDIE),
                          (unsigned)!!(val & R_WDOG_IM_WDOG_RST));
        }
        return;
    case R_WDOG_TCWTR:
        /* WDCLR: counter clear accepted; stub has nothing to time out */
        return;
    case R_WDOG_TCWNMI:
        s->tcwnmi = val;
        return;
    case R_WDOG_TCWOV:
        s->tcwov = val;
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unimplemented write @ 0x%" HWADDR_PRIX
                      " = 0x%08" PRIX64 "\n", __func__, addr, val);
        return;
    }
}

static const MemoryRegionOps rtd2841_wdog_ops = {
    .read  = rtd2841_wdog_read,
    .write = rtd2841_wdog_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void rtd2841_wdog_realize(DeviceState *dev, Error **errp)
{
    RTD2841WdogState *s = RTD2841_WDOG(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &rtd2841_wdog_ops, s,
                          "rtd2841.wdog", RTD2841_WDOG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->frq);
    sysbus_init_irq(sbd, &s->rst);
}

static void rtd2841_wdog_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    (void)data;
    dc->realize = rtd2841_wdog_realize;
    dc->desc = "Realtek RTD2841 watchdog (bring-up stub, never fires)";
    dc->vmsd = NULL;}

/* ------------------------------------------------------------------ */
/* Type registration & instantiation helpers                           */
/* ------------------------------------------------------------------ */

static const TypeInfo rtd2841_timer_info[] = {
    {
        .name            = TYPE_RTD2841_TIMER,
        .parent          = TYPE_SYS_BUS_DEVICE,
        .instance_size   = sizeof(RTD2841TimerState),
        .instance_finalize = rtd2841_timer_finalize,
        .class_init      = rtd2841_timer_class_init,
    },
    {
        .name            = TYPE_RTD2841_WDOG,
        .parent          = TYPE_SYS_BUS_DEVICE,
        .instance_size   = sizeof(RTD2841WdogState),
        .class_init      = rtd2841_wdog_class_init,
    },
};

DEFINE_TYPES(rtd2841_timer_info)

/*
 * Instantiate the timer block and wire the shared IRQ line to GIC SPI 2
 * (INTID 34). Expected machine glue:
 *
 *   rtd2841_timer_create(RTD2841_TIMER_BASE,
 *                        qdev_get_gpio_in(gicdev, RTD2841_GIC_SPI_TO_GPIO(2)));
 */
DeviceState *rtd2841_timer_create(hwaddr base, qemu_irq irq)
{
    return sysbus_create_simple(TYPE_RTD2841_TIMER, base, irq);
}

/*
 * Instantiate the watchdog stub. It exports 3 unrealized IRQs; the machine
 * may leave them unconnected (the stub never raises). Example:
 *
 *   DeviceState *wd = rtd2841_wdog_create(RTD2841_WDOG_BASE);
 */
DeviceState *rtd2841_wdog_create(hwaddr base)
{
    DeviceState *dev = qdev_new(TYPE_RTD2841_WDOG);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);
    return dev;
}
