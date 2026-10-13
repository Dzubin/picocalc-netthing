/*
 * net_stream_pico.c - the stream part of net.h on lwIP's altcp layer: one
 * connection that is plain TCP (http) or TLS through Mbed TLS (https), with
 * the same calls for both. As elsewhere, the lwIP callbacks run in the Pico
 * SDK's background context and the main loop only polls: received data waits
 * in a chain of pbufs, and lwIP's receive window grows again only as the
 * program reads it, so a fast server cannot flood the PicoCalc.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform_time.h"

#include "net.h"

static struct altcp_pcb *stream;
static struct altcp_tls_config *stream_tls_config;
static struct pbuf *stream_pending;             /* received data not yet read, as a chain */
static volatile int stream_state = NET_STREAM_FAILED;
static const char *stream_error = "";
static bool stream_tls;
static net_clock_fn clock_source;

void net_set_clock(net_clock_fn clock)
{
    clock_source = clock;
}

/* Milliseconds since power-up, which Mbed TLS uses for timeouts. */
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)to_ms_since_boot(get_absolute_time());
}

/* The time for certificate checks; Mbed TLS asks through this. */
static mbedtls_time_t tls_time(mbedtls_time_t *t)
{
    mbedtls_time_t now = clock_source ? (mbedtls_time_t)clock_source() : 0;

    if (t != NULL)
        *t = now;
    return now;
}

static void stream_fail(const char *why)
{
    if (stream_state == NET_STREAM_CONNECTING || stream_state == NET_STREAM_OPEN) {
        stream_error = why;
        stream_state = NET_STREAM_FAILED;
    }
}

static void stream_on_error(void *arg, err_t e)
{
    (void)arg;
    (void)e;
    stream = NULL;                              /* lwIP has freed it already */
    if (stream_state == NET_STREAM_CONNECTING)
        stream_fail(stream_tls ? "TLS HANDSHAKE FAILED (CERTIFICATE?)" : "CONNECTION REFUSED OR LOST");
    else if (stream_state == NET_STREAM_OPEN)
        stream_fail("CONNECTION LOST");
}

static err_t stream_on_connected(void *arg, struct altcp_pcb *pcb, err_t e)
{
    (void)arg;
    (void)pcb;
    if (e != ERR_OK) {
        stream_fail("COULD NOT CONNECT");
        return e;
    }
    stream_state = NET_STREAM_OPEN;
    return ERR_OK;
}

static err_t stream_on_received(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t e)
{
    (void)arg;
    (void)pcb;
    if (p == NULL) {                            /* the server closed its side */
        if (stream_state == NET_STREAM_OPEN)
            stream_state = NET_STREAM_CLOSED;
        else
            stream_fail("CLOSED DURING THE HANDSHAKE");
        return ERR_OK;
    }
    if (e != ERR_OK) {
        pbuf_free(p);
        return e;
    }
    if (stream_pending == NULL)
        stream_pending = p;
    else
        pbuf_cat(stream_pending, p);
    return ERR_OK;
}

void net_stream_close(void)
{
    cyw43_arch_lwip_begin();
    if (stream != NULL) {
        altcp_arg(stream, NULL);
        altcp_err(stream, NULL);
        altcp_recv(stream, NULL);
        if (altcp_close(stream) != ERR_OK)
            altcp_abort(stream);
        stream = NULL;
    }
    if (stream_pending != NULL) {
        pbuf_free(stream_pending);
        stream_pending = NULL;
    }
    if (stream_tls_config != NULL) {
        altcp_tls_free_config(stream_tls_config);
        stream_tls_config = NULL;
    }
    cyw43_arch_lwip_end();
    stream_state = NET_STREAM_FAILED;
    stream_error = "";
}

int net_stream_open(const char *addr, uint16_t port, const char *tls_host,
                    const char *ca_pem, size_t ca_len)
{
    ip_addr_t target;
    err_t e;

    net_stream_close();
    if (!ipaddr_aton(addr, &target))
        return 1;
    stream_tls = tls_host != NULL;
    stream_state = NET_STREAM_CONNECTING;
    stream_error = "";
    cyw43_arch_lwip_begin();
    if (stream_tls) {
        mbedtls_platform_set_time(tls_time);
        stream_tls_config = altcp_tls_create_config_client((const u8_t *)ca_pem, ca_len);
        if (stream_tls_config == NULL) {
            cyw43_arch_lwip_end();
            stream_fail("THE CERTIFICATE FILE COULD NOT BE READ");
            return 2;
        }
        stream = altcp_tls_new(stream_tls_config, IP_GET_TYPE(&target));
        if (stream != NULL)
            mbedtls_ssl_set_hostname((mbedtls_ssl_context *)altcp_tls_context(stream), tls_host);
    } else {
        stream = altcp_tcp_new_ip_type(IP_GET_TYPE(&target));
    }
    if (stream == NULL) {
        cyw43_arch_lwip_end();
        net_stream_close();
        stream_error = "OUT OF MEMORY";
        return 3;
    }
    altcp_err(stream, stream_on_error);
    altcp_recv(stream, stream_on_received);
    e = altcp_connect(stream, &target, port, stream_on_connected);
    cyw43_arch_lwip_end();
    if (e != ERR_OK) {
        net_stream_close();
        stream_error = "COULD NOT START THE CONNECTION";
        return 4;
    }
    return 0;
}

static mbedtls_sha256_context sha;

void net_sha256_start(void)
{
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
}

void net_sha256_update(const void *data, size_t length)
{
    mbedtls_sha256_update(&sha, (const unsigned char *)data, length);
}

void net_sha256_finish(char hex[65])
{
    unsigned char sum[32];
    unsigned i;

    mbedtls_sha256_finish(&sha, sum);
    mbedtls_sha256_free(&sha);
    for (i = 0; i < 32; i++)
        snprintf(hex + 2 * i, 3, "%02x", sum[i]);
}

int net_stream_state(void)
{
    return stream_state;
}

const char *net_stream_error(void)
{
    return stream_error;
}

int net_stream_send(const char *data, size_t length)
{
    err_t e = ERR_CLSD;

    cyw43_arch_lwip_begin();
    if (stream != NULL && stream_state == NET_STREAM_OPEN) {
        e = altcp_write(stream, data, (u16_t)length, TCP_WRITE_FLAG_COPY);
        if (e == ERR_OK)
            e = altcp_output(stream);
    }
    cyw43_arch_lwip_end();
    return e == ERR_OK ? 0 : 1;
}

size_t net_stream_write_some(const void *data, size_t length)
{
    size_t n = 0;

    cyw43_arch_lwip_begin();
    if (stream != NULL && stream_state == NET_STREAM_OPEN) {
        n = altcp_sndbuf(stream);
        if (n > length)
            n = length;
        if (n > 0xFFFF)
            n = 0xFFFF;
        if (n > 0 && altcp_write(stream, data, (u16_t)n, TCP_WRITE_FLAG_COPY) == ERR_OK)
            (void)altcp_output(stream);
        else
            n = 0;
    }
    cyw43_arch_lwip_end();
    return n;
}

void net_stream_finish_sending(void)
{
    cyw43_arch_lwip_begin();
    if (stream != NULL)
        (void)altcp_shutdown(stream, 0, 1);
    cyw43_arch_lwip_end();
}

size_t net_stream_read(uint8_t *buffer, size_t max)
{
    size_t n = 0;

    cyw43_arch_lwip_begin();
    if (stream_pending != NULL) {
        n = stream_pending->tot_len < max ? stream_pending->tot_len : max;
        pbuf_copy_partial(stream_pending, buffer, (u16_t)n, 0);
        stream_pending = pbuf_free_header(stream_pending, (u16_t)n);
        if (stream != NULL)
            altcp_recved(stream, (u16_t)n);
    }
    cyw43_arch_lwip_end();
    return n;
}
