/*
 * dnsquery.c - see dnsquery.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "dnsquery.h"
#include "dnsquery_const.h"

static void put16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static unsigned get16(const uint8_t *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

size_t dnsq_build(const char *name, uint16_t type, uint16_t id, uint8_t *out, size_t max)
{
    size_t n = DNSQ_HEADER_LEN, label_at, len = strlen(name);
    const char *p = name;

    if (len == 0 || len > 253 || max < DNSQ_HEADER_LEN + len + 2 + 4)
        return 0;
    if (p[len - 1] == '.')
        len--;
    memset(out, 0, DNSQ_HEADER_LEN);
    put16(out, id);
    put16(out + 2, DNSQ_FLAG_RD);
    put16(out + 4, 1);                  /* one question */
    while (len > 0) {
        size_t label = 0;

        label_at = n++;
        while (label < len && p[label] != '.') {
            out[n++] = (uint8_t)p[label];
            label++;
        }
        if (label == 0 || label > 63)
            return 0;
        out[label_at] = (uint8_t)label;
        p += label;
        len -= label;
        if (len > 0) {
            p++;                        /* the dot */
            len--;
            if (len == 0)
                return 0;               /* a name ending in two dots */
        }
    }
    out[n++] = 0;
    put16(out + n, type);
    put16(out + n + 2, DNSQ_CLASS_IN);
    return n + 4;
}

/* Move past a name at message[*at] (labels, or a pointer that ends it). False if it runs off the end. */
static bool skip_name(const uint8_t *message, size_t length, size_t *at)
{
    size_t p = *at;

    for (;;) {
        unsigned c;

        if (p >= length)
            return false;
        c = message[p];
        if (c == 0) {
            *at = p + 1;
            return true;
        }
        if ((c & 0xC0) == 0xC0) {
            if (p + 2 > length)
                return false;
            *at = p + 2;
            return true;
        }
        if ((c & 0xC0) != 0)
            return false;
        p += 1 + c;
    }
}

bool dnsq_parse(const uint8_t *message, size_t length, uint16_t id, uint16_t type, dnsq_answer_t *out)
{
    unsigned flags, questions, answers, i;
    size_t at = DNSQ_HEADER_LEN;

    memset(out, 0, sizeof *out);
    if (length < DNSQ_HEADER_LEN || get16(message) != id)
        return false;
    flags = get16(message + 2);
    if (!(flags & DNSQ_FLAG_RESPONSE))
        return false;
    out->rcode = (int)(flags & 0x0F);
    questions = get16(message + 4);
    answers = get16(message + 6);
    for (i = 0; i < questions; i++) {
        if (!skip_name(message, length, &at) || at + 4 > length)
            return false;
        at += 4;
    }
    for (i = 0; i < answers; i++) {
        unsigned rtype, rclass, rdlength;

        if (!skip_name(message, length, &at) || at + 10 > length)
            return false;
        rtype = get16(message + at);
        rclass = get16(message + at + 2);
        rdlength = get16(message + at + 8);
        at += 10;
        if (at + rdlength > length)
            return false;
        if (rtype == type && rclass == DNSQ_CLASS_IN && rdlength == (type == DNSQ_TYPE_A ? 4u : 16u)) {
            if (out->count < DNSQ_ADDRS_MAX)
                memcpy(out->addr[out->count], message + at, rdlength);
            out->count++;
        }
        at += rdlength;
    }
    if (out->count > DNSQ_ADDRS_MAX)
        out->count = DNSQ_ADDRS_MAX;
    return true;
}

void dnsq_format(uint16_t type, const uint8_t addr[16], char *out, size_t size)
{
    unsigned group[8], i, best_start = 0, best_len = 0, run_start = 0, run_len = 0;
    size_t n = 0;

    if (type == DNSQ_TYPE_A) {
        snprintf(out, size, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
        return;
    }
    for (i = 0; i < 8; i++)
        group[i] = ((unsigned)addr[2 * i] << 8) | addr[2 * i + 1];
    for (i = 0; i < 8; i++) {
        if (group[i] == 0) {
            if (run_len == 0)
                run_start = i;
            run_len++;
            if (run_len > best_len) {
                best_len = run_len;
                best_start = run_start;
            }
        } else {
            run_len = 0;
        }
    }
    if (best_len < 2)
        best_len = 0;
    out[0] = '\0';
    for (i = 0; i < 8 && n + 6 < size; i++) {
        if (best_len > 0 && i == best_start) {
            n += (size_t)snprintf(out + n, size - n, "::");
            i += best_len - 1;
            continue;
        }
        if (n > 0 && out[n - 1] != ':')
            n += (size_t)snprintf(out + n, size - n, ":");
        n += (size_t)snprintf(out + n, size - n, "%x", group[i]);
    }
}
