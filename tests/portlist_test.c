/*
 * portlist_test.c - PC test of portlist.c.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/portlist_test.c portlist.c -o portlist_test && ./portlist_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "portlist.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

int main(void)
{
    uint16_t p[PORTLIST_MAX + 8];

    check("one port", portlist_parse("80", p) == 1 && p[0] == 80);
    check("two ports", portlist_parse("80,443", p) == 2 && p[0] == 80 && p[1] == 443);
    check("spaces and commas", portlist_parse(" 22  ,23 80", p) == 3 && p[2] == 80);
    check("a range", portlist_parse("20-25", p) == 6 && p[0] == 20 && p[5] == 25);
    check("a range and a port", portlist_parse("1-3,80", p) == 4 && p[3] == 80);
    check("top port", portlist_parse("65535", p) == 1 && p[0] == 65535);
    check("zero refused", portlist_parse("0", p) == -1);
    check("too big refused", portlist_parse("65536", p) == -1);
    check("empty refused", portlist_parse("", p) == -1 && portlist_parse(" , ", p) == -1);
    check("letters refused", portlist_parse("80,http", p) == -1 && portlist_parse("8x", p) == -1);
    check("backwards range refused", portlist_parse("25-20", p) == -1);
    check("open range refused", portlist_parse("20-", p) == -1 && portlist_parse("-20", p) == -1);
    check("64 ports are the limit", portlist_parse("1-64", p) == 64 && portlist_parse("1-65", p) == -1);
    check("name of 443", strcmp(portlist_name(443), "HTTPS") == 0);
    check("no name for 4242", portlist_name(4242)[0] == '\0');
    printf("\n%d check(s) failed\n", failures);
    return failures;
}
