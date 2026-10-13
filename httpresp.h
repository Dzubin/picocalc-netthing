/*
 * httpresp.h - reads an HTTP/1.x response as the bytes arrive: the status line
 * and headers first, then the body, whether it is counted (Content-Length),
 * sent in chunks, or runs until the connection closes. The body bytes are handed
 * to a function as they are decoded. Plain portable C, no network code.
 *
 * Author: Thomas Dzubin
 */
#ifndef HTTPRESP_H
#define HTTPRESP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_HEADERS_MAX   2048     /* the header text that is kept for display */
#define HTTP_LOCATION_MAX  256
#define HTTP_TYPE_MAX      64
#define HTTP_REASON_MAX    48

#define HTTP_PHASE_HEADERS 0
#define HTTP_PHASE_BODY    1
#define HTTP_PHASE_DONE    2
#define HTTP_PHASE_ERROR   3

typedef struct {
    int      phase;
    int      status;                         /* 200, 404... (0 until the status line is read) */
    char     reason[HTTP_REASON_MAX + 1];
    char     location[HTTP_LOCATION_MAX + 1];
    char     content_type[HTTP_TYPE_MAX + 1];
    char     headers[HTTP_HEADERS_MAX + 1];  /* the header lines as received, with LF line ends */
    size_t   headers_len;
    long     content_length;                 /* -1 if not given */
    bool     chunked;
    bool     no_body;                        /* a HEAD request, or a status that has no body */
    uint32_t body_bytes;                     /* body bytes delivered so far */
    /* internal state */
    char     line[HTTP_HEADERS_MAX + 1];
    size_t   line_len;
    int      chunk_state;
    uint32_t chunk_left;
    long     body_left;
} httpresp_t;

/* Start a response. `head_only` is true after a HEAD request (no body follows). */
void httpresp_init(httpresp_t *r, bool head_only);

/* Feed received bytes. Body bytes go to sink(context, data, length). Returns the
 * number of bytes used (less than `length` when the response ended inside the
 * data), or -1 if the response is malformed (the phase is then HTTP_PHASE_ERROR). */
int httpresp_feed(httpresp_t *r, const uint8_t *data, size_t length,
                  void (*sink)(void *context, const uint8_t *data, size_t length), void *context);

/* The connection closed: a body without a length ends here. Returns true if
 * the response is complete (including one that ran to the end of the connection). */
bool httpresp_end(httpresp_t *r);

#endif /* HTTPRESP_H */
