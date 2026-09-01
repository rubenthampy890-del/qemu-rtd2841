/*
 * Realtek RTD2841 / RTK284x IPC & Kernel Driver Emulation Specification
 * Generated for Track 2 Firmware Reverse Engineering & QEMU Service Emulation
 */

#ifndef _RTK_RPC_STUBS_H_
#define _RTK_RPC_STUBS_H_

#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. Hardware Mailbox & Signal Constants
 * ========================================================================= */
#define REG_SB2_CPU_INT             0xb801a104
#define REG_SB2_CPU_INT_EN          0xb801a108

#define RPC_INT_WRITE_1             (1 << 0)
#define RPC_INT_SA                  (1 << 1)
#define RPC_INT_SV                  (1 << 2)
#define RPC_INT_SV2                 (1 << 3)
#define RPC_INT_AS                  (1 << 4)
#define RPC_INT_AV                  (1 << 5)
#define RPC_INT_AV2                 (1 << 6)
#define RPC_INT_VA                  (1 << 7)
#define RPC_INT_VS                  (1 << 8)
#define RPC_INT_VV2                 (1 << 9)
#define RPC_INT_V2S                 (1 << 10)
#define RPC_INT_V2A                 (1 << 11)
#define RPC_INT_V2V                 (1 << 12)
#define RPC_INT_KS                  (1 << 13)
#define RPC_INT_KA                  (1 << 14)
#define RPC_INT_KV                  (1 << 15)
#define RPC_INT_KV2                 (1 << 16)
#define RPC_INT_SK                  (1 << 17)
#define RPC_INT_AK                  (1 << 18)
#define RPC_INT_VK                  (1 << 19)
#define RPC_INT_V2K                 (1 << 20)

/* Inter-processor sync flags (physical addresses) */
#define SYNC_FLAG_ACPU              0x000000d0
#define SYNC_FLAG_V1CPU             0x000000d4
#define SYNC_FLAG_V2CPU             0x000000d8
#define SYNC_FLAG_SCPU              0x00000800

/* Signal used by Realtek IPC on 32-bit ARM Bionic */
#define SIGRPC                      SIGUSR2 /* Signal 12 */

/* =========================================================================
 * 2. Character Device Paths & Default Major Numbers
 * ========================================================================= */
#define RPC_DEFAULT_MAJOR           240

#define DEV_RPC_SA_POLL_WRITE       "/dev/rpc0"   /* Minor 0  - System -> Audio Poll */
#define DEV_RPC_SA_INTR_WRITE       "/dev/rpc1"   /* Minor 1  - System -> Audio Intr */
#define DEV_RPC_AS_POLL_READ        "/dev/rpc2"   /* Minor 2  - Audio -> System Poll */
#define DEV_RPC_AS_INTR_READ        "/dev/rpc3"   /* Minor 3  - Audio -> System Intr */
#define DEV_RPC_SV1_POLL_WRITE      "/dev/rpc4"   /* Minor 4  - System -> Video1 Poll */
#define DEV_RPC_SV1_INTR_WRITE      "/dev/rpc5"   /* Minor 5  - System -> Video1 Intr */
#define DEV_RPC_V1S_POLL_READ       "/dev/rpc6"   /* Minor 6  - Video1 -> System Poll */
#define DEV_RPC_V1S_INTR_READ       "/dev/rpc7"   /* Minor 7  - Video1 -> System Intr */
#define DEV_RPC_SV2_POLL_WRITE      "/dev/rpc8"   /* Minor 8  - System -> Video2 Poll */
#define DEV_RPC_SV2_INTR_WRITE      "/dev/rpc9"   /* Minor 9  - System -> Video2 Intr */
#define DEV_RPC_V2S_POLL_READ       "/dev/rpc10"  /* Minor 10 - Video2 -> System Poll */
#define DEV_RPC_V2S_INTR_READ       "/dev/rpc11"  /* Minor 11 - Video2 -> System Intr */
#define DEV_RPC_SK_POLL_WRITE       "/dev/rpc12"  /* Minor 12 - System -> KCPU Poll */
#define DEV_RPC_SK_INTR_WRITE       "/dev/rpc13"  /* Minor 13 - System -> KCPU Intr */
#define DEV_RPC_KS_POLL_READ        "/dev/rpc14"  /* Minor 14 - KCPU -> System Poll */
#define DEV_RPC_KS_INTR_READ        "/dev/rpc15"  /* Minor 15 - KCPU -> System Intr */
#define DEV_RPC_CTRL                "/dev/rpc100" /* Minor 100- RPC Control */
#define DEV_RPC_DEBUG               "/dev/rpc101" /* Minor 101- RPC Debug */

#define DEV_SYS_MISC                "/dev/sys-misc"
#define DEV_RTD_LOGGER              "/dev/rtd-logger"
#define DEV_TEE_PROXY               "/dev/teeproxy"
#define DEV_EMCU_KDV                "/dev/emcu_kdv"

/* =========================================================================
 * 3. RPC Ioctl Commands (Magic 'k')
 * ========================================================================= */
#define RPC_IOC_MAGIC               'k'

#define RPC_IOCTTIMEOUT             _IO(RPC_IOC_MAGIC, 0) /* 0x00006b00 */
#define RPC_IOCQTIMEOUT             _IO(RPC_IOC_MAGIC, 1) /* 0x00006b01 */
#define RPC_IOCTRESET               _IO(RPC_IOC_MAGIC, 2) /* 0x00006b02 */
#define RPC_IOCTHANDLER             _IO(RPC_IOC_MAGIC, 3) /* 0x00006b03 */
#define RPC_IOSTOP                  _IO(RPC_IOC_MAGIC, 4) /* 0x00006b04 */

/* =========================================================================
 * 4. sys-misc Ioctl Commands (Magic 's')
 * ========================================================================= */
#define MISC_IOC_MAGIC              's'

#define MISC_WAKEUP_ECPU            _IO(MISC_IOC_MAGIC, 0)                 /* 0x00007300 */
#define MISC_SET_WOL_ENABLE         _IOW(MISC_IOC_MAGIC, 1, int)           /* 0x40047301 */
#define MISC_GET_WOL_ENABLE         _IOR(MISC_IOC_MAGIC, 2, int*)          /* 0x80047302 */
#define MISC_SET_DEBUG_LOCK         _IOW(MISC_IOC_MAGIC, 3, int)           /* 0x40047303 */
#define MISC_SET_DDR_SSC_PARAM      _IOWR(MISC_IOC_MAGIC, 6, void*)        /* 0xc0047306 */
#define MISC_SET_RTDLOG             _IOW(MISC_IOC_MAGIC, 7, unsigned int)  /* 0x40047307 */
#define MISC_GET_RTDLOG             _IOR(MISC_IOC_MAGIC, 8, unsigned int*) /* 0x80047308 */
#define MISC_SET_WARM_WOL_ENABLE    _IOW(MISC_IOC_MAGIC, 11, int)          /* 0x4004730b */
#define MISC_GET_WARM_WOL_ENABLE    _IOR(MISC_IOC_MAGIC, 12, int*)         /* 0x8004730c */
#define MISC_QSM_OFF                _IO(MISC_IOC_MAGIC, 13)                /* 0x0000730d */
#define MISC_SET_WATCHDOG_ENABLE    _IOW(MISC_IOC_MAGIC, 14, int)          /* 0x4004730e */
#define MISC_UART_PINMUX_SWITCH     _IOW(MISC_IOC_MAGIC, 17, int)          /* 0x40047311 */
#define MISC_UART_GET_UART0_ENABLE  _IOW(MISC_IOC_MAGIC, 18, unsigned int*)/* 0x40047312 */
#define MISC_UART_GET_UART1_ENABLE  _IOW(MISC_IOC_MAGIC, 19, unsigned int*)/* 0x40047313 */

/* =========================================================================
 * 5. rtd-logger Ioctl Commands (Magic 'k')
 * ========================================================================= */
#define RTDLOG_DEV_MAGIC            'k'

#define RTDLOG_IOC_LOCK_CMD         _IOWR(RTDLOG_DEV_MAGIC, 0, int)          /* 0xc0046b00 */
#define RTDLOG_IOC_UNLOCK_CMD       _IOWR(RTDLOG_DEV_MAGIC, 1, int)          /* 0xc0046b01 */
#define RTDLOG_IOC_LOGPATH_CMD      _IOWR(RTDLOG_DEV_MAGIC, 2, unsigned long)/* 0xc0046b02 */
#define RTDLOG_IOC_KERNEL_SLEEP     _IOWR(RTDLOG_DEV_MAGIC, 3, unsigned long)/* 0xc0046b03 */

#define LOGBUF_DIR_MAX_SIZE         256
#define RTD_LOGBUF_DEFAULT_DIR      "/tmp/var/log/"

/* =========================================================================
 * 6. teeproxy Ioctl Commands (Magic 't')
 * ========================================================================= */
#define TEE_PROXY_IOC_MAGIC         't'

#define TEE_PROXY_IOC_INIT          _IO(TEE_PROXY_IOC_MAGIC, 1)              /* 0x00007401 */
#define TEE_PROXY_IOC_DEINIT        _IO(TEE_PROXY_IOC_MAGIC, 2)              /* 0x00007402 */
#define TEE_PROXY_IOC_ALLOC         _IOW(TEE_PROXY_IOC_MAGIC, 3, void*)      /* 0x40047403 */
#define TEE_PROXY_IOC_FREE          _IOW(TEE_PROXY_IOC_MAGIC, 4, int)        /* 0x40047404 */
#define TEE_PROXY_IOC_KILL          _IOW(TEE_PROXY_IOC_MAGIC, 5, int)        /* 0x40047405 */
#define TEE_PROXY_IOC_TEEMGR_PID    _IOR(TEE_PROXY_IOC_MAGIC, 6, pid_t)      /* 0x80047406 */

/* =========================================================================
 * 7. Protocol Structures & Registered Program IDs
 * ========================================================================= */
#define KERNELID                    98
#define R_PROGRAM                   98
#define REPLYID                     99
#define VIDEO_SYSTEM                100
#define AUDIO_SYSTEM                201
#define AUDIO_AGENT                 202
#define VIDEO_AGENT                 300
#define SECURE_SYSTEM               501
#define SECURE_AGENT                502

/* Kernel RPC Command IDs */
#define RPC_DVR_MALLOC              0x1
#define RPC_DVR_FREE                0x2
#define RPC_VCPU_ID_0x102_MALLOC    0x102
#define RPC_VCPU_ID_0x103_FREE      0x103
#define RPC_ACPU_REMOTE_MALLOC      0x300
#define RPC_ACPU_REMOTE_FREE        0x301
#define RPC_KCPU_ID_0x501_FREE      0x501
#define RPC_KCPU_ID_0x503_NOTIFY_PID 0x503

#pragma pack(push, 4)

typedef struct RPC_STRUCT {
    uint32_t programID;     /* Program ID (Big-Endian across ring buffer) */
    uint32_t versionID;     /* Interface Version ID */
    uint32_t procedureID;   /* Procedure / Method ID */
    uint32_t taskID;        /* Caller TID from gettid(), 0 if NON_BLOCK */
    uint32_t sysPID;        /* Callee task / process ID */
    uint32_t parameterSize; /* Payload size in bytes following this header */
    uint32_t mycontext;     /* Return context pointer / cookie */
} RPC_STRUCT;

typedef struct REPLY_RPC_STRUCT {
    RPC_STRUCT rpc;
    uint32_t req_taskID;    /* Target taskID waiting in sigsuspend */
    uint32_t replay_result; /* Return status code (0 = S_OK) */
} REPLY_RPC_STRUCT;

typedef struct RPCRES_LONG {
    int32_t result;         /* 0: S_OK */
    int32_t data;           /* Instance handle / pointer */
} RPCRES_LONG;

typedef struct {
    uint32_t rtdlog_level;     /* 0: Fatal, 1: Error, 2: Warn, 3: Info, 4: Debug */
    uint32_t rtdlog_module;    /* Module bitmask */
    uint32_t rtdlog_module_ex; /* Extended module bitmask */
    uint32_t rtdlog_option;    /* Option flags */
} rtk_rtdlog_info_t;

typedef struct {
    uint32_t cmd;
    uint32_t module;
    uint32_t ratio;
    uint32_t period;
    uint8_t  module_support;
    uint8_t  reserved[3];
} DDR_SSC_PARA;

#pragma pack(pop)

/* =========================================================================
 * 8. Userspace Emulation Mock Dispatcher
 * ========================================================================= */

/*
 * Returns 0 on success, negative error code on failure.
 */
static inline int rtk_emul_handle_ioctl(const char *dev_name, unsigned int cmd, void *arg)
{
    if (!dev_name) return -1;

    /* Handle /dev/rtd-logger */
    if (strstr(dev_name, "rtd-logger")) {
        switch (cmd) {
            case RTDLOG_IOC_LOCK_CMD:
            case RTDLOG_IOC_UNLOCK_CMD:
            case RTDLOG_IOC_KERNEL_SLEEP:
                return 0;
            case RTDLOG_IOC_LOGPATH_CMD:
                if (arg) {
                    char *out_buf = (char *)arg;
                    strncpy(out_buf, RTD_LOGBUF_DEFAULT_DIR, LOGBUF_DIR_MAX_SIZE - 1);
                    out_buf[LOGBUF_DIR_MAX_SIZE - 1] = '\0';
                }
                return 0;
            default:
                return 0;
        }
    }

    /* Handle /dev/sys-misc */
    if (strstr(dev_name, "sys-misc")) {
        switch (cmd) {
            case MISC_SET_WATCHDOG_ENABLE:
                /* Watchdog arm/disarm succeeds cleanly */
                return 0;
            case MISC_GET_RTDLOG:
                if (arg) {
                    rtk_rtdlog_info_t *info = (rtk_rtdlog_info_t *)arg;
                    info->rtdlog_level = 3; /* INFO */
                    info->rtdlog_module = 0xFFFFFFFF;
                    info->rtdlog_module_ex = 0xFFFFFFFF;
                    info->rtdlog_option = 0;
                }
                return 0;
            case MISC_SET_RTDLOG:
            case MISC_SET_DEBUG_LOCK:
            case MISC_WAKEUP_ECPU:
            case MISC_QSM_OFF:
            case MISC_SET_WOL_ENABLE:
                return 0;
            case MISC_GET_WOL_ENABLE:
            case MISC_GET_WARM_WOL_ENABLE:
                if (arg) *(int *)arg = 0;
                return 0;
            case MISC_SET_DDR_SSC_PARAM:
                return 0;
            default:
                return -14; /* -EFAULT matching kernel driver default */
        }
    }

    /* Handle /dev/rpc* */
    if (strstr(dev_name, "rpc")) {
        switch (cmd) {
            case RPC_IOCTHANDLER:
            case RPC_IOCTTIMEOUT:
            case RPC_IOCQTIMEOUT:
            case RPC_IOCTRESET:
            case RPC_IOSTOP:
                return 0;
            default:
                return 0;
        }
    }

    /* Handle /dev/teeproxy */
    if (strstr(dev_name, "teeproxy")) {
        switch (cmd) {
            case TEE_PROXY_IOC_INIT:
            case TEE_PROXY_IOC_DEINIT:
            case TEE_PROXY_IOC_FREE:
            case TEE_PROXY_IOC_KILL:
                return 0;
            case TEE_PROXY_IOC_TEEMGR_PID:
                if (arg) *(pid_t *)arg = 1000;
                return 0;
            default:
                return 0;
        }
    }

    return -25; /* -ENOTTY */
}

#ifdef __cplusplus
}
#endif

#endif /* _RTK_RPC_STUBS_H_ */
