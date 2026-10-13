/*
 * url.h - web addresses: split "http://host:port/path?query" into its parts, and
 * work out where a redirect's Location header points. Plain portable C; only
 * http and https, host names or IPv4 addresses or IPv6 addresses in [brackets].
 *
 * Author: Thomas Dzubin
 */
#ifndef URL_H
#define URL_H

#include <stdbool.h>
#include <stdint.h>

#define URL_HOST_MAX   63
#define URL_PATH_MAX   255      /* the path and the query together */
#define URL_TEXT_MAX   (8 + URL_HOST_MAX + 2 + 6 + URL_PATH_MAX + 1)   /* "https://" [host] ":port" path and the end */

typedef struct {
    bool     tls;                       /* https */
    char     host[URL_HOST_MAX + 1];
    uint16_t port;                      /* 80 or 443 unless given */
    char     path[URL_PATH_MAX + 1];    /* always starts with a slash */
} url_t;

/* Read an address. The scheme may be left out (http is assumed). Returns false
 * if it is empty, has another scheme, or a part is too long. A fragment (#...)
 * is dropped. */
bool url_parse(const char *text, url_t *out);

/* Where `location` leads from the page at `base`: an absolute address, one
 * starting with // (same scheme), with / (same host), or a relative path. */
bool url_resolve(const url_t *base, const char *location, url_t *out);

/* Write the address back as text ("https://host/path", with the port only if
 * it is not the scheme's own). */
void url_format(const url_t *url, char *out, unsigned size);

/* The host as it is written in an address or a Host header: an IPv6 address
 * (it has colons) gets square brackets. `size` must be at least URL_HOST_MAX + 3. */
void url_host_text(const url_t *url, char *out, unsigned size);

/* A file name for saving the page: the last part of the path made into 8.3
 * characters (upper case letters, digits and underscores), INDEX.HTM if the
 * path has no file name. `size` must be at least 13. */
void url_file_name(const url_t *url, char *out, unsigned size);

#endif /* URL_H */
