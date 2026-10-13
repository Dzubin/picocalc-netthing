/*
 * usbnet.c - see usbnet.h.
 *
 * TinyUSB runs from the cyw43_arch async context (the same one lwIP runs in), so
 * its callbacks may call lwIP at once: a "when pending" worker runs tud_task()
 * whenever TinyUSB queues an event, and an "at time" worker looks at the state
 * every USBNET_TICK_MS. Frames arriving from the PC go straight into lwIP; frames
 * lwIP sends go out through tud_network_xmit(), or are dropped (TCP sends them
 * again) when the USB side has no room.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/async_context.h"
#include "pico/unique_id.h"
#include "pico/time.h"
#include "tusb.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip6_addr.h"

#include "usbnet.h"

uint8_t tud_network_mac_address[6];

static struct netif usb_netif;
static bool started, netif_added, dhcp_running, tick_added;
static volatile int state = USBNET_OFF;
static uint32_t dhcp_started_ms;
static volatile uint32_t event_count;      /* USB events TinyUSB has queued (for the status text) */
static async_when_pending_worker_t usb_worker;
static async_at_time_worker_t      tick_worker;

/* ---- frames --------------------------------------------------------- */
static err_t usb_linkoutput(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    if (!tud_ready())
        return ERR_USE;
    if (!tud_network_can_xmit(p->tot_len))
        return ERR_MEM;
    tud_network_xmit(p, 0);
    return ERR_OK;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    struct pbuf *p = (struct pbuf *)ref;

    (void)arg;
    return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    struct pbuf *p;

    if (!netif_added || !netif_is_up(&usb_netif) || size == 0) {
        tud_network_recv_renew();
        return true;
    }
    p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p == NULL) {
        tud_network_recv_renew();
        return true;                    /* no buffer: the frame is dropped */
    }
    pbuf_take(p, src, size);
    if (usb_netif.input(p, &usb_netif) != ERR_OK)
        pbuf_free(p);
    tud_network_recv_renew();           /* the frame is copied: the USB side may take the next one */
    return true;
}

void tud_network_init_cb(void)
{
}

/* ---- the lwIP interface ----------------------------------------------- */
static err_t usb_output_ip4(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr)
{
    return etharp_output(netif, p, addr);
}

static err_t usb_netif_init(struct netif *netif)
{
    netif->mtu = USBNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
    netif->state = NULL;
    netif->name[0] = 'U';
    netif->name[1] = 'S';
    netif->linkoutput = usb_linkoutput;
    netif->output = usb_output_ip4;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif
    return ERR_OK;
}

static void make_mac(void)
{
    pico_unique_board_id_t id;

    pico_get_unique_board_id(&id);
    tud_network_mac_address[0] = 0x02;      /* a locally administered address */
    memcpy(tud_network_mac_address + 1, id.id + 3, 5);
}

/* ---- keeping the state up to date --------------------------------------- */
static void dhcp_begin(void)
{
    if (dhcp_running)
        dhcp_stop(&usb_netif);
    dhcp_start(&usb_netif);
    dhcp_running = true;
    dhcp_started_ms = to_ms_since_boot(get_absolute_time());
}

static void update_state(void)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!started) {
        state = USBNET_OFF;
        return;
    }
    if (!tud_mounted()) {
        if (dhcp_running) {
            dhcp_stop(&usb_netif);
            dhcp_running = false;
        }
        netif_set_link_down(&usb_netif);
        state = USBNET_WAITING;
        return;
    }
    netif_set_link_up(&usb_netif);
    if (dhcp_supplied_address(&usb_netif)) {
        if (state != USBNET_UP)
            netif_set_default(&usb_netif);      /* the tools now go out this way */
        state = USBNET_UP;
        return;
    }
    if (!dhcp_running || now - dhcp_started_ms > USBNET_DHCP_RETRY_MS)
        dhcp_begin();
    state = USBNET_LINK;
}

static void usb_do_work(async_context_t *context, async_when_pending_worker_t *worker)
{
    (void)context;
    (void)worker;
    tud_task();
    update_state();
}

static void tick_do_work(async_context_t *context, async_at_time_worker_t *worker)
{
    tud_task();                         /* in case an event came before the worker was ready */
    update_state();
    if (started)
        async_context_add_at_time_worker_in_ms(context, worker, state == USBNET_UP ? USBNET_TICK_MS : USBNET_FAST_TICK_MS);
}

/* TinyUSB calls this (also from its interrupt) when an event is queued. */
void tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr)
{
    (void)rhport;
    (void)eventid;
    (void)in_isr;
    event_count++;
    if (started)
        async_context_set_work_pending(cyw43_arch_async_context(), &usb_worker);
}

/* ---- the calls of usbnet.h ------------------------------------------------ */
int usbnet_start(void)
{
    async_context_t *context = cyw43_arch_async_context();
    ip4_addr_t none;

    if (started)
        return 0;
    make_mac();
    cyw43_arch_lwip_begin();
    /* The workers go in first: TinyUSB lets the PC start enumerating inside tusb_init(). */
    usb_worker.do_work = usb_do_work;
    async_context_add_when_pending_worker(context, &usb_worker);
    started = true;
    state = USBNET_WAITING;
    if (!tusb_inited()) {
        tusb_rhport_init_t init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };

        if (!tusb_init(0, &init)) {
            started = false;
            state = USBNET_OFF;
            async_context_remove_when_pending_worker(context, &usb_worker);
            cyw43_arch_lwip_end();
            return 1;
        }
    }
    if (!netif_added) {
        ip4_addr_set_zero(&none);
        memcpy(usb_netif.hwaddr, tud_network_mac_address, 6);
        usb_netif.hwaddr_len = 6;
        usb_netif.hwaddr[5] ^= 0x01;        /* not the same as the address the PC sees */
        netif_add(&usb_netif, &none, &none, &none, NULL, usb_netif_init, netif_input);
        netif_added = true;
    }
    netif_set_up(&usb_netif);
    tud_connect();
    async_context_set_work_pending(context, &usb_worker);
    if (!tick_added) {
        tick_worker.do_work = tick_do_work;
        tick_added = true;
    }
    async_context_add_at_time_worker_in_ms(context, &tick_worker, USBNET_FAST_TICK_MS);
    cyw43_arch_lwip_end();
    return 0;
}

void usbnet_stop(void)
{
    async_context_t *context = cyw43_arch_async_context();

    if (!started)
        return;
    if (dhcp_running && dhcp_supplied_address(&usb_netif)) {
        /* tell the PC's DHCP server the address is free, and give the message time to go */
        cyw43_arch_lwip_begin();
        dhcp_release_and_stop(&usb_netif);
        dhcp_running = false;
        cyw43_arch_lwip_end();
        sleep_ms(USBNET_RELEASE_MS);
    }
    cyw43_arch_lwip_begin();
    started = false;
    tud_disconnect();
    if (dhcp_running) {
        dhcp_stop(&usb_netif);
        dhcp_running = false;
    }
    netif_set_link_down(&usb_netif);
    netif_set_down(&usb_netif);
    netif_set_default(&cyw43_state.netif[CYW43_ITF_STA]);   /* Wi-Fi's interface again */
    async_context_remove_when_pending_worker(context, &usb_worker);
    async_context_remove_at_time_worker(context, &tick_worker);
    state = USBNET_OFF;
    cyw43_arch_lwip_end();
}

int usbnet_state(void)
{
    return state;
}

void usbnet_debug_text(char *out, unsigned size)
{
    snprintf(out, size, "EVENTS %lu  BUS %s  PC %s", (unsigned long)event_count, tud_connected() ? "YES" : "NO",
             tud_mounted() ? "READY" : "NO");
}

bool usbnet_info(wifi_info_t *info)
{
    const ip_addr_t *dns;

    memset(info, 0, sizeof *info);
    if (state != USBNET_UP)
        return false;
    snprintf(info->ssid, sizeof info->ssid, "USB");
    cyw43_arch_lwip_begin();
    ip4addr_ntoa_r(netif_ip4_addr(&usb_netif), info->ip, sizeof info->ip);
    ip4addr_ntoa_r(netif_ip4_netmask(&usb_netif), info->netmask, sizeof info->netmask);
    ip4addr_ntoa_r(netif_ip4_gw(&usb_netif), info->gateway, sizeof info->gateway);
    dns = dns_getserver(0);
    ipaddr_ntoa_r(dns, info->dns, sizeof info->dns);
    cyw43_arch_lwip_end();
    memcpy(info->mac, usb_netif.hwaddr, 6);
    return true;
}
