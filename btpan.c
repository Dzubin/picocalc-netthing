/*
 * btpan.c - see btpan.h.
 *
 * It follows BTstack's panu_demo: an SDP query finds the phone's network access
 * point (NAP), bnep_lwip_connect() opens the BNEP link, and the lwIP interface
 * that bnep_lwip.c makes then gets an address from the phone with DHCP. BTstack
 * runs in the same background context as lwIP (see bt_pico.c), so its handlers
 * may call lwIP directly; the main loop only polls the state.
 *
 * Author: Thomas Dzubin
 */
#include "chip_limits.h"

#if CHIP_BT_TETHER

#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "btstack.h"
#include "bnep_lwip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"

#include "bt.h"
#include "btpan.h"

static volatile int state = BTPAN_IDLE;
static volatile bool bnep_open;                 /* the BNEP link is open */
static const char *message = "";
static bd_addr_t remote;
static char remote_name[BTPAN_NAME_MAX + 1];
static uint32_t phase_started_ms;

static btstack_packet_callback_registration_t hci_registration;

/* the SDP answer */
static int      record_id = -1;
static uint8_t  attribute_value[BTPAN_ATTR_MAX];
static uint16_t sdp_psm, found_psm;
static uint32_t sdp_uuid, found_uuid;

/* a pairing question waiting for an answer */
static volatile bool pairing_pending;
static uint32_t pairing_number;
static bd_addr_t pairing_addr;

static uint32_t now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

static void set_state(int s, const char *text)
{
    state = s;
    message = text;
    phase_started_ms = now_ms();
}

/* ---- the SDP query for the phone's network service --------------------- */
static void record_complete(void)
{
    /* take the first service, or a NAP if the first one was only a PANU */
    if (found_uuid == 0 || (sdp_uuid == BLUETOOTH_SERVICE_CLASS_NAP && found_uuid == BLUETOOTH_SERVICE_CLASS_PANU)) {
        if (sdp_psm != 0) {
            found_psm = sdp_psm;
            found_uuid = sdp_uuid;
        }
    }
}

static void sdp_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    des_iterator_t des_list_it, prot_it;

    (void)packet_type;
    (void)channel;
    (void)size;
    switch (hci_event_packet_get_type(packet)) {
    case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
        if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id) {
            record_complete();
            record_id = sdp_event_query_attribute_byte_get_record_id(packet);
            sdp_psm = 0;
            sdp_uuid = 0;
        }
        if (sdp_event_query_attribute_byte_get_attribute_length(packet) > sizeof attribute_value)
            break;
        attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
        if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet) + 1) !=
            sdp_event_query_attribute_byte_get_attribute_length(packet))
            break;
        switch (sdp_event_query_attribute_byte_get_attribute_id(packet)) {
        case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
            if (de_get_element_type(attribute_value) != DE_DES)
                break;
            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it);
                 des_iterator_next(&des_list_it)) {
                uint8_t *element = des_iterator_get_element(&des_list_it);
                uint32_t uuid;

                if (de_get_element_type(element) != DE_UUID)
                    continue;
                uuid = de_get_uuid32(element);
                if (uuid == BLUETOOTH_SERVICE_CLASS_PANU || uuid == BLUETOOTH_SERVICE_CLASS_NAP ||
                    uuid == BLUETOOTH_SERVICE_CLASS_GN)
                    sdp_uuid = uuid;
            }
            break;
        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it);
                 des_iterator_next(&des_list_it)) {
                uint8_t *des_element, *element;
                uint32_t uuid;

                if (des_iterator_get_type(&des_list_it) != DE_DES)
                    continue;
                des_element = des_iterator_get_element(&des_list_it);
                des_iterator_init(&prot_it, des_element);
                element = des_iterator_get_element(&prot_it);
                if (element == NULL || de_get_element_type(element) != DE_UUID)
                    continue;
                uuid = de_get_uuid32(element);
                des_iterator_next(&prot_it);
                if (uuid == BLUETOOTH_PROTOCOL_L2CAP && des_iterator_has_more(&prot_it))
                    de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_psm);
            }
            break;
        default:
            break;
        }
        break;
    case SDP_EVENT_QUERY_COMPLETE:
        record_complete();
        if (found_psm != 0) {
            set_state(BTPAN_CONNECT, BTPAN_MSG_CONNECT);
            if (bnep_lwip_connect(remote, found_psm, BLUETOOTH_SERVICE_CLASS_PANU, (uint16_t)found_uuid) != 0)
                set_state(BTPAN_FAILED, BTPAN_MSG_NO_LINK);
        } else {
            set_state(BTPAN_FAILED, BTPAN_MSG_NO_NAP);
        }
        break;
    default:
        break;
    }
}

/* ---- BNEP events (forwarded by bnep_lwip.c) and pairing --------------------- */
static void dhcp_begin(struct netif *netif)
{
    ip4_addr_t none;

    ip4_addr_set_zero(&none);
    netif_set_addr(netif, &none, &none, &none);
    dhcp_start(netif);
}

static void bnep_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    switch (hci_event_packet_get_type(packet)) {
    case BNEP_EVENT_CHANNEL_OPENED:
        if (bnep_event_channel_opened_get_status(packet) != 0) {
            set_state(BTPAN_FAILED, BTPAN_MSG_NO_LINK);
            break;
        }
        bnep_open = true;
        bt_keep_on(true);                       /* the radio stays on while the link is up */
        dhcp_begin(bnep_lwip_get_interface());
        set_state(BTPAN_DHCP, BTPAN_MSG_DHCP);
        break;
    case BNEP_EVENT_CHANNEL_TIMEOUT:
        if (state == BTPAN_CONNECT)
            set_state(BTPAN_FAILED, BTPAN_MSG_NO_LINK);
        break;
    case BNEP_EVENT_CHANNEL_CLOSED:
        bnep_open = false;
        dhcp_stop(bnep_lwip_get_interface());
        netif_set_default(&cyw43_state.netif[CYW43_ITF_STA]);
        bt_keep_on(false);
        if (state == BTPAN_UP || state == BTPAN_DHCP)
            set_state(BTPAN_FAILED, BTPAN_MSG_CLOSED);
        else if (state == BTPAN_CONNECT)
            set_state(BTPAN_FAILED, BTPAN_MSG_NO_LINK);
        else
            state = BTPAN_IDLE;
        break;
    default:
        break;
    }
}

static void hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bd_addr_t addr;

    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    switch (hci_event_packet_get_type(packet)) {
    case HCI_EVENT_PIN_CODE_REQUEST:
        hci_event_pin_code_request_get_bd_addr(packet, addr);
        gap_pin_code_negative(addr);            /* only the newer pairing (SSP) is used */
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        hci_event_user_confirmation_request_get_bd_addr(packet, pairing_addr);
        pairing_number = little_endian_read_32(packet, 8);
        pairing_pending = true;
        break;
    default:
        break;
    }
}

/* ---- the calls of btpan.h ------------------------------------------------------ */
void btpan_init(void)
{
    /* bnep_lwip_init() makes the interface and, unasked, the default one: Wi-Fi's goes back */
    bnep_init();
    bnep_lwip_init();
    netif_set_default(&cyw43_state.netif[CYW43_ITF_STA]);
    bnep_lwip_register_service(BLUETOOTH_SERVICE_CLASS_PANU, BTPAN_BNEP_MTU);
    bnep_lwip_register_packet_handler(bnep_handler);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_bondable_mode(1);
    hci_registration.callback = &hci_handler;
    hci_add_event_handler(&hci_registration);
}

int btpan_start(const uint8_t address[6], const char *name)
{
    int result = 0;

    cyw43_arch_lwip_begin();
    if (state == BTPAN_QUERY || state == BTPAN_CONNECT || state == BTPAN_DHCP || state == BTPAN_UP) {
        result = 1;
    } else {
        memcpy(remote, address, 6);
        snprintf(remote_name, sizeof remote_name, "%s", name);
        record_id = -1;
        sdp_psm = found_psm = 0;
        sdp_uuid = found_uuid = 0;
        pairing_pending = false;
        set_state(BTPAN_QUERY, BTPAN_MSG_QUERY);
        if (sdp_client_query_uuid16(&sdp_handler, remote, BLUETOOTH_SERVICE_CLASS_NAP) != 0)
            set_state(BTPAN_FAILED, BTPAN_MSG_NO_START);
    }
    cyw43_arch_lwip_end();
    return result;
}

void btpan_stop(void)
{
    uint32_t started = now_ms();

    if (state == BTPAN_UP) {
        /* tell the phone's DHCP server the address is free, and give the message time to go */
        cyw43_arch_lwip_begin();
        dhcp_release_and_stop(bnep_lwip_get_interface());
        cyw43_arch_lwip_end();
        sleep_ms(BTPAN_RELEASE_MS);
    }
    cyw43_arch_lwip_begin();
    if (state == BTPAN_CONNECT || state == BTPAN_DHCP || state == BTPAN_UP) {
        bnep_disconnect(remote);
        dhcp_stop(bnep_lwip_get_interface());
    }
    cyw43_arch_lwip_end();
    while (bnep_open && now_ms() - started < BTPAN_CLOSE_MS)      /* let the link close properly */
        sleep_ms(20);
    cyw43_arch_lwip_begin();
    pairing_pending = false;
    netif_set_default(&cyw43_state.netif[CYW43_ITF_STA]);
    state = BTPAN_IDLE;
    message = "";
    cyw43_arch_lwip_end();
    bt_keep_on(false);
}

int btpan_state(void)
{
    uint32_t now = now_ms();

    if (state == BTPAN_IDLE || state == BTPAN_FAILED)
        return state;                   /* before the stack starts there is no lock to take */
    cyw43_arch_lwip_begin();
    if (state == BTPAN_DHCP && dhcp_supplied_address(bnep_lwip_get_interface())) {
        netif_set_default(bnep_lwip_get_interface());   /* the tools now go out this way */
        set_state(BTPAN_UP, BTPAN_MSG_UP);
    } else if (state == BTPAN_QUERY && now - phase_started_ms > BTPAN_QUERY_MS) {
        set_state(BTPAN_FAILED, BTPAN_MSG_NO_ANSWER);
    } else if (state == BTPAN_CONNECT && now - phase_started_ms > BTPAN_CONNECT_MS) {
        bnep_disconnect(remote);
        set_state(BTPAN_FAILED, BTPAN_MSG_NO_LINK);
    } else if (state == BTPAN_DHCP && now - phase_started_ms > BTPAN_DHCP_MS) {
        bnep_disconnect(remote);
        set_state(BTPAN_FAILED, BTPAN_MSG_NO_DHCP);
    }
    cyw43_arch_lwip_end();
    return state;
}

const char *btpan_message(void)
{
    return message;
}

bool btpan_pairing_request(uint32_t *number)
{
    if (!pairing_pending)
        return false;
    *number = pairing_number;
    return true;
}

void btpan_pairing_answer(bool yes)
{
    cyw43_arch_lwip_begin();
    if (pairing_pending) {
        if (yes)
            gap_ssp_confirmation_response(pairing_addr);
        else
            gap_ssp_confirmation_negative(pairing_addr);
        pairing_pending = false;
    }
    cyw43_arch_lwip_end();
}

bool btpan_info(wifi_info_t *info)
{
    struct netif *netif = bnep_lwip_get_interface();
    const ip_addr_t *dns;

    memset(info, 0, sizeof *info);
    if (state != BTPAN_UP)
        return false;
    snprintf(info->ssid, sizeof info->ssid, "%s", remote_name);
    cyw43_arch_lwip_begin();
    ip4addr_ntoa_r(netif_ip4_addr(netif), info->ip, sizeof info->ip);
    ip4addr_ntoa_r(netif_ip4_netmask(netif), info->netmask, sizeof info->netmask);
    ip4addr_ntoa_r(netif_ip4_gw(netif), info->gateway, sizeof info->gateway);
    dns = dns_getserver(0);
    ipaddr_ntoa_r(dns, info->dns, sizeof info->dns);
    cyw43_arch_lwip_end();
    memcpy(info->mac, netif->hwaddr, 6);
    return true;
}

#endif /* CHIP_BT_TETHER */
