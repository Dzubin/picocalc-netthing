/*
 * lanscan.h - the arithmetic of a scan of the local network: IPv4 addresses
 * as numbers, the range to scan, and the list of devices found, sorted. Plain
 * portable C; the ARP requests and the pings are net.h's job.
 *
 * Author: Thomas Dzubin
 */
#ifndef LANSCAN_H
#define LANSCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LAN_MAX_ADDRESSES  1024     /* the most addresses one scan covers */
#define LAN_MAX_DEVICES     128     /* the most devices kept in the list */
#define LAN_TEXT_MAX         16     /* "255.255.255.255" and the end */
#define LAN_NO_PING        (-1)     /* a device that did not answer a ping */

typedef struct {
    uint32_t ip;                    /* host order: 192.168.1.5 is 0xC0A80105 */
    uint8_t  mac[6];
    int16_t  ping_ms;               /* LAN_NO_PING if none */
} lan_device_t;

/* "192.168.1.5" to a number. Returns false for anything else (four decimal parts of 0 to 255). */
bool lan_parse(const char *text, uint32_t *ip);

/* A number to "192.168.1.5"; `size` must be at least LAN_TEXT_MAX. */
void lan_format(uint32_t ip, char *out, size_t size);

/* The range worth scanning from our address and mask: the hosts of the network, but never more
 * than the 254 addresses of the /24 around our own. */
void lan_default_range(uint32_t ip, uint32_t mask, uint32_t *first, uint32_t *last);

/* True if both addresses are on the same network. */
bool lan_same_network(uint32_t a, uint32_t b, uint32_t mask);

/* True for a "private" (locally administered) MAC address, which a phone or computer makes up
 * for itself and which says nothing about who made it. */
bool lan_mac_private(const uint8_t mac[6]);

/* Add a device to a list of `count` (kept sorted by address, no address twice; an address that
 * is there keeps its place and gets the newer MAC). Returns the new count, which stays the same
 * when the list is full. */
unsigned lan_add_device(lan_device_t *list, unsigned count, unsigned max, uint32_t ip, const uint8_t mac[6]);

#endif /* LANSCAN_H */
