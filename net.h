/*
 * net.h - the network as netthing.c's tools see it: look up a name, ping, talk
 * to a TCP server, ask the time of an NTP server. Every call starts something
 * and returns at once; the caller polls until it is no longer NET_BUSY and
 * decides how long it is willing to wait (and may stop it earlier).
 *
 * The implementation is net_pico.c (lwIP on the Pico 2 W). Addresses are text:
 * IPv4 dotted ("192.168.1.1") or IPv6 ("2001:db8::1"). A name is looked up
 * IPv4 first; IPv6 is used when there is no IPv4 answer.
 *
 * Author: Thomas Dzubin
 */
#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_BUSY    0       /* still working */
#define NET_DONE    1       /* finished, the result is ready */
#define NET_FAILED  2       /* it did not work (the name does not exist, refused...) */

#define NET_ADDR_MAX  46    /* room for an IPv4 or IPv6 address in text */
#define NET_REPLY_MAX 1024  /* the most of a TCP reply that is kept */

/* ---- names -------------------------------------------------------------- */
/* Look a host name up (a dotted address is accepted as it is). */
int net_resolve_start(const char *host);
int net_resolve_poll(char *addr, size_t size);

/* ---- ping --------------------------------------------------------------- */
/* Send one echo request to a dotted address. NET_DONE means the reply came,
 * after *ms milliseconds; the caller calls net_ping_start() again for the
 * next one and gives up on a request that stays NET_BUSY too long. */
int net_ping_start(const char *addr);
int net_ping_poll(unsigned *ms);
/* A hop limit (TTL) for the requests started after this; 0 means the normal one. A router that
 * drops a request whose hop limit ran out answers, so NET_DONE then comes from that router. */
void net_ping_set_ttl(unsigned ttl);
/* After net_ping_poll() gave NET_DONE: the address that answered, and whether it was the target
 * itself (an echo reply, or "unreachable") rather than a router on the way. */
void net_ping_hop(char *addr, size_t size, bool *reached);

/* ---- TCP ---------------------------------------------------------------- */
/* Connect, send `text` and collect what the server sends back. Poll: NET_BUSY
 * while the connection is open (reply[] and *len show what has arrived so
 * far), NET_DONE when the server has closed it, NET_FAILED if it could not be
 * made or broke. net_tcp_stop() closes it whatever its state. */
int  net_tcp_start(const char *addr, uint16_t port, const char *text);
int  net_tcp_poll(char *reply, size_t size, size_t *len);
void net_tcp_stop(void);

/* ---- port probe --------------------------------------------------------- */
/* Try to open a TCP connection to a port and close it again. NET_DONE: the port is open (after
 * *ms milliseconds); NET_FAILED: the connection was refused or the network said no; NET_BUSY
 * until then, so a caller that gives up on its own deadline has found a silent (filtered) port. */
int  net_probe_start(const char *addr, uint16_t port);
int  net_probe_poll(unsigned *ms);
void net_probe_stop(void);

/* ---- one UDP question and its answer -------------------------------------- */
#define NET_UDP_MAX 512     /* the most of a reply that is kept */
/* Send one datagram to an address and port and wait for one back from there (a DNS query, for
 * example). NET_DONE: *length bytes are in reply[], after *ms milliseconds. NET_BUSY until then;
 * the caller gives up on its own deadline and calls net_udp_stop(). */
int  net_udp_start(const char *addr, uint16_t port, const uint8_t *data, size_t length);
int  net_udp_poll(uint8_t *reply, size_t size, size_t *length, unsigned *ms);
void net_udp_stop(void);

/* ---- the local network (IPv4) ---------------------------------------------- */
/* Our own address, mask, the router's address and our MAC address, as numbers in host order
 * (192.168.1.5 is 0xC0A80105). Returns false if there is no address yet. */
bool net_lan_self(uint32_t *ip, uint32_t *mask, uint32_t *gateway, uint8_t mac[6]);

/* Send one ARP request ("who has this address?") for an address. Returns 0, or non-zero if it
 * could not be sent (the radio is busy; try again soon). The answer lands in the ARP table. */
int net_arp_ask(uint32_t ip);

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
} net_arp_entry_t;

/* Copy the devices the ARP table knows now (at most `max`); returns how many. The table is
 * small and forgets the oldest, so read it after every few requests. */
unsigned net_arp_table(net_arp_entry_t *out, unsigned max);

/* ---- time --------------------------------------------------------------- */
/* Ask an NTP server (dotted address) for the time. NET_DONE gives the seconds
 * since 1 January 1970 (UTC). */
int  net_ntp_start(const char *addr);
int  net_ntp_poll(uint32_t *unix_seconds);
void net_ntp_stop(void);

/* ---- streams: plain TCP or TLS (https) ------------------------------------ */
#define NET_STREAM_CONNECTING 0     /* connecting (and, for TLS, shaking hands) */
#define NET_STREAM_OPEN       1     /* connected, data may come and go */
#define NET_STREAM_CLOSED     2     /* the server closed it (data may still be waiting to be read) */
#define NET_STREAM_FAILED     3     /* it broke; net_stream_error() says how */

/* Where the TLS layer finds the time, as seconds since 1970, for checking that
 * a certificate has not expired. The function returns 0 if the clock has not
 * been set. */
typedef uint32_t (*net_clock_fn)(void);
void net_set_clock(net_clock_fn clock);

/* Open a connection to a dotted address and port. For https pass the host name
 * (for the server name indication and the certificate check) and the root
 * certificates as PEM text (ca_len counts the closing NUL); for plain http pass
 * NULL for both. Returns 0, or non-zero if it could not even be started. */
int  net_stream_open(const char *addr, uint16_t port, const char *tls_host,
                     const char *ca_pem, size_t ca_len);
int  net_stream_state(void);
const char *net_stream_error(void);
/* Send a few hundred bytes (it waits for nothing). Returns 0 or non-zero. */
int  net_stream_send(const char *data, size_t length);
/* Send as much of the data as there is room for right now; returns how many bytes were taken (0 if
 * the connection is full or closed). For sending more than a few hundred bytes. */
size_t net_stream_write_some(const void *data, size_t length);
/* Say that nothing more will be sent (the other side sees the end), but go on receiving. */
void net_stream_finish_sending(void);
/* Take up to `max` received bytes; returns how many (0 if none have come). */
size_t net_stream_read(uint8_t *buffer, size_t max);
void net_stream_close(void);

/* ---- SHA-256 ------------------------------------------------------------- */
/* A running SHA-256: start, feed the bytes, finish with the 64 hex digits (and the end). */
void net_sha256_start(void);
void net_sha256_update(const void *data, size_t length);
void net_sha256_finish(char hex[65]);

#endif /* NET_H */
