/*
 * lanscan.c - see lanscan.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lanscan.h"

bool lan_parse(const char *text, uint32_t *ip)
{
    uint32_t value = 0;
    unsigned part, digits;

    for (part = 0; part < 4; part++) {
        unsigned n = 0;

        for (digits = 0; text[digits] >= '0' && text[digits] <= '9'; digits++) {
            n = n * 10 + (unsigned)(text[digits] - '0');
            if (n > 255 || digits >= 3)
                return false;
        }
        if (digits == 0)
            return false;
        value = (value << 8) | n;
        text += digits;
        if (part < 3) {
            if (*text != '.')
                return false;
            text++;
        }
    }
    if (*text != '\0')
        return false;
    *ip = value;
    return true;
}

void lan_format(uint32_t ip, char *out, size_t size)
{
    snprintf(out, size, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 255u),
             (unsigned)((ip >> 8) & 255u), (unsigned)(ip & 255u));
}

void lan_default_range(uint32_t ip, uint32_t mask, uint32_t *first, uint32_t *last)
{
    uint32_t network = ip & mask, broadcast = network | ~mask;

    *first = network + 1;
    *last = broadcast - 1;
    if (*last < *first || *last - *first + 1 > 254) {
        *first = (ip & 0xFFFFFF00u) + 1;
        *last = (ip & 0xFFFFFF00u) + 254;
    }
}

bool lan_same_network(uint32_t a, uint32_t b, uint32_t mask)
{
    return (a & mask) == (b & mask);
}

bool lan_mac_private(const uint8_t mac[6])
{
    return (mac[0] & 0x02) != 0;
}

unsigned lan_add_device(lan_device_t *list, unsigned count, unsigned max, uint32_t ip, const uint8_t mac[6])
{
    unsigned i = 0, j;

    while (i < count && list[i].ip < ip)
        i++;
    if (i < count && list[i].ip == ip) {
        memcpy(list[i].mac, mac, 6);
        return count;
    }
    if (count >= max)
        return count;
    for (j = count; j > i; j--)
        list[j] = list[j - 1];
    list[i].ip = ip;
    memcpy(list[i].mac, mac, 6);
    list[i].ping_ms = LAN_NO_PING;
    return count + 1;
}
