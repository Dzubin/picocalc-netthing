/*
 * dnsquery_test.c - PC test of dnsquery.c.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/dnsquery_test.c dnsquery.c -o dnsquery_test && ./dnsquery_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "dnsquery.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

int main(void)
{
    uint8_t q[128];
    size_t n;
    char text[DNSQ_TEXT_MAX];
    dnsq_answer_t a;
    /* a reply for example.com A: 93.184.216.34, name compressed with a pointer, plus a CNAME first */
    static const uint8_t reply[] = {
        0x12, 0x34, 0x81, 0x80, 0, 1, 0, 2, 0, 0, 0, 0,
        7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, 0, 1, 0, 1,
        0xC0, 12, 0, 5, 0, 1, 0, 0, 0, 60, 0, 2, 0xC0, 12,                         /* CNAME (skipped) */
        0xC0, 12, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 93, 184, 216, 34 };
    static const uint8_t nx[] = { 0x12, 0x34, 0x81, 0x83, 0, 1, 0, 0, 0, 0, 0, 0,
                                  3, 'n', 'o', 'p', 0, 0, 1, 0, 1 };
    uint8_t v6[16] = { 0x26, 0x06, 0x28, 0x00, 0x02, 0x20, 0, 1, 0x02, 0x48, 0x18, 0x93, 0x25, 0xc8, 0x19, 0x46 };
    uint8_t loop[16] = { 0 }, ll[16] = { 0xfe, 0x80 }, one_zero[16] = { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };

    n = dnsq_build("example.com", DNSQ_TYPE_A, 0x1234, q, sizeof q);
    check("query length", n == 12 + 13 + 4);
    check("query header", q[0] == 0x12 && q[1] == 0x34 && q[2] == 0x01 && q[3] == 0x00 && q[5] == 1);
    check("query labels", q[12] == 7 && memcmp(q + 13, "example", 7) == 0 && q[20] == 3 && q[24] == 0);
    check("query type and class", q[25] == 0 && q[26] == 1 && q[27] == 0 && q[28] == 1);
    n = dnsq_build("example.com.", DNSQ_TYPE_AAAA, 1, q, sizeof q);
    check("a trailing dot is fine, AAAA type", n == 29 && q[26] == 28);
    check("bad names refused", dnsq_build("", DNSQ_TYPE_A, 1, q, sizeof q) == 0 &&
          dnsq_build("a..b", DNSQ_TYPE_A, 1, q, sizeof q) == 0 && dnsq_build(".com", DNSQ_TYPE_A, 1, q, sizeof q) == 0 &&
          dnsq_build("a.com..", DNSQ_TYPE_A, 1, q, sizeof q) == 0);
    check("a label of 64 refused", dnsq_build("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com", DNSQ_TYPE_A, 1, q, sizeof q) == 0);
    check("a reply", dnsq_parse(reply, sizeof reply, 0x1234, DNSQ_TYPE_A, &a) && a.rcode == 0 && a.count == 1 &&
          a.addr[0][0] == 93 && a.addr[0][3] == 34);
    check("the same reply asked as AAAA has none", dnsq_parse(reply, sizeof reply, 0x1234, DNSQ_TYPE_AAAA, &a) && a.count == 0);
    check("wrong id refused", !dnsq_parse(reply, sizeof reply, 0x1235, DNSQ_TYPE_A, &a));
    check("a truncated reply refused", !dnsq_parse(reply, sizeof reply - 2, 0x1234, DNSQ_TYPE_A, &a) && !dnsq_parse(reply, 5, 0x1234, DNSQ_TYPE_A, &a));
    check("nxdomain", dnsq_parse(nx, sizeof nx, 0x1234, DNSQ_TYPE_A, &a) && a.rcode == DNSQ_RCODE_NXDOMAIN && a.count == 0);
    check("a query is not an answer", !dnsq_parse(q, n, 1, DNSQ_TYPE_AAAA, &a));
    (void)dnsq_parse(reply, sizeof reply, 0x1234, DNSQ_TYPE_A, &a);
    dnsq_format(DNSQ_TYPE_A, a.addr[0], text, sizeof text);
    check("format A", strcmp(text, "93.184.216.34") == 0);
    dnsq_format(DNSQ_TYPE_AAAA, v6, text, sizeof text);
    check("format AAAA", strcmp(text, "2606:2800:220:1:248:1893:25c8:1946") == 0);
    dnsq_format(DNSQ_TYPE_AAAA, loop, text, sizeof text);
    check("format ::", strcmp(text, "::") == 0);
    ll[15] = 1;
    dnsq_format(DNSQ_TYPE_AAAA, ll, text, sizeof text);
    check("format fe80::1", strcmp(text, "fe80::1") == 0);
    dnsq_format(DNSQ_TYPE_AAAA, one_zero, text, sizeof text);
    check("a single zero group is not shortened", strcmp(text, "2001:db8:0:1:1:1:1:1") == 0);
    printf("\n%d check(s) failed\n", failures);
    return failures;
}
