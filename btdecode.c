/*
 * btdecode.c - see btdecode.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "btdecode.h"

void btd_clear(bt_adv_t *adv)
{
    memset(adv, 0, sizeof *adv);
}

static void add_service(bt_adv_t *adv, uint16_t uuid)
{
    unsigned i;

    for (i = 0; i < adv->service_count; i++)
        if (adv->services[i] == uuid)
            return;
    if (adv->service_count < BT_SERVICES_MAX)
        adv->services[adv->service_count++] = uuid;
}

static void note_service(bt_adv_t *adv, uint16_t uuid)
{
    add_service(adv, uuid);
    if (uuid == BT_UUID_EDDYSTONE)
        adv->eddystone = true;
    else if (uuid == BT_UUID_TILE_1 || uuid == BT_UUID_TILE_2)
        adv->tracker = BT_TRACKER_TILE;
    else if (uuid == BT_UUID_SMARTTAG)
        adv->tracker = BT_TRACKER_SMARTTAG;
}

unsigned btd_kind_from_appearance(uint16_t appearance)
{
    switch (appearance >> 6) {
    case 1:  return BT_KIND_PHONE;
    case 2:  return BT_KIND_COMPUTER;
    case 3:  return BT_KIND_WATCH;
    case 8:                         /* generic tag */
    case 9:  return BT_KIND_TRACKER;    /* key ring */
    case 6:                         /* remote control */
    case 15: return BT_KIND_INPUT;      /* human interface device */
    case 12:                        /* thermometer */
    case 13:                        /* heart rate sensor */
    case 14:                        /* blood pressure */
    case 16:                        /* glucose meter */
    case 49:                        /* pulse oximeter */
    case 50: return BT_KIND_HEALTH;     /* weight scale */
    case 17:                        /* running and walking sensor */
    case 18: return BT_KIND_SENSOR;     /* cycling */
    case 37: return BT_KIND_AUDIO;      /* wearable audio device */
    case 0:  return BT_KIND_UNKNOWN;
    default: return BT_KIND_OTHER;
    }
}

unsigned btd_kind_from_class(uint32_t class_of_device)
{
    switch ((class_of_device >> 8) & 0x1F) {
    case 1:  return BT_KIND_COMPUTER;
    case 2:  return BT_KIND_PHONE;
    case 4:  return BT_KIND_AUDIO;      /* audio and video: headsets, speakers, car kits */
    case 5:  return BT_KIND_INPUT;      /* peripheral: keyboard, mouse */
    case 7:  return BT_KIND_WATCH;      /* wearable */
    case 9:  return BT_KIND_HEALTH;
    case 0:  return BT_KIND_UNKNOWN;
    default: return BT_KIND_OTHER;
    }
}

/* Work out the kind from what has been read. */
static void classify(bt_adv_t *adv)
{
    unsigned i, kind = BT_KIND_UNKNOWN;

    if (adv->has_company && adv->company == BT_COMPANY_APPLE && adv->apple_type == BT_APPLE_FIND_MY &&
        adv->tracker == BT_TRACKER_NONE)
        adv->tracker = BT_TRACKER_FIND_MY;
    if (adv->tracker != BT_TRACKER_NONE) {
        adv->kind = BT_KIND_TRACKER;
        return;
    }
    if (adv->ibeacon || adv->eddystone) {
        adv->kind = BT_KIND_BEACON;
        return;
    }
    if (adv->has_appearance)
        kind = btd_kind_from_appearance(adv->appearance);
    for (i = 0; i < adv->service_count && kind == BT_KIND_UNKNOWN; i++) {
        switch (adv->services[i]) {
        case 0x1812: kind = BT_KIND_INPUT; break;
        case 0x180D: case 0x1809: case 0x181D: case 0x1826: kind = BT_KIND_HEALTH; break;
        case 0x181A: case 0x183B: case 0x1816: case 0x1818: kind = BT_KIND_SENSOR; break;
        default: break;
        }
    }
    if (kind == BT_KIND_UNKNOWN && adv->has_company && adv->company == BT_COMPANY_APPLE && adv->apple_type == 0x07)
        kind = BT_KIND_AUDIO;               /* AirPods and Beats announce themselves with type 0x07 */
    adv->kind = kind;
}

void btd_read_adv(const uint8_t *data, size_t length, bt_adv_t *adv)
{
    size_t at = 0;

    while (at + 1 < length) {
        unsigned size = data[at];
        const uint8_t *p = data + at + 2;
        unsigned n;

        if (size == 0 || at + 1 + size > length)
            break;
        n = size - 1;                               /* the bytes after the type */
        switch (data[at + 1]) {
        case BT_AD_FLAGS:
            if (n >= 1) {
                adv->flags = p[0];
                adv->has_flags = true;
            }
            break;
        case BT_AD_UUID16_PARTIAL:
        case BT_AD_UUID16_COMPLETE: {
            unsigned i;

            for (i = 0; i + 1 < n; i += 2)
                note_service(adv, (uint16_t)(p[i] | (p[i + 1] << 8)));
            break;
        }
        case BT_AD_NAME_SHORT:
        case BT_AD_NAME_COMPLETE: {
            unsigned i, m = n < BT_NAME_MAX ? n : BT_NAME_MAX;

            if (adv->name_complete && data[at + 1] == BT_AD_NAME_SHORT)
                break;
            for (i = 0; i < m; i++)
                adv->name[i] = (p[i] >= 0x20 && p[i] < 0x7F) ? (char)p[i] : '?';
            adv->name[m] = '\0';
            adv->has_name = m > 0;
            adv->name_complete = data[at + 1] == BT_AD_NAME_COMPLETE;
            break;
        }
        case BT_AD_TX_POWER:
            if (n >= 1) {
                adv->tx_power = (int8_t)p[0];
                adv->has_tx_power = true;
            }
            break;
        case BT_AD_SERVICE_DATA16:
            if (n >= 2)
                note_service(adv, (uint16_t)(p[0] | (p[1] << 8)));
            break;
        case BT_AD_APPEARANCE:
            if (n >= 2) {
                adv->appearance = (uint16_t)(p[0] | (p[1] << 8));
                adv->has_appearance = true;
            }
            break;
        case BT_AD_MANUFACTURER:
            if (n >= 2) {
                adv->company = (uint16_t)(p[0] | (p[1] << 8));
                adv->has_company = true;
                if (adv->company == BT_COMPANY_APPLE && n >= 3) {
                    adv->apple_type = p[2];
                    if (p[2] == BT_APPLE_IBEACON && n >= 4 && p[3] == 0x15)
                        adv->ibeacon = true;
                }
            }
            break;
        default:
            break;
        }
        at += 1 + size;
    }
    classify(adv);
}

const char *btd_company_name(uint16_t company)
{
    static const struct {
        uint16_t    id;
        const char *name;
    } table[] = BT_COMPANIES;
    size_t i;

    for (i = 0; table[i].id != 0xFFFF; i++)
        if (table[i].id == company)
            return table[i].name;
    return "";
}

const char *btd_service_name(uint16_t uuid16)
{
    static const struct {
        uint16_t    id;
        const char *name;
    } table[] = BT_SERVICES;
    size_t i;

    for (i = 0; table[i].id != 0; i++)
        if (table[i].id == uuid16)
            return table[i].name;
    return "";
}

const char *btd_kind_name(unsigned kind)
{
    static const char *const names[] = BT_KIND_NAMES;

    return kind < sizeof names / sizeof names[0] ? names[kind] : "";
}

const char *btd_tracker_name(unsigned tracker)
{
    static const char *const names[] = BT_TRACKER_NAMES;

    return tracker < sizeof names / sizeof names[0] ? names[tracker] : "";
}

void btd_format_address(const uint8_t address[6], char *out, size_t size)
{
    snprintf(out, size, "%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3],
             address[4], address[5]);
}

bool btd_address_is_private(unsigned address_type, const uint8_t address[6])
{
    /* a random address is static when its top two bits are 11; anything else (01 resolvable, 00
     * non-resolvable) changes over time */
    return address_type != 0 && (address[0] & 0xC0) != 0xC0;
}
