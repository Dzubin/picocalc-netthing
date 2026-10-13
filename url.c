/*
 * url.c - see url.h.
 *
 * Author: Thomas Dzubin
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "url.h"

static bool starts_with(const char *text, const char *prefix)
{
    while (*prefix) {
        if (tolower((unsigned char)*text) != *prefix)
            return false;
        text++;
        prefix++;
    }
    return true;
}

/* Copy the path and query of `text` (up to a fragment) into out, adding a
 * leading slash if there is none. */
static bool copy_path(const char *text, char *out)
{
    size_t n = 0;

    if (*text != '/' ) {
        out[n++] = '/';
    }
    while (*text && *text != '#') {
        if (n >= URL_PATH_MAX)
            return false;
        out[n++] = *text++;
    }
    out[n] = '\0';
    return true;
}

bool url_parse(const char *text, url_t *out)
{
    const char *p = text, *end;
    size_t host_len;
    unsigned long port = 0;

    memset(out, 0, sizeof *out);
    while (*p == ' ')
        p++;
    out->tls = false;
    if (starts_with(p, "https://")) {
        out->tls = true;
        p += 8;
    } else if (starts_with(p, "http://")) {
        p += 7;
    } else if (strstr(p, "://") != NULL) {
        return false;
    }
    end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#' && *end != ' ')
        end++;
    host_len = (size_t)(end - p);
    if (*p == '[') {
        /* an IPv6 address in brackets, with an optional :port after the closing bracket */
        const char *close = memchr(p, ']', host_len);
        const char *d;

        if (close == NULL || close == p + 1)
            return false;
        d = close + 1;
        if (d < p + host_len) {
            if (*d != ':' || d + 1 == p + host_len)
                return false;
            for (d++; d < p + host_len; d++) {
                if (*d < '0' || *d > '9')
                    return false;
                port = port * 10 + (unsigned long)(*d - '0');
                if (port > 65535)
                    return false;
            }
        }
        p++;
        host_len = (size_t)(close - p);
    } else {
        const char *colon = memchr(p, ':', host_len);

        if (colon != NULL) {
            const char *d = colon + 1;

            if (d == p + host_len)
                return false;
            for (; d < p + host_len; d++) {
                if (*d < '0' || *d > '9')
                    return false;
                port = port * 10 + (unsigned long)(*d - '0');
                if (port > 65535)
                    return false;
            }
            host_len = (size_t)(colon - p);
        }
    }
    if (host_len == 0 || host_len > URL_HOST_MAX)
        return false;
    memcpy(out->host, p, host_len);
    out->host[host_len] = '\0';
    out->port = port ? (uint16_t)port : (out->tls ? 443 : 80);
    if (*end == '?') {
        char with_slash[URL_PATH_MAX + 2];

        if (strlen(end) > URL_PATH_MAX - 1)
            return false;
        snprintf(with_slash, sizeof with_slash, "/%s", end);
        return copy_path(with_slash, out->path);
    }
    if (*end == '\0' || *end == '#' || *end == ' ') {
        strcpy(out->path, "/");
        return true;
    }
    return copy_path(end, out->path);
}

bool url_resolve(const url_t *base, const char *location, url_t *out)
{
    char text[URL_TEXT_MAX + 1];

    if (strstr(location, "://") != NULL)
        return url_parse(location, out);
    if (location[0] == '/' && location[1] == '/') {
        snprintf(text, sizeof text, "%s:%s", base->tls ? "https" : "http", location);
        return url_parse(text, out);
    }
    *out = *base;
    if (location[0] == '/')
        return copy_path(location, out->path);
    if (location[0] == '?') {
        char path_only[URL_PATH_MAX + 1];
        char *q;

        snprintf(path_only, sizeof path_only, "%s", base->path);
        q = strchr(path_only, '?');
        if (q)
            *q = '\0';
        if (strlen(path_only) + strlen(location) > URL_PATH_MAX)
            return false;
        snprintf(out->path, sizeof out->path, "%s%s", path_only, location);
        return true;
    }
    {
        /* relative: replace everything after the last slash of the base path */
        char dir[URL_PATH_MAX + 1];
        char *slash, *q;

        snprintf(dir, sizeof dir, "%s", base->path);
        q = strchr(dir, '?');
        if (q)
            *q = '\0';
        slash = strrchr(dir, '/');
        if (slash)
            slash[1] = '\0';
        if (strlen(dir) + strlen(location) > URL_PATH_MAX)
            return false;
        snprintf(text, sizeof text, "%s%s", dir, location);
        return copy_path(text, out->path);
    }
}

void url_host_text(const url_t *url, char *out, unsigned size)
{
    if (strchr(url->host, ':') != NULL)
        snprintf(out, size, "[%s]", url->host);
    else
        snprintf(out, size, "%s", url->host);
}

void url_format(const url_t *url, char *out, unsigned size)
{
    bool own = url->port == (url->tls ? 443 : 80);
    char host[URL_HOST_MAX + 3];

    url_host_text(url, host, sizeof host);
    if (own)
        snprintf(out, size, "%s://%s%s", url->tls ? "https" : "http", host, url->path);
    else
        snprintf(out, size, "%s://%s:%u%s", url->tls ? "https" : "http", host,
                 (unsigned)url->port, url->path);
}

void url_file_name(const url_t *url, char *out, unsigned size)
{
    char last[URL_PATH_MAX + 1];
    const char *slash, *dot;
    char base[9], ext[4];
    unsigned nb = 0, ne = 0;
    const char *p;
    char *q;

    snprintf(last, sizeof last, "%s", url->path);
    q = strchr(last, '?');
    if (q)
        *q = '\0';
    slash = strrchr(last, '/');
    slash = slash ? slash + 1 : last;
    if (*slash == '\0') {
        snprintf(out, size, "INDEX.HTM");
        return;
    }
    dot = strrchr(slash, '.');
    for (p = slash; *p && (dot == NULL || p < dot) && nb < 8; p++)
        if (isalnum((unsigned char)*p) || *p == '_')
            base[nb++] = (char)toupper((unsigned char)*p);
    if (dot != NULL)
        for (p = dot + 1; *p && ne < 3; p++)
            if (isalnum((unsigned char)*p) || *p == '_')
                ext[ne++] = (char)toupper((unsigned char)*p);
    if (nb == 0) {
        base[0] = 'F';
        base[1] = 'I';
        base[2] = 'L';
        base[3] = 'E';
        nb = 4;
    }
    base[nb] = '\0';
    ext[ne] = '\0';
    if (ne > 0)
        snprintf(out, size, "%s.%s", base, ext);
    else
        snprintf(out, size, "%s", base);
}
