/*
 * btdecode.h - what a Bluetooth device says about itself, made readable: the
 * advertising data of a Low Energy device (name, flags, services, maker, type),
 * the class of a Classic device, and the names of the common maker and service
 * numbers. Plain portable C; the radio side is bt.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef BTDECODE_H
#define BTDECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "btdecode_const.h"

/* The kinds of device the program tells apart */
#define BT_KIND_UNKNOWN   0
#define BT_KIND_PHONE     1
#define BT_KIND_COMPUTER  2
#define BT_KIND_WATCH     3
#define BT_KIND_AUDIO     4
#define BT_KIND_INPUT     5       /* keyboard, mouse, game controller, remote */
#define BT_KIND_TRACKER   6       /* a tag that can be found with a phone */
#define BT_KIND_BEACON    7
#define BT_KIND_HEALTH    8       /* heart rate, thermometer, scales... */
#define BT_KIND_SENSOR    9
#define BT_KIND_OTHER    10       /* it says what it is, but not one of the above */

/* Which family of tracker, if it is one */
#define BT_TRACKER_NONE     0
#define BT_TRACKER_FIND_MY  1     /* Apple's Find My network (an AirTag, and other tags made for it) */
#define BT_TRACKER_TILE     2
#define BT_TRACKER_SMARTTAG 3     /* Samsung */

typedef struct {
    char     name[BT_NAME_MAX + 1];
    bool     has_name;
    bool     name_complete;
    uint16_t company;             /* the maker's number from the manufacturer data */
    bool     has_company;
    uint8_t  apple_type;          /* the first byte of Apple's data (0x12 Find My, 0x02 iBeacon...) */
    uint8_t  flags;
    bool     has_flags;
    uint16_t appearance;
    bool     has_appearance;
    int8_t   tx_power;
    bool     has_tx_power;
    uint16_t services[BT_SERVICES_MAX];
    unsigned service_count;
    bool     ibeacon;
    bool     eddystone;
    unsigned tracker;             /* BT_TRACKER_ */
    unsigned kind;                /* BT_KIND_ */
} bt_adv_t;

/* Clear a record, then read advertising data into it (call btd_read_adv() again with a scan
 * response to add to the same record). The kind is worked out after every call. */
void btd_clear(bt_adv_t *adv);
void btd_read_adv(const uint8_t *data, size_t length, bt_adv_t *adv);

/* The kind from an appearance value (GAP), or from a class of device (Classic). */
unsigned btd_kind_from_appearance(uint16_t appearance);
unsigned btd_kind_from_class(uint32_t class_of_device);

/* Names for the numbers; NULL (or "") for one that is not in the short lists. */
const char *btd_company_name(uint16_t company);
const char *btd_service_name(uint16_t uuid16);
const char *btd_kind_name(unsigned kind);
const char *btd_tracker_name(unsigned tracker);

/* An address as text, "A4:CF:12:34:56:78" (the six bytes as BTstack keeps them, first byte first);
 * `size` must be at least BT_ADDR_TEXT_MAX. */
void btd_format_address(const uint8_t address[6], char *out, size_t size);

/* True for an address that the device made up for itself (a random one that is not static), which
 * changes from time to time. `address_type` is 0 for a public address, 1 for a random one. */
bool btd_address_is_private(unsigned address_type, const uint8_t address[6]);

#endif /* BTDECODE_H */
