/*
 * bt_pico.c - bt.h on BTstack and the Pico 2 W's CYW43 chip. BTstack runs in the
 * Pico SDK's background context together with lwIP, so every call into it, and
 * every read of the table of devices, is bracketed with cyw43_arch_lwip_begin()
 * and cyw43_arch_lwip_end(); the event handlers run inside that context and only
 * fill in tables and flags, which the main loop polls.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

#include "bt.h"
#include "bt_pico_const.h"
#include "btpan.h"

/* ===================================================================== */
/*  State                                                                 */
/* ===================================================================== */
typedef struct {
    bt_device_t device;
    uint8_t     page_scan_mode;     /* what the inquiry said, for asking the name */
    uint16_t    clock_offset;
    bool        name_tried;
} entry_t;

static entry_t  table[BT_DEVICES_MAX];
static unsigned table_count;

static btstack_packet_callback_registration_t hci_registration;
static bool     stack_started;
static bool     keep_on;                 /* a tether link is up: the radio stays on */
static volatile int power_state = BT_POWER_OFF;
static bool     le_scanning;
static volatile bool inquiry_on;
static volatile bool names_pending;     /* remote name requests are being made after an inquiry */

static uint32_t now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

static void gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

/* ===================================================================== */
/*  The table                                                             */
/* ===================================================================== */
static entry_t *find_entry(const uint8_t address[6])
{
    unsigned i;

    for (i = 0; i < table_count; i++)
        if (memcmp(table[i].device.address, address, 6) == 0)
            return &table[i];
    return NULL;
}

/* The entry for an address, made if there is none; when the table is full the one not heard for
 * the longest time goes (but never one heard in the last BT_KEEP_MS). NULL if there is no room. */
static entry_t *entry_for(const uint8_t address[6], uint8_t address_type)
{
    entry_t *e = find_entry(address);
    uint32_t now = now_ms();

    if (e == NULL) {
        if (table_count < BT_DEVICES_MAX) {
            e = &table[table_count++];
        } else {
            unsigned i, oldest = 0;

            for (i = 1; i < table_count; i++)
                if ((int32_t)(table[i].device.last_ms - table[oldest].device.last_ms) < 0)
                    oldest = i;
            if (now - table[oldest].device.last_ms < BT_KEEP_MS)
                return NULL;
            e = &table[oldest];
        }
        memset(e, 0, sizeof *e);
        memcpy(e->device.address, address, 6);
        e->device.address_type = address_type;
        e->device.first_ms = now;
        e->device.rssi_best = -127;
        btd_clear(&e->device.adv);
    }
    return e;
}

static void hear(entry_t *e, int rssi)
{
    e->device.rssi = (int8_t)(rssi < -127 ? -127 : (rssi > 20 ? 20 : rssi));
    if (e->device.rssi > e->device.rssi_best)
        e->device.rssi_best = e->device.rssi;
    if (e->device.sightings == 0) {
        e->device.rssi_x8 = (int16_t)(e->device.rssi * 8);
        e->device.rssi_avg = e->device.rssi;
        e->device.rssi_sort = e->device.rssi;
    } else {
        /* a running average for the scan screen, which sorts by it in steps of BT_SORT_MARGIN */
        e->device.rssi_x8 = (int16_t)(e->device.rssi_x8 + (e->device.rssi * 8 - e->device.rssi_x8) / BT_SMOOTH_DIV);
        e->device.rssi_avg = (int8_t)((e->device.rssi_x8 >= 0 ? e->device.rssi_x8 + 4 : e->device.rssi_x8 - 4) / 8);
        if (abs(e->device.rssi_avg - e->device.rssi_sort) >= BT_SORT_MARGIN)
            e->device.rssi_sort = e->device.rssi_avg;
    }
    e->device.sightings++;
    e->device.last_ms = now_ms();
}

/* ===================================================================== */
/*  The inspector                                                         */
/* ===================================================================== */
static int          ins_state = BT_INSPECT_IDLE;
static bt_inspect_t ins;
static hci_con_handle_t ins_handle = HCI_CON_HANDLE_INVALID;
static unsigned     ins_step;           /* which read of INSPECT_READS is going */
static bool         ins_discovering;

static void ins_fail(const char *why)
{
    ins.error = why;
    ins_state = BT_INSPECT_FAILED;
}

static void ins_text(char *out, const uint8_t *value, unsigned length)
{
    unsigned i, n = length < BT_TEXT_MAX ? length : BT_TEXT_MAX;

    for (i = 0; i < n; i++)
        out[i] = (value[i] >= 0x20 && value[i] < 0x7F) ? (char)value[i] : '?';
    out[n] = '\0';
}

/* Ask for the next characteristic of the list, or finish and let go of the device. */
static void ins_next_read(void)
{
    static const uint16_t reads[] = INSPECT_READS;

    while (ins_step < sizeof reads / sizeof reads[0]) {
        if (gatt_client_read_value_of_characteristics_by_uuid16(gatt_handler, ins_handle, 0x0001, 0xFFFF,
                                                                reads[ins_step]) == ERROR_CODE_SUCCESS)
            return;
        ins_step++;
    }
    ins_state = BT_INSPECT_DONE;
    if (ins_handle != HCI_CON_HANDLE_INVALID)
        gap_disconnect(ins_handle);
}

static void gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    static const uint16_t reads[] = INSPECT_READS;

    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    switch (hci_event_packet_get_type(packet)) {
    case GATT_EVENT_SERVICE_QUERY_RESULT: {
        gatt_client_service_t service;

        gatt_event_service_query_result_get_service(packet, &service);
        if (service.uuid16 != 0 && ins.service_count < BT_INSPECT_SERVICES)
            ins.services[ins.service_count++] = service.uuid16;
        break;
    }
    case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT: {
        const uint8_t *value = gatt_event_characteristic_value_query_result_get_value(packet);
        unsigned length = gatt_event_characteristic_value_query_result_get_value_length(packet);

        if (ins_step >= sizeof reads / sizeof reads[0])
            break;
        switch (reads[ins_step]) {
        case 0x2A00: ins_text(ins.name, value, length); break;
        case 0x2A29: ins_text(ins.manufacturer, value, length); break;
        case 0x2A24: ins_text(ins.model, value, length); break;
        case 0x2A26: ins_text(ins.firmware, value, length); break;
        case 0x2A19: if (length >= 1) ins.battery = value[0]; break;
        default: break;
        }
        break;
    }
    case GATT_EVENT_QUERY_COMPLETE:
        if (ins_discovering) {
            ins_discovering = false;
            ins_step = 0;
        } else {
            ins_step++;
        }
        ins_next_read();
        break;
    default:
        break;
    }
}

static void ins_connected(hci_con_handle_t handle, uint8_t status)
{
    if (ins_state != BT_INSPECT_CONNECTING)
        return;
    if (status != 0) {
        ins_fail("COULD NOT CONNECT");
        return;
    }
    ins_handle = handle;
    ins_state = BT_INSPECT_READING;
    ins_discovering = true;
    if (gatt_client_discover_primary_services(gatt_handler, handle) != ERROR_CODE_SUCCESS)
        ins_fail("COULD NOT ASK FOR ITS SERVICES");
}

/* ===================================================================== */
/*  Events                                                                */
/* ===================================================================== */
/* Ask the name of the next Classic device that has none; false when there is none left. */
static void request_next_name(void)
{
    unsigned i;

    for (i = 0; i < table_count; i++) {
        entry_t *e = &table[i];

        if (e->device.classic && !e->device.adv.has_name && !e->name_tried) {
            e->name_tried = true;
            if (gap_remote_name_request(e->device.address, e->page_scan_mode, (uint16_t)(e->clock_offset | 0x8000)) == 0)
                return;
        }
    }
    names_pending = false;
}

static void on_advert(const uint8_t *packet)
{
    bd_addr_t address;
    entry_t *e;

    gap_event_advertising_report_get_address(packet, address);
    e = entry_for(address, gap_event_advertising_report_get_address_type(packet));
    if (e == NULL)
        return;
    hear(e, gap_event_advertising_report_get_rssi(packet));
    btd_read_adv(gap_event_advertising_report_get_data(packet), gap_event_advertising_report_get_data_length(packet),
                 &e->device.adv);
}

static void on_inquiry_result(const uint8_t *packet)
{
    bd_addr_t address;
    entry_t *e;

    gap_event_inquiry_result_get_bd_addr(packet, address);
    e = entry_for(address, 0);
    if (e == NULL)
        return;
    e->device.classic = true;
    e->device.class_of_device = gap_event_inquiry_result_get_class_of_device(packet);
    e->page_scan_mode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
    e->clock_offset = gap_event_inquiry_result_get_clock_offset(packet);
    hear(e, gap_event_inquiry_result_get_rssi_available(packet) ? gap_event_inquiry_result_get_rssi(packet) : -100);
    if (e->device.adv.kind == BT_KIND_UNKNOWN)
        e->device.adv.kind = btd_kind_from_class(e->device.class_of_device);
    if (gap_event_inquiry_result_get_name_available(packet)) {
        unsigned n = gap_event_inquiry_result_get_name_len(packet);

        if (n > BT_NAME_MAX)
            n = BT_NAME_MAX;
        memcpy(e->device.adv.name, gap_event_inquiry_result_get_name(packet), n);
        e->device.adv.name[n] = '\0';
        e->device.adv.has_name = n > 0;
    }
}

static void hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING)
            power_state = BT_POWER_READY;
        break;
    case GAP_EVENT_ADVERTISING_REPORT:
        on_advert(packet);
        break;
    case GAP_EVENT_INQUIRY_RESULT:
        on_inquiry_result(packet);
        break;
    case GAP_EVENT_INQUIRY_COMPLETE:
        inquiry_on = false;
        names_pending = true;
        request_next_name();
        break;
    case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
        if (hci_event_remote_name_request_complete_get_status(packet) == 0) {
            bd_addr_t address;
            entry_t *e;

            hci_event_remote_name_request_complete_get_bd_addr(packet, address);
            e = find_entry(address);
            if (e != NULL) {
                const char *name = hci_event_remote_name_request_complete_get_remote_name(packet);
                size_t n = strlen(name);

                if (n > BT_NAME_MAX)
                    n = BT_NAME_MAX;
                memcpy(e->device.adv.name, name, n);
                e->device.adv.name[n] = '\0';
                e->device.adv.has_name = n > 0;
            }
        }
        request_next_name();
        break;
    case HCI_EVENT_LE_META:
        switch (hci_event_le_meta_get_subevent_code(packet)) {
        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            ins_connected(hci_subevent_le_connection_complete_get_connection_handle(packet),
                          hci_subevent_le_connection_complete_get_status(packet));
            break;
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V1:
            ins_connected(hci_subevent_le_enhanced_connection_complete_v1_get_connection_handle(packet),
                          hci_subevent_le_enhanced_connection_complete_v1_get_status(packet));
            break;
        default:
            break;
        }
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        if (hci_event_disconnection_complete_get_connection_handle(packet) == ins_handle) {
            ins_handle = HCI_CON_HANDLE_INVALID;
            if (ins_state == BT_INSPECT_READING)
                ins_fail("THE DEVICE DISCONNECTED");
        }
        break;
    default:
        break;
    }
}

/* ===================================================================== */
/*  The radio                                                             */
/* ===================================================================== */
void bt_power_on(void)
{
    cyw43_arch_lwip_begin();
    if (!stack_started) {
        l2cap_init();
        sm_init();
        sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
        gatt_client_init();
        btpan_init();
        hci_registration.callback = &hci_handler;
        hci_add_event_handler(&hci_registration);
        stack_started = true;
    }
    if (power_state != BT_POWER_READY) {      /* already on: no new state event would come */
        power_state = BT_POWER_STARTING;
        hci_power_control(HCI_POWER_ON);
    }
    cyw43_arch_lwip_end();
}

void bt_keep_on(bool keep)
{
    keep_on = keep;
}

void bt_power_off(void)
{
    cyw43_arch_lwip_begin();
    if (keep_on && stack_started) {            /* only the scans stop */
        if (le_scanning)
            gap_stop_scan();
        if (inquiry_on)
            gap_inquiry_stop();
        le_scanning = false;
        cyw43_arch_lwip_end();
        return;
    }
    if (stack_started) {
        if (le_scanning)
            gap_stop_scan();
        if (inquiry_on)
            gap_inquiry_stop();
        if (ins_handle != HCI_CON_HANDLE_INVALID)
            gap_disconnect(ins_handle);
        hci_power_control(HCI_POWER_OFF);
    }
    le_scanning = false;
    inquiry_on = false;
    names_pending = false;
    ins_state = BT_INSPECT_IDLE;
    ins_handle = HCI_CON_HANDLE_INVALID;
    power_state = BT_POWER_OFF;
    cyw43_arch_lwip_end();
}

int bt_power_state(void)
{
    return power_state;
}

int bt_le_scan_start(void)
{
    int result = 1;

    cyw43_arch_lwip_begin();
    if (power_state == BT_POWER_READY) {
        gap_set_scan_params(1, BT_SCAN_INTERVAL, BT_SCAN_WINDOW, 0);
        gap_set_scan_duplicate_filter(false);
        gap_start_scan();
        le_scanning = true;
        result = 0;
    }
    cyw43_arch_lwip_end();
    return result;
}

void bt_le_scan_stop(void)
{
    cyw43_arch_lwip_begin();
    if (le_scanning && power_state == BT_POWER_READY)
        gap_stop_scan();
    le_scanning = false;
    cyw43_arch_lwip_end();
}

int bt_inquiry_start(unsigned seconds)
{
    int result = 1;
    unsigned units = seconds * 1000u / 1280u;

    if (units < 1)
        units = 1;
    if (units > 30)
        units = 30;
    cyw43_arch_lwip_begin();
    if (power_state == BT_POWER_READY && !inquiry_on && gap_inquiry_start((uint8_t)units) == 0) {
        inquiry_on = true;
        names_pending = false;
        result = 0;
    }
    cyw43_arch_lwip_end();
    return result;
}

void bt_inquiry_stop(void)
{
    cyw43_arch_lwip_begin();
    if (inquiry_on && power_state == BT_POWER_READY)
        gap_inquiry_stop();
    cyw43_arch_lwip_end();
}

bool bt_inquiry_running(void)
{
    return inquiry_on;
}

bool bt_inquiry_busy(void)
{
    return inquiry_on || names_pending;
}

/* ===================================================================== */
/*  The table of devices                                                  */
/* ===================================================================== */
void bt_devices_clear(void)
{
    cyw43_arch_lwip_begin();
    table_count = 0;
    cyw43_arch_lwip_end();
}

unsigned bt_device_count(void)
{
    unsigned n;

    cyw43_arch_lwip_begin();
    n = table_count;
    cyw43_arch_lwip_end();
    return n;
}

bool bt_device_get(unsigned index, bt_device_t *out)
{
    bool ok = false;

    cyw43_arch_lwip_begin();
    if (index < table_count) {
        *out = table[index].device;
        ok = true;
    }
    cyw43_arch_lwip_end();
    return ok;
}

bool bt_device_paired(const uint8_t address[6])
{
    bd_addr_t addr;
    link_key_t key;
    link_key_type_t type;
    bool found;

    memcpy(addr, address, 6);
    cyw43_arch_lwip_begin();
    found = power_state == BT_POWER_READY && gap_get_link_key_for_bd_addr(addr, key, &type);
    cyw43_arch_lwip_end();
    return found;
}

bool bt_device_find(const uint8_t address[6], bt_device_t *out)
{
    bool ok = false;
    entry_t *e;

    cyw43_arch_lwip_begin();
    e = find_entry(address);
    if (e != NULL) {
        *out = e->device;
        ok = true;
    }
    cyw43_arch_lwip_end();
    return ok;
}

/* ===================================================================== */
/*  The inspector                                                         */
/* ===================================================================== */
int bt_inspect_start(const uint8_t address[6], uint8_t address_type)
{
    int result = 1;
    bd_addr_t a;

    memcpy(a, address, 6);
    cyw43_arch_lwip_begin();
    if (power_state == BT_POWER_READY) {
        if (le_scanning) {
            gap_stop_scan();                /* a connection is made while the scan is off */
            le_scanning = false;
        }
        memset(&ins, 0, sizeof ins);
        ins.battery = -1;
        ins_handle = HCI_CON_HANDLE_INVALID;
        ins_state = BT_INSPECT_CONNECTING;
        if (gap_connect(a, (bd_addr_type_t)address_type) == ERROR_CODE_SUCCESS) {
            result = 0;
        } else {
            ins_fail("COULD NOT START");
        }
    }
    cyw43_arch_lwip_end();
    return result;
}

int bt_inspect_poll(bt_inspect_t *out)
{
    int state;

    cyw43_arch_lwip_begin();
    state = ins_state;
    *out = ins;
    cyw43_arch_lwip_end();
    return state;
}

void bt_inspect_stop(void)
{
    cyw43_arch_lwip_begin();
    if (ins_state == BT_INSPECT_CONNECTING)
        gap_connect_cancel();
    if (ins_handle != HCI_CON_HANDLE_INVALID)
        gap_disconnect(ins_handle);
    ins_state = BT_INSPECT_IDLE;
    cyw43_arch_lwip_end();
}
