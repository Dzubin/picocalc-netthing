/*
 * pemselect.c - see pemselect.h.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "pemselect.h"
#include "pemselect_const.h"

static const char BEGIN[] = "-----BEGIN CERTIFICATE-----";
static const char END[] = "-----END CERTIFICATE-----";

void pemselect_init(pemselect_t *p, const char *const *names, unsigned count, size_t max_bytes,
                    void (*sink)(void *context, const char *text, size_t length), void *context)
{
    memset(p, 0, sizeof *p);
    p->names = names;
    p->name_count = count > PEMSELECT_NAMES_MAX ? PEMSELECT_NAMES_MAX : count;
    p->max_bytes = max_bytes;
    p->sink = sink;
    p->context = context;
    p->wanted = -1;
    p->state = S_SEEK;
}

static void hold(pemselect_t *p, const char *text, size_t length)
{
    if (p->held_len + length + 1 < sizeof p->held) {
        memcpy(p->held + p->held_len, text, length);
        p->held_len += (unsigned)length;
        p->held[p->held_len++] = '\n';
    }
}

/* One complete line, without its line end. */
static void line_done(pemselect_t *p)
{
    char *line = p->line;
    unsigned n = p->line_len, i;

    while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\r'))
        n--;
    line[n] = '\0';
    switch (p->state) {
    case S_SEEK:
        if (n >= 5 && memcmp(line, "=====", 5) == 0) {
            memcpy(p->name, p->previous, sizeof p->name);
            p->wanted = -1;
            for (i = 0; i < p->name_count; i++)
                if (strcmp(p->names[i], p->name) == 0)
                    p->wanted = (int)i;
            p->state = S_WAIT;
        } else if (n > 0) {
            memcpy(p->previous, line, n + 1);
        }
        break;
    case S_WAIT:
        if (strcmp(line, BEGIN) == 0) {
            if (p->wanted >= 0) {
                p->state = S_CERT;
                p->held_len = 0;
                hold(p, line, n);
            } else {
                p->state = S_SKIP;
            }
        }
        break;
    case S_CERT:
        hold(p, line, n);
        if (strcmp(line, END) == 0) {
            if (p->bytes + p->held_len <= p->max_bytes) {
                p->sink(p->context, p->held, p->held_len);
                p->bytes += p->held_len;
                p->roots++;
                p->found[p->wanted] = true;
            } else {
                p->too_big = true;
            }
            p->state = S_SEEK;
            p->previous[0] = '\0';
        }
        break;
    default:                                    /* S_SKIP */
        if (strcmp(line, END) == 0) {
            p->state = S_SEEK;
            p->previous[0] = '\0';
        }
        break;
    }
}

void pemselect_feed(pemselect_t *p, const uint8_t *data, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++) {
        char c = (char)data[i];

        if (c == '\n') {
            line_done(p);
            p->line_len = 0;
        } else if (p->line_len < PEMSELECT_LINE_MAX) {
            p->line[p->line_len++] = c;
        }
    }
}

void pemselect_finish(pemselect_t *p)
{
    if (p->line_len > 0) {
        line_done(p);
        p->line_len = 0;
    }
}
