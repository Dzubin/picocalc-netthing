/*
 * pemselect.h - picks named root certificates out of the Mozilla list that the
 * curl project publishes (cacert.pem: for each root its name, a line of equals
 * signs, some facts, and the certificate as PEM text). The list is fed in
 * pieces of any size, so a file of 190 KB needs only a few hundred bytes of
 * memory; the chosen certificates come out, as PEM text, through a function.
 * Plain portable C; the same picking is done on a PC by tools/make_ca_pem.py.
 *
 * Author: Thomas Dzubin
 */
#ifndef PEMSELECT_H
#define PEMSELECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PEMSELECT_NAMES_MAX  96     /* names that can be asked for */
#define PEMSELECT_LINE_MAX   200    /* a longer line is cut */

typedef struct {
    const char *const *names;
    unsigned           name_count;
    bool               found[PEMSELECT_NAMES_MAX];
    size_t             max_bytes;       /* the most to write; a certificate that would not fit is left out */
    void             (*sink)(void *context, const char *text, size_t length);
    void              *context;
    size_t             bytes;           /* written so far */
    unsigned           roots;           /* certificates written so far */
    bool               too_big;         /* a wanted certificate did not fit in max_bytes */
    /* inside */
    char               line[PEMSELECT_LINE_MAX + 1];
    unsigned           line_len;
    char               previous[PEMSELECT_LINE_MAX + 1];
    char               name[PEMSELECT_LINE_MAX + 1];
    int                wanted;          /* index of the wanted root being read, or -1 */
    int                state;
    char               held[2560];      /* the certificate being read, until it is known to fit */
    unsigned           held_len;
} pemselect_t;

void pemselect_init(pemselect_t *p, const char *const *names, unsigned count, size_t max_bytes,
                    void (*sink)(void *context, const char *text, size_t length), void *context);
void pemselect_feed(pemselect_t *p, const uint8_t *data, size_t length);
void pemselect_finish(pemselect_t *p);

#endif /* PEMSELECT_H */
