/*
 * wifi_pico.c - wifi.h on the Pico 2 W: the CYW43 radio through the Pico SDK's
 * cyw43_arch with the lwIP TCP/IP stack (the threadsafe background flavour: the
 * driver and lwIP run from interrupts, so the main loop only polls).
 *
 * The scan runs in the SDK's background context: cyw43_wifi_scan() returns at
 * once and scan_result() is called for every answer, possibly the same access
 * point several times, so a repeat replaces the earlier entry.
 *
 * Author: Thomas Dzubin
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"

#include "wifi.h"
#include "wifi_pico_const.h"

static wifi_ap_t ap_list[WIFI_AP_MAX];
static volatile unsigned ap_total;      /* entries filled in ap_list */
static bool sorted;                     /* ap_list is in signal order */
static bool radio_up;
static bool joining;                    /* a connection was started and not ended */
static char joined_ssid[WIFI_SSID_MAX + 1];

/* Called by the driver for each answer to the scan. */
static int scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
    unsigned i;
    wifi_ap_t *ap;
    unsigned len;

    (void)env;
    if (result == NULL)
        return 0;
    for (i = 0; i < ap_total; i++)
        if (memcmp(ap_list[i].bssid, result->bssid, sizeof ap_list[i].bssid) == 0)
            break;
    if (i == ap_total) {
        if (ap_total >= WIFI_AP_MAX)
            return 0;                   /* no room: this access point is not listed */
        ap_total = ap_total + 1;
    }
    ap = &ap_list[i];
    len = result->ssid_len;
    if (len > WIFI_SSID_MAX)
        len = WIFI_SSID_MAX;
    memcpy(ap->ssid, result->ssid, len);
    ap->ssid[len] = '\0';
    memcpy(ap->bssid, result->bssid, sizeof ap->bssid);
    ap->rssi = result->rssi;
    ap->channel = (uint8_t)result->channel;
    ap->security = result->auth_mode;
    sorted = false;
    return 0;
}

int wifi_start(void)
{
    if (radio_up)
        return 0;
    if (cyw43_arch_init_with_country(WIFI_COUNTRY) != 0)
        return 1;
    cyw43_arch_enable_sta_mode();
    radio_up = true;
    return 0;
}

int wifi_scan_start(void)
{
    cyw43_wifi_scan_options_t options;

    if (!radio_up)
        return 1;
    memset(&options, 0, sizeof options);
    ap_total = 0;
    sorted = true;
    return cyw43_wifi_scan(&cyw43_state, &options, NULL, scan_result) == 0 ? 0 : 2;
}

bool wifi_scan_busy(void)
{
    return radio_up && cyw43_wifi_scan_active(&cyw43_state);
}

/* strongest signal first (rssi is negative, so a larger value is stronger) */
static int by_signal(const void *a, const void *b)
{
    int x = ((const wifi_ap_t *)a)->rssi;
    int y = ((const wifi_ap_t *)b)->rssi;

    return y - x;
}

unsigned wifi_ap_count(void)
{
    return ap_total;
}

bool wifi_ap_get(unsigned index, wifi_ap_t *out)
{
    if (!sorted && !wifi_scan_busy()) {
        qsort(ap_list, ap_total, sizeof ap_list[0], by_signal);
        sorted = true;
    }
    if (index >= ap_total)
        return false;
    *out = ap_list[index];
    return true;
}

/* ===================================================================== */
/*  Joining a network                                                     */
/* ===================================================================== */
/* The authentication methods to try, by attempt number, for an access point
 * that shows WPA2 (or for a network typed in by hand, whose kind is unknown). */
static const uint32_t wpa2_methods[] = WIFI_METHODS_WPA2;

unsigned wifi_attempts(uint8_t security, bool has_password)
{
    if (!has_password)
        return 1;                       /* open */
    if (security & WIFI_SEC_WPA2)
        return sizeof wpa2_methods / sizeof wpa2_methods[0];
    if (security & WIFI_SEC_WPA)
        return 1;                       /* WPA with TKIP */
    if (security & WIFI_SEC_PRIVACY)
        return 0;                       /* WEP: the driver cannot join it */
    return sizeof wpa2_methods / sizeof wpa2_methods[0];    /* hand-typed, kind unknown */
}

int wifi_connect_start(const char *ssid, const char *password, uint8_t security,
                       bool has_password, unsigned attempt)
{
    uint32_t method;

    if (!radio_up || attempt >= wifi_attempts(security, has_password))
        return 1;
    if (!has_password)
        method = CYW43_AUTH_OPEN;
    else if (!(security & WIFI_SEC_WPA2) && (security & WIFI_SEC_WPA))
        method = CYW43_AUTH_WPA_TKIP_PSK;
    else
        method = wpa2_methods[attempt];
    if (joining)
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    snprintf(joined_ssid, sizeof joined_ssid, "%s", ssid);
    joining = true;
    return cyw43_arch_wifi_connect_async(ssid, has_password ? password : NULL, method) == 0 ? 0 : 2;
}

int wifi_link_state(void)
{
    switch (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA)) {
    case CYW43_LINK_UP:
        return WIFI_LINK_UP;
    case CYW43_LINK_JOIN:
    case CYW43_LINK_NOIP:
        return WIFI_LINK_JOINING;
    case CYW43_LINK_NONET:
        return WIFI_LINK_NO_NETWORK;
    case CYW43_LINK_BADAUTH:
        return WIFI_LINK_BAD_PASSWORD;
    case CYW43_LINK_FAIL:
        return WIFI_LINK_FAILED;
    default:
        return joining ? WIFI_LINK_JOINING : WIFI_LINK_IDLE;
    }
}

bool wifi_connected(void)
{
    return radio_up && wifi_link_state() == WIFI_LINK_UP;
}

void wifi_disconnect(void)
{
    if (radio_up)
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    joining = false;
}

bool wifi_info_get(wifi_info_t *info)
{
    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    int32_t rssi = 0;
    const ip_addr_t *dns;
    int i;

    memset(info, 0, sizeof *info);
    if (!radio_up || wifi_link_state() != WIFI_LINK_UP)
        return false;
    snprintf(info->ssid, sizeof info->ssid, "%s", joined_ssid);
    if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0)
        info->rssi = (int16_t)rssi;
    cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, info->mac);
    cyw43_arch_lwip_begin();
    ip4addr_ntoa_r(netif_ip4_addr(netif), info->ip, sizeof info->ip);
    ip4addr_ntoa_r(netif_ip4_netmask(netif), info->netmask, sizeof info->netmask);
    ip4addr_ntoa_r(netif_ip4_gw(netif), info->gateway, sizeof info->gateway);
    dns = dns_getserver(0);
    ipaddr_ntoa_r(dns, info->dns, sizeof info->dns);
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        const ip6_addr_t *a = netif_ip6_addr(netif, i);

        if (!ip6_addr_ispreferred(netif_ip6_addr_state(netif, i)))
            continue;
        if (ip6_addr_islinklocal(a)) {
            if (info->ip6_local[0] == '\0')
                ip6addr_ntoa_r(a, info->ip6_local, sizeof info->ip6_local);
        } else if (info->ip6[0] == '\0') {
            ip6addr_ntoa_r(a, info->ip6, sizeof info->ip6);
        }
    }
    cyw43_arch_lwip_end();
    return true;
}
