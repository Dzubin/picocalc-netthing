/*
 * dnsquery.h - a DNS question and its answer as bytes: build the query packet
 * for a name, read the reply (its result code and the addresses in it), and
 * write an address as text. Plain portable C; sending the packet to a chosen
 * server is net.h's job.
 *
 * Author: Thomas Dzubin
 */
#ifndef DNSQUERY_H
#define DNSQUERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DNSQ_TYPE_A       1
#define DNSQ_TYPE_AAAA   28
#define DNSQ_ADDRS_MAX    8         /* answers kept from one reply */
#define DNSQ_PACKET_MAX 512         /* the biggest reply read (a plain UDP reply) */
#define DNSQ_TEXT_MAX    40         /* an address in text and the end */

#define DNSQ_RCODE_OK         0
#define DNSQ_RCODE_FORMAT     1
#define DNSQ_RCODE_SERVFAIL   2
#define DNSQ_RCODE_NXDOMAIN   3
#define DNSQ_RCODE_NOTIMP     4
#define DNSQ_RCODE_REFUSED    5

typedef struct {
    int      rcode;                 /* DNSQ_RCODE_ ... */
    unsigned count;                 /* addresses of the type asked for (up to DNSQ_ADDRS_MAX kept) */
    uint8_t  addr[DNSQ_ADDRS_MAX][16];   /* 4 bytes used for an A record */
} dnsq_answer_t;

/* Build the query for `name` (a host name such as "example.com") of type DNSQ_TYPE_A or
 * DNSQ_TYPE_AAAA with message number `id`. Returns the length of the packet in out[], or 0 if
 * the name is empty, too long or has a label that is empty or longer than 63 characters. */
size_t dnsq_build(const char *name, uint16_t type, uint16_t id, uint8_t *out, size_t max);

/* Read a reply. Returns false if it is not a well-formed answer with this id; otherwise
 * fills *out (a name that does not exist has rcode DNSQ_RCODE_NXDOMAIN and no addresses). */
bool dnsq_parse(const uint8_t *message, size_t length, uint16_t id, uint16_t type, dnsq_answer_t *out);

/* An address as text: dotted for DNSQ_TYPE_A, colon form for DNSQ_TYPE_AAAA (the longest run
 * of zero groups is written as ::). `size` must be at least DNSQ_TEXT_MAX. */
void dnsq_format(uint16_t type, const uint8_t addr[16], char *out, size_t size);

#endif /* DNSQUERY_H */
