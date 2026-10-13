/*
 * lan_test.c - PC test of lanscan.c.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/lan_test.c lanscan.c -o lan_test && ./lan_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lanscan.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

int main(void)
{
    uint32_t ip = 0, first, last;
    char text[LAN_TEXT_MAX];
    lan_device_t list[4];
    uint8_t mac_a[6] = { 0xA4, 0xCF, 0x12, 0x34, 0x56, 0x78 }, mac_b[6] = { 0x02, 1, 2, 3, 4, 5 };
    unsigned n = 0;

    check("parse", lan_parse("192.168.1.5", &ip) && ip == 0xC0A80105u);
    check("parse the ends", lan_parse("0.0.0.0", &ip) && ip == 0 && lan_parse("255.255.255.255", &ip) && ip == 0xFFFFFFFFu);
    check("parse refuses", !lan_parse("192.168.1", &ip) && !lan_parse("192.168.1.256", &ip) &&
          !lan_parse("192.168.1.5.6", &ip) && !lan_parse("a.b.c.d", &ip) && !lan_parse("1..2.3", &ip) &&
          !lan_parse("192.168.1.5 ", &ip) && !lan_parse("", &ip) && !lan_parse("1234.1.1.1", &ip));
    lan_format(0xC0A80105u, text, sizeof text);
    check("format", strcmp(text, "192.168.1.5") == 0);
    lan_default_range(0xC0A80105u, 0xFFFFFF00u, &first, &last);
    check("a /24", first == 0xC0A80101u && last == 0xC0A801FEu);
    lan_default_range(0xC0A80145u, 0xFFFFFFC0u, &first, &last);
    check("a /26 is the whole of it", first == 0xC0A80141u && last == 0xC0A8017Eu);
    lan_default_range(0x0A010203u, 0xFFFF0000u, &first, &last);
    check("a /16 is cut to the /24 around us", first == 0x0A010201u && last == 0x0A0102FEu);
    check("same network", lan_same_network(0xC0A80105u, 0xC0A801C8u, 0xFFFFFF00u) &&
          !lan_same_network(0xC0A80105u, 0xC0A80205u, 0xFFFFFF00u));
    check("private mac", lan_mac_private(mac_b) && !lan_mac_private(mac_a));
    n = lan_add_device(list, n, 4, 0xC0A80109u, mac_a);
    n = lan_add_device(list, n, 4, 0xC0A80102u, mac_b);
    n = lan_add_device(list, n, 4, 0xC0A80105u, mac_a);
    check("sorted by address", n == 3 && list[0].ip == 0xC0A80102u && list[1].ip == 0xC0A80105u && list[2].ip == 0xC0A80109u);
    n = lan_add_device(list, n, 4, 0xC0A80105u, mac_b);
    check("no address twice, newer mac kept", n == 3 && memcmp(list[1].mac, mac_b, 6) == 0);
    n = lan_add_device(list, n, 4, 0xC0A80101u, mac_a);
    n = lan_add_device(list, n, 4, 0xC0A80120u, mac_a);
    check("a full list stays full", n == 4 && list[0].ip == 0xC0A80101u && list[3].ip == 0xC0A80109u);
    check("new devices have no ping", list[0].ping_ms == LAN_NO_PING);
    printf("\n%d check(s) failed\n", failures);
    return failures;
}
