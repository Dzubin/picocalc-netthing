/*
 * tusb_config.h - the TinyUSB settings: the PicoCalc shows up on the PC as a USB
 * network adapter (CDC NCM) and nothing else.
 *
 * Author: Thomas Dzubin
 */
#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUSB_OS                 OPT_OS_PICO
#define CFG_TUSB_DEBUG              0

#define CFG_TUD_ENABLED             1
#define CFG_TUD_MAX_SPEED           OPT_MODE_DEFAULT_SPEED
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))
#define CFG_TUD_ENDPOINT0_SIZE      64

#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

#define CFG_TUD_ECM_RNDIS           0
#define CFG_TUD_NCM                 1

/* One network transfer block each way, big enough for two full frames. */
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE   (2 * 1460 + 100)
#define CFG_TUD_NCM_OUT_NTB_MAX_SIZE  (2 * 1460 + 100)
#define CFG_TUD_NCM_OUT_NTB_N       1
#define CFG_TUD_NCM_IN_NTB_N        1

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H */
