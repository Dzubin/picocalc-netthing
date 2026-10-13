/*
 * btdecode_test.c - PC test of btdecode.c.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/btdecode_test.c btdecode.c -o btdecode_test && ./btdecode_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "btdecode.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

int main(void)
{
    bt_adv_t a;
    char text[BT_ADDR_TEXT_MAX];
    /* flags, complete name "Watch", appearance 0x00C1 (watch), tx power -8 */
    static const uint8_t watch[] = { 2, 0x01, 0x06, 6, 0x09, 'W', 'a', 't', 'c', 'h', 3, 0x19, 0xC1, 0x00, 2, 0x0A, 0xF8 };
    /* heart rate service and battery service, company Garmin, short name */
    static const uint8_t hr[] = { 5, 0x03, 0x0D, 0x18, 0x0F, 0x18, 4, 0xFF, 0x87, 0x00, 0x01, 4, 0x08, 'H', 'R', 'M' };
    /* Apple Find My (type 0x12) */
    static const uint8_t findmy[] = { 5, 0xFF, 0x4C, 0x00, 0x12, 0x19 };
    /* iBeacon */
    static const uint8_t ibeacon[] = { 6, 0xFF, 0x4C, 0x00, 0x02, 0x15, 0x01 };
    /* Tile */
    static const uint8_t tile[] = { 3, 0x03, 0xED, 0xFE };
    /* a damaged record: length runs past the end */
    static const uint8_t bad[] = { 9, 0x09, 'a', 'b' };
    static const uint8_t addr[6] = { 0xA4, 0xCF, 0x12, 0x34, 0x56, 0x78 };
    static const uint8_t rpa[6] = { 0x5A, 1, 2, 3, 4, 5 }, stat[6] = { 0xC1, 1, 2, 3, 4, 5 };

    btd_clear(&a);
    btd_read_adv(watch, sizeof watch, &a);
    check("name", a.has_name && strcmp(a.name, "Watch") == 0 && a.name_complete);
    check("flags", a.has_flags && a.flags == 0x06);
    check("appearance and kind", a.has_appearance && a.appearance == 0xC1 && a.kind == BT_KIND_WATCH);
    check("tx power", a.has_tx_power && a.tx_power == -8);
    btd_clear(&a);
    btd_read_adv(hr, sizeof hr, &a);
    check("services", a.service_count == 2 && a.services[0] == 0x180D && a.services[1] == 0x180F);
    check("company", a.has_company && a.company == 0x0087 && strcmp(btd_company_name(a.company), "GARMIN") == 0);
    check("kind from a heart rate service", a.kind == BT_KIND_HEALTH);
    check("a short name", a.has_name && strcmp(a.name, "HRM") == 0 && !a.name_complete);
    btd_clear(&a);
    btd_read_adv(findmy, sizeof findmy, &a);
    check("find my tracker", a.tracker == BT_TRACKER_FIND_MY && a.kind == BT_KIND_TRACKER);
    btd_clear(&a);
    btd_read_adv(ibeacon, sizeof ibeacon, &a);
    check("ibeacon", a.ibeacon && a.kind == BT_KIND_BEACON);
    btd_clear(&a);
    btd_read_adv(tile, sizeof tile, &a);
    check("tile", a.tracker == BT_TRACKER_TILE && a.kind == BT_KIND_TRACKER);
    btd_clear(&a);
    btd_read_adv(bad, sizeof bad, &a);
    check("a damaged record gives nothing", !a.has_name && a.kind == BT_KIND_UNKNOWN);
    btd_clear(&a);
    btd_read_adv(watch, sizeof watch, &a);
    btd_read_adv(hr, sizeof hr, &a);
    check("a scan response adds to the record", a.service_count == 2 && a.kind == BT_KIND_WATCH);
    check("class of device: phone", btd_kind_from_class(0x5A020C) == BT_KIND_PHONE);
    check("class of device: audio", btd_kind_from_class(0x240404) == BT_KIND_AUDIO);
    check("class of device: keyboard", btd_kind_from_class(0x002540) == BT_KIND_INPUT);
    check("appearance: keyboard", btd_kind_from_appearance(0x03C1) == BT_KIND_INPUT);
    check("appearance: phone", btd_kind_from_appearance(0x0040) == BT_KIND_PHONE);
    check("appearance 0", btd_kind_from_appearance(0) == BT_KIND_UNKNOWN);
    btd_format_address(addr, text, sizeof text);
    check("address text", strcmp(text, "A4:CF:12:34:56:78") == 0);
    check("private addresses", btd_address_is_private(1, rpa) && !btd_address_is_private(1, stat) && !btd_address_is_private(0, rpa));
    check("service names", strcmp(btd_service_name(0x180F), "BATTERY") == 0 && btd_service_name(0x1234)[0] == '\0');
    check("kind names", strcmp(btd_kind_name(BT_KIND_TRACKER), "TRACKER") == 0 && btd_kind_name(99)[0] == '\0');
    printf("\n%d check(s) failed\n", failures);
    return failures;
}
