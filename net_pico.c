/*
 * net_pico.c - net.h on lwIP. The lwIP callbacks run in the Pico SDK's
 * background context, so the main loop only starts things and polls the
 * volatile state they set; every call into lwIP is bracketed with
 * cyw43_arch_lwip_begin() and cyw43_arch_lwip_end().
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/etharp.h"
#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/icmp6.h"
#include "lwip/ip6.h"
#include "lwip/inet_chksum.h"
#include "lwip/raw.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"

#include "net.h"
#include "net_pico_const.h"

/* ===================================================================== */
/*  Names                                                                 */
/* ===================================================================== */
static ip_addr_t resolved;
static volatile int resolve_state = NET_FAILED;

static void dns_found(const char *name, const ip_addr_t *address, void *arg)
{
    (void)name;
    (void)arg;
    if (address != NULL) {
        resolved = *address;
        resolve_state = NET_DONE;
    } else {
        resolve_state = NET_FAILED;
    }
}

int net_resolve_start(const char *host)
{
    err_t e;

    resolve_state = NET_BUSY;
    cyw43_arch_lwip_begin();
    /* IPv4 first; the AAAA answer is asked for only when there is no A record */
    e = dns_gethostbyname_addrtype(host, &resolved, dns_found, NULL, LWIP_DNS_ADDRTYPE_IPV4_IPV6);
    cyw43_arch_lwip_end();
    if (e == ERR_OK)
        resolve_state = NET_DONE;           /* a dotted address, or already known */
    else if (e != ERR_INPROGRESS)
        resolve_state = NET_FAILED;
    return 0;
}

int net_resolve_poll(char *addr, size_t size)
{
    int state = resolve_state;

    if (state == NET_DONE) {
        char text[NET_ADDR_MAX];

        ipaddr_ntoa_r(&resolved, text, sizeof text);
        snprintf(addr, size, "%s", text);
    }
    return state;
}

/* ===================================================================== */
/*  Ping                                                                  */
/* ===================================================================== */
static struct raw_pcb *ping_pcb;        /* ICMP (IPv4) */
static struct raw_pcb *ping6_pcb;       /* ICMPv6 */
static ip_addr_t ping_target;
static uint16_t ping_seq;
static uint64_t ping_sent_us;
static volatile int ping_state = NET_FAILED;
static volatile unsigned ping_ms;

static char ping_hop[NET_ADDR_MAX];
static bool ping_reached;
static unsigned ping_ttl;

static void ping_answer(const ip_addr_t *from, bool reached)
{
    ping_ms = (unsigned)((time_us_64() - ping_sent_us) / 1000u);
    ipaddr_ntoa_r(from, ping_hop, sizeof ping_hop);
    ping_reached = reached;
    ping_state = NET_DONE;
}

/* Called for every ICMP packet that arrives: take the answers to our own echo requests, which
 * are echo replies, or "time exceeded" and "unreachable" errors that carry the start of the
 * request they answer (the IP header of it, then the first 8 bytes). */
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *from)
{
    struct ip_hdr *ip;
    struct icmp_echo_hdr *icmp;
    unsigned header;

    (void)arg;
    (void)pcb;
    if (ping_state != NET_BUSY || !IP_IS_V4(from) ||
        p->len < sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))
        return 0;
    ip = (struct ip_hdr *)p->payload;
    header = IPH_HL_BYTES(ip);
    if (p->len < header + sizeof(struct icmp_echo_hdr))
        return 0;
    icmp = (struct icmp_echo_hdr *)((u8_t *)p->payload + header);
    if (ICMPH_TYPE(icmp) == ICMP_ER) {
        if (ip_addr_eq(from, &ping_target) && icmp->id == lwip_htons(PING_ID) &&
            icmp->seqno == lwip_htons(ping_seq)) {
            ping_answer(from, true);
            pbuf_free(p);
            return 1;                           /* taken */
        }
    } else if (ICMPH_TYPE(icmp) == ICMP_TE || ICMPH_TYPE(icmp) == ICMP_DUR) {
        const u8_t *inner = (const u8_t *)icmp + sizeof(struct icmp_echo_hdr);
        unsigned inner_header;

        if (p->len >= header + sizeof(struct icmp_echo_hdr) + sizeof(struct ip_hdr)) {
            inner_header = IPH_HL_BYTES((const struct ip_hdr *)inner);
            if (p->len >= header + sizeof(struct icmp_echo_hdr) + inner_header + sizeof(struct icmp_echo_hdr)) {
                const struct icmp_echo_hdr *sent = (const struct icmp_echo_hdr *)(inner + inner_header);

                if (ICMPH_TYPE(sent) == ICMP_ECHO && sent->id == lwip_htons(PING_ID) &&
                    sent->seqno == lwip_htons(ping_seq)) {
                    ping_answer(from, ICMPH_TYPE(icmp) == ICMP_DUR);
                    pbuf_free(p);
                    return 1;
                }
            }
        }
    }
    return 0;                                   /* not ours; lwIP goes on with it */
}

/* The same for ICMPv6: the IPv6 header is a fixed 40 bytes (no extension headers are expected). */
static u8_t ping6_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *from)
{
    const struct icmp6_echo_hdr *icmp;

    (void)arg;
    (void)pcb;
    if (ping_state != NET_BUSY || !IP_IS_V6(from) || p->len < IP6_HLEN + sizeof(struct icmp6_echo_hdr))
        return 0;
    icmp = (const struct icmp6_echo_hdr *)((u8_t *)p->payload + IP6_HLEN);
    if (icmp->type == ICMP6_TYPE_EREP) {
        if (ip_addr_eq(from, &ping_target) && icmp->id == lwip_htons(PING_ID) &&
            icmp->seqno == lwip_htons(ping_seq)) {
            ping_answer(from, true);
            pbuf_free(p);
            return 1;
        }
    } else if ((icmp->type == ICMP6_TYPE_TE || icmp->type == ICMP6_TYPE_DUR) &&
               p->len >= IP6_HLEN + sizeof(struct icmp6_echo_hdr) + IP6_HLEN + sizeof(struct icmp6_echo_hdr)) {
        const struct icmp6_echo_hdr *sent = (const struct icmp6_echo_hdr *)((u8_t *)icmp +
                                            sizeof(struct icmp6_echo_hdr) + IP6_HLEN);

        if (sent->type == ICMP6_TYPE_EREQ && sent->id == lwip_htons(PING_ID) &&
            sent->seqno == lwip_htons(ping_seq)) {
            ping_answer(from, icmp->type == ICMP6_TYPE_DUR);
            pbuf_free(p);
            return 1;
        }
    }
    return 0;
}

void net_ping_set_ttl(unsigned ttl)
{
    ping_ttl = ttl;
}

void net_ping_hop(char *addr, size_t size, bool *reached)
{
    snprintf(addr, size, "%s", ping_hop);
    *reached = ping_reached;
}

int net_ping_start(const char *addr)
{
    struct pbuf *p;
    struct icmp_echo_hdr *echo;
    size_t size = sizeof *echo + PING_DATA_SIZE;
    struct raw_pcb *pcb;
    bool v6;
    unsigned i;
    int result = 0;

    if (!ipaddr_aton(addr, &ping_target))
        return 1;
    v6 = IP_IS_V6(&ping_target);
    cyw43_arch_lwip_begin();
    if (!v6 && ping_pcb == NULL) {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        if (ping_pcb != NULL) {
            raw_recv(ping_pcb, ping_recv, NULL);
            raw_bind(ping_pcb, IP_ADDR_ANY);
        }
    }
    if (v6 && ping6_pcb == NULL) {
        ping6_pcb = raw_new_ip_type(IPADDR_TYPE_V6, IP6_NEXTH_ICMP6);
        if (ping6_pcb != NULL) {
            ping6_pcb->chksum_reqd = 1;         /* lwIP adds the pseudo-header checksum itself */
            ping6_pcb->chksum_offset = 2;
            raw_recv(ping6_pcb, ping6_recv, NULL);
            raw_bind(ping6_pcb, IP_ANY_TYPE);
        }
    }
    pcb = v6 ? ping6_pcb : ping_pcb;
    p = pcb ? pbuf_alloc(PBUF_IP, (u16_t)size, PBUF_RAM) : NULL;
    if (p == NULL) {
        result = 2;
    } else {
        echo = (struct icmp_echo_hdr *)p->payload;
        ICMPH_TYPE_SET(echo, v6 ? ICMP6_TYPE_EREQ : ICMP_ECHO);
        ICMPH_CODE_SET(echo, 0);
        echo->chksum = 0;
        echo->id = lwip_htons(PING_ID);
        echo->seqno = lwip_htons(++ping_seq);
        for (i = 0; i < PING_DATA_SIZE; i++)
            ((char *)echo)[sizeof *echo + i] = (char)(PING_PATTERN + i % 23);
        if (!v6)
            echo->chksum = inet_chksum(echo, (u16_t)size);
        pcb->ttl = ping_ttl ? (u8_t)ping_ttl : IP_DEFAULT_TTL;
        ping_state = NET_BUSY;
        ping_sent_us = time_us_64();
        if (raw_sendto(pcb, p, &ping_target) != ERR_OK) {
            ping_state = NET_FAILED;
            result = 3;
        }
        pbuf_free(p);
    }
    cyw43_arch_lwip_end();
    return result;
}

int net_ping_poll(unsigned *ms)
{
    int state = ping_state;

    if (state == NET_DONE)
        *ms = ping_ms;
    return state;
}

/* ===================================================================== */
/*  TCP                                                                   */
/* ===================================================================== */
static struct tcp_pcb *tcp;
static volatile int tcp_state = NET_FAILED;
static char tcp_out[NET_REPLY_MAX];     /* what we send once connected */
static char tcp_in[NET_REPLY_MAX];
static volatile size_t tcp_in_len;

static void tcp_error(void *arg, err_t e)
{
    (void)arg;
    (void)e;
    tcp = NULL;                         /* lwIP has freed it already */
    if (tcp_state == NET_BUSY)
        tcp_state = NET_FAILED;
}

static err_t tcp_received(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t e)
{
    (void)arg;
    if (p == NULL) {                    /* the server closed the connection */
        tcp_state = NET_DONE;
        return ERR_OK;
    }
    if (e == ERR_OK) {
        struct pbuf *q;

        for (q = p; q != NULL; q = q->next) {
            size_t room = sizeof tcp_in - tcp_in_len;
            size_t n = q->len < room ? q->len : room;

            memcpy(tcp_in + tcp_in_len, q->payload, n);
            tcp_in_len += n;
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_connected(void *arg, struct tcp_pcb *pcb, err_t e)
{
    (void)arg;
    if (e != ERR_OK) {
        tcp_state = NET_FAILED;
        return e;
    }
    if (tcp_write(pcb, tcp_out, (u16_t)strlen(tcp_out), TCP_WRITE_FLAG_COPY) != ERR_OK ||
        tcp_output(pcb) != ERR_OK)
        tcp_state = NET_FAILED;
    return ERR_OK;
}

void net_tcp_stop(void)
{
    cyw43_arch_lwip_begin();
    if (tcp != NULL) {
        tcp_arg(tcp, NULL);
        tcp_err(tcp, NULL);
        tcp_recv(tcp, NULL);
        if (tcp_close(tcp) != ERR_OK)
            tcp_abort(tcp);
        tcp = NULL;
    }
    cyw43_arch_lwip_end();
}

int net_tcp_start(const char *addr, uint16_t port, const char *text)
{
    ip_addr_t target;
    err_t e;

    net_tcp_stop();
    if (!ipaddr_aton(addr, &target))
        return 1;
    snprintf(tcp_out, sizeof tcp_out, "%s", text);
    tcp_in_len = 0;
    tcp_state = NET_BUSY;
    cyw43_arch_lwip_begin();
    tcp = tcp_new_ip_type(IP_GET_TYPE(&target));
    if (tcp == NULL) {
        cyw43_arch_lwip_end();
        tcp_state = NET_FAILED;
        return 2;
    }
    tcp_err(tcp, tcp_error);
    tcp_recv(tcp, tcp_received);
    e = tcp_connect(tcp, &target, port, tcp_connected);
    cyw43_arch_lwip_end();
    if (e != ERR_OK) {
        net_tcp_stop();
        tcp_state = NET_FAILED;
        return 3;
    }
    return 0;
}

int net_tcp_poll(char *reply, size_t size, size_t *len)
{
    size_t n;

    cyw43_arch_lwip_begin();
    n = tcp_in_len;
    if (n > size - 1)
        n = size - 1;
    memcpy(reply, tcp_in, n);
    cyw43_arch_lwip_end();
    reply[n] = '\0';
    *len = n;
    return tcp_state;
}

/* ===================================================================== */
/*  Port probe                                                            */
/* ===================================================================== */
static struct tcp_pcb *probe;
static volatile int probe_state = NET_FAILED;
static volatile unsigned probe_ms;
static uint64_t probe_started_us;

static void probe_error(void *arg, err_t e)
{
    (void)arg;
    (void)e;
    probe = NULL;                       /* lwIP has freed it already */
    if (probe_state == NET_BUSY)
        probe_state = NET_FAILED;
}

static err_t probe_connected(void *arg, struct tcp_pcb *pcb, err_t e)
{
    (void)arg;
    (void)pcb;
    if (e == ERR_OK) {
        probe_ms = (unsigned)((time_us_64() - probe_started_us) / 1000u);
        probe_state = NET_DONE;
    } else {
        probe_state = NET_FAILED;
    }
    return ERR_OK;
}

void net_probe_stop(void)
{
    cyw43_arch_lwip_begin();
    if (probe != NULL) {
        tcp_arg(probe, NULL);
        tcp_err(probe, NULL);
        if (tcp_close(probe) != ERR_OK)
            tcp_abort(probe);
        probe = NULL;
    }
    cyw43_arch_lwip_end();
}

int net_probe_start(const char *addr, uint16_t port)
{
    ip_addr_t target;
    err_t e;

    net_probe_stop();
    if (!ipaddr_aton(addr, &target))
        return 1;
    probe_state = NET_BUSY;
    cyw43_arch_lwip_begin();
    probe = tcp_new_ip_type(IP_GET_TYPE(&target));
    if (probe == NULL) {
        cyw43_arch_lwip_end();
        probe_state = NET_FAILED;
        return 2;
    }
    tcp_err(probe, probe_error);
    probe_started_us = time_us_64();
    e = tcp_connect(probe, &target, port, probe_connected);
    cyw43_arch_lwip_end();
    if (e != ERR_OK) {
        net_probe_stop();
        probe_state = NET_FAILED;
        return 3;
    }
    return 0;
}

int net_probe_poll(unsigned *ms)
{
    int state = probe_state;

    if (state == NET_DONE)
        *ms = probe_ms;
    return state;
}

/* ===================================================================== */
/*  One UDP question and its answer                                       */
/* ===================================================================== */
static struct udp_pcb *xchg_pcb;
static uint8_t xchg_buf[NET_UDP_MAX];
static volatile size_t xchg_len;
static volatile int xchg_state = NET_FAILED;
static volatile unsigned xchg_ms;
static uint64_t xchg_sent_us;

static void xchg_received(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *from, u16_t port)
{
    (void)arg;
    (void)pcb;
    (void)from;
    (void)port;
    if (xchg_state == NET_BUSY) {
        size_t n = p->tot_len < sizeof xchg_buf ? p->tot_len : sizeof xchg_buf;

        pbuf_copy_partial(p, xchg_buf, (u16_t)n, 0);
        xchg_len = n;
        xchg_ms = (unsigned)((time_us_64() - xchg_sent_us) / 1000u);
        xchg_state = NET_DONE;
    }
    pbuf_free(p);
}

void net_udp_stop(void)
{
    cyw43_arch_lwip_begin();
    if (xchg_pcb != NULL) {
        udp_remove(xchg_pcb);
        xchg_pcb = NULL;
    }
    cyw43_arch_lwip_end();
}

int net_udp_start(const char *addr, uint16_t port, const uint8_t *data, size_t length)
{
    ip_addr_t target;
    struct pbuf *p;
    int result = 0;

    net_udp_stop();
    if (!ipaddr_aton(addr, &target) || length > NET_UDP_MAX)
        return 1;
    xchg_state = NET_BUSY;
    cyw43_arch_lwip_begin();
    xchg_pcb = udp_new_ip_type(IP_GET_TYPE(&target));
    p = xchg_pcb ? pbuf_alloc(PBUF_TRANSPORT, (u16_t)length, PBUF_RAM) : NULL;
    if (p == NULL) {
        result = 2;
    } else {
        memcpy(p->payload, data, length);
        udp_recv(xchg_pcb, xchg_received, NULL);
        if (udp_connect(xchg_pcb, &target, port) != ERR_OK) {
            result = 3;
        } else {
            xchg_sent_us = time_us_64();
            if (udp_send(xchg_pcb, p) != ERR_OK)
                result = 4;
        }
        pbuf_free(p);
    }
    cyw43_arch_lwip_end();
    if (result != 0)
        xchg_state = NET_FAILED;
    return result;
}

int net_udp_poll(uint8_t *reply, size_t size, size_t *length, unsigned *ms)
{
    int state = xchg_state;

    if (state == NET_DONE) {
        size_t n = xchg_len < size ? xchg_len : size;

        memcpy(reply, xchg_buf, n);
        *length = n;
        *ms = xchg_ms;
    }
    return state;
}

/* ===================================================================== */
/*  The local network                                                     */
/* ===================================================================== */
bool net_lan_self(uint32_t *ip, uint32_t *mask, uint32_t *gateway, uint8_t mac[6])
{
    struct netif *netif = netif_default;
    bool ok = false;

    cyw43_arch_lwip_begin();
    if (netif != NULL && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        *ip = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(netif)));
        *mask = lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(netif)));
        *gateway = lwip_ntohl(ip4_addr_get_u32(netif_ip4_gw(netif)));
        memcpy(mac, netif->hwaddr, 6);
        ok = true;
    }
    cyw43_arch_lwip_end();
    return ok;
}

int net_arp_ask(uint32_t ip)
{
    ip4_addr_t target;
    err_t e = ERR_IF;

    ip4_addr_set_u32(&target, lwip_htonl(ip));
    cyw43_arch_lwip_begin();
    if (netif_default != NULL)
        e = etharp_request(netif_default, &target);
    cyw43_arch_lwip_end();
    return e == ERR_OK ? 0 : 1;
}

unsigned net_arp_table(net_arp_entry_t *out, unsigned max)
{
    unsigned n = 0;
    size_t i;

    cyw43_arch_lwip_begin();
    for (i = 0; i < ARP_TABLE_SIZE && n < max; i++) {
        ip4_addr_t *ip;
        struct netif *netif;
        struct eth_addr *eth;

        if (etharp_get_entry(i, &ip, &netif, &eth)) {
            out[n].ip = lwip_ntohl(ip4_addr_get_u32(ip));
            memcpy(out[n].mac, eth->addr, 6);
            n++;
        }
    }
    cyw43_arch_lwip_end();
    return n;
}

/* ===================================================================== */
/*  NTP                                                                   */
/* ===================================================================== */
static struct udp_pcb *ntp_pcb;
static volatile int ntp_state = NET_FAILED;
static volatile uint32_t ntp_unix;

static void ntp_received(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *from,
                         u16_t port)
{
    uint8_t seconds[4];

    (void)arg;
    (void)pcb;
    (void)from;
    (void)port;
    if (p->tot_len >= NTP_PACKET_SIZE &&
        pbuf_copy_partial(p, seconds, sizeof seconds, NTP_TRANSMIT_AT) == sizeof seconds) {
        uint32_t since_1900 = ((uint32_t)seconds[0] << 24) | ((uint32_t)seconds[1] << 16) |
                              ((uint32_t)seconds[2] << 8) | seconds[3];

        if (since_1900 > NTP_UNIX_OFFSET) {
            ntp_unix = since_1900 - NTP_UNIX_OFFSET;
            ntp_state = NET_DONE;
        }
    }
    pbuf_free(p);
}

void net_ntp_stop(void)
{
    cyw43_arch_lwip_begin();
    if (ntp_pcb != NULL) {
        udp_remove(ntp_pcb);
        ntp_pcb = NULL;
    }
    cyw43_arch_lwip_end();
}

int net_ntp_start(const char *addr)
{
    ip_addr_t target;
    struct pbuf *p;
    int result = 0;

    net_ntp_stop();
    if (!ipaddr_aton(addr, &target))
        return 1;
    ntp_state = NET_BUSY;
    cyw43_arch_lwip_begin();
    ntp_pcb = udp_new_ip_type(IP_GET_TYPE(&target));
    p = ntp_pcb ? pbuf_alloc(PBUF_TRANSPORT, NTP_PACKET_SIZE, PBUF_RAM) : NULL;
    if (p == NULL) {
        result = 2;
    } else {
        memset(p->payload, 0, NTP_PACKET_SIZE);
        ((uint8_t *)p->payload)[0] = NTP_FIRST_BYTE;
        udp_recv(ntp_pcb, ntp_received, NULL);
        if (udp_sendto(ntp_pcb, p, &target, NTP_PORT) != ERR_OK)
            result = 3;
        pbuf_free(p);
    }
    cyw43_arch_lwip_end();
    if (result != 0)
        ntp_state = NET_FAILED;
    return result;
}

int net_ntp_poll(uint32_t *unix_seconds)
{
    int state = ntp_state;

    if (state == NET_DONE)
        *unix_seconds = ntp_unix;
    return state;
}
