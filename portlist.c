/*
 * portlist.c - see portlist.h.
 *
 * Author: Thomas Dzubin
 */
#include <stddef.h>

#include "portlist.h"

/* Read a port number at *p (1 to 65535) and move on; returns 0 if there is none. */
static unsigned read_number(const char **p)
{
    unsigned long n = 0;
    const char *q = *p;

    if (*q < '0' || *q > '9')
        return 0;
    while (*q >= '0' && *q <= '9') {
        n = n * 10 + (unsigned long)(*q - '0');
        if (n > 65535)
            return 0;
        q++;
    }
    *p = q;
    return (unsigned)n;
}

int portlist_parse(const char *text, uint16_t *out)
{
    int count = 0;
    const char *p = text;

    for (;;) {
        unsigned first, last, port;

        while (*p == ',' || *p == ' ')
            p++;
        if (*p == '\0')
            break;
        first = read_number(&p);
        if (first == 0)
            return -1;
        last = first;
        if (*p == '-') {
            p++;
            last = read_number(&p);
            if (last == 0 || last < first)
                return -1;
        }
        if (*p != '\0' && *p != ',' && *p != ' ')
            return -1;
        for (port = first; port <= last; port++) {
            if (count >= PORTLIST_MAX)
                return -1;
            out[count++] = (uint16_t)port;
        }
    }
    return count > 0 ? count : -1;
}

const char *portlist_name(uint16_t port)
{
    static const struct {
        uint16_t    port;
        const char *name;
    } names[] = PORTLIST_NAMES;
    size_t i;

    for (i = 0; names[i].port != 0; i++)
        if (names[i].port == port)
            return names[i].name;
    return "";
}
