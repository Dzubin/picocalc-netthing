/*
 * bt.h - Bluetooth as the tools see it: switch the radio on, scan for Low Energy
 * devices and for Classic ones, keep a table of what was heard, and connect to
 * one Low Energy device to read what it says about itself. Every call starts
 * something and returns at once; the caller polls. The implementation is
 * bt_pico.c (BTstack on the Pico 2 W's CYW43 chip).
 *
 * Author: Thomas Dzubin
 */
#ifndef BT_H
#define BT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "btdecode.h"
#include "chip_limits.h"

#define BT_DEVICES_MAX     CHIP_BT_DEVICES_MAX
#define BT_TEXT_MAX        24       /* a name, maker or model read from a device */
#define BT_INSPECT_SERVICES 12

/* The radio */
#define BT_POWER_OFF       0
#define BT_POWER_STARTING  1        /* the firmware is being loaded (a second or two) */
#define BT_POWER_READY     2
#define BT_POWER_FAILED    3

typedef struct {
    uint8_t  address[6];
    uint8_t  address_type;          /* 0 public, 1 random (Low Energy) */
    bool     classic;               /* found by a Classic inquiry */
    int8_t   rssi;                  /* the latest signal, dBm */
    int8_t   rssi_best;             /* the strongest heard */
    uint32_t sightings;             /* adverts or inquiry results heard */
    uint32_t first_ms, last_ms;     /* when it was first and last heard (the platform clock) */
    uint32_t class_of_device;       /* Classic */
    bt_adv_t adv;                   /* what it advertises (a Classic device has only its name) */
} bt_device_t;

/* Switch the radio on (loads its firmware) or off. bt_power_state() is polled until it is no longer
 * BT_POWER_STARTING. Switching it off stops any scan. */
void bt_power_on(void);
void bt_power_off(void);

/* While true the radio is not switched off by bt_power_off() (only the scans stop): a tether link needs it. */
void bt_keep_on(bool keep);
int  bt_power_state(void);

/* Scan for Low Energy devices (active: the answers to scan requests bring names) until stopped. */
int  bt_le_scan_start(void);
void bt_le_scan_stop(void);

/* A Classic inquiry of about `seconds` seconds; bt_inquiry_running() goes false when it is over, and
 * the names of devices that did not give one are asked for after that (bt_inquiry_busy()). */
int  bt_inquiry_start(unsigned seconds);
void bt_inquiry_stop(void);
bool bt_inquiry_running(void);
bool bt_inquiry_busy(void);

/* The table of what was heard. */
void     bt_devices_clear(void);
unsigned bt_device_count(void);
bool     bt_device_get(unsigned index, bt_device_t *out);
bool     bt_device_find(const uint8_t address[6], bt_device_t *out);

/* True if the device is paired with this PicoCalc (a link key is saved for it). */
bool     bt_device_paired(const uint8_t address[6]);

/* Connect to a Low Energy device and read its services, name, maker, model, firmware and battery. */
#define BT_INSPECT_IDLE        0
#define BT_INSPECT_CONNECTING  1
#define BT_INSPECT_READING     2
#define BT_INSPECT_DONE        3
#define BT_INSPECT_FAILED      4

typedef struct {
    char     name[BT_TEXT_MAX + 1];
    char     manufacturer[BT_TEXT_MAX + 1];
    char     model[BT_TEXT_MAX + 1];
    char     firmware[BT_TEXT_MAX + 1];
    int      battery;               /* percent, or -1 if it has none */
    uint16_t services[BT_INSPECT_SERVICES];
    unsigned service_count;
    const char *error;              /* why it failed */
} bt_inspect_t;

int  bt_inspect_start(const uint8_t address[6], uint8_t address_type);
int  bt_inspect_poll(bt_inspect_t *out);
void bt_inspect_stop(void);

#endif /* BT_H */
