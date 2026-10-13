/*
 * lwipopts.h - settings of the lwIP TCP/IP stack for PicoCalc NetThing: the
 * polled "NO_SYS" flavour with DHCP and DNS, TCP and UDP, no sockets API.
 *
 * Author: Thomas Dzubin
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#include "chip_limits.h"

#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    CHIP_MEM_SIZE   /* the TLS code takes its memory from here too */
#define PBUF_POOL_SIZE              CHIP_PBUF_POOL_SIZE
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define LWIP_ARP                    1
#define ARP_TABLE_SIZE              24      /* the LAN scan reads it after every few requests */
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   1       /* dual stack: the address is made by SLAAC from the router's advertisements */
#define LWIP_ICMP6                  1
#define LWIP_IPV6_MLD               1
#define LWIP_IPV6_AUTOCONFIG        1
#define LWIP_ND6_RDNSS_MAX_DNS_SERVERS 1    /* take an IPv6 DNS server from the router's advertisement */
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_ALTCP                  1       /* one connection API for plain TCP and TLS */
#define LWIP_ALTCP_TLS              1
#define LWIP_ALTCP_TLS_MBEDTLS      1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define TCP_MSS                     1460
#define TCP_WND                     (12 * TCP_MSS)   /* at least the TLS receive buffer (16 KB) */
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_CHKSUM_ALGORITHM       3
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

#endif /* LWIPOPTS_H */
