/*
 * httpresp.c - see httpresp.h.
 *
 * Author: Thomas Dzubin
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "httpresp.h"
#include "httpresp_const.h"

void httpresp_init(httpresp_t *r, bool head_only)
{
    memset(r, 0, sizeof *r);
    r->phase = HTTP_PHASE_HEADERS;
    r->content_length = -1;
    r->no_body = head_only;
}

/* case-insensitive: does `line` start with `name` followed by a colon? Returns
 * a pointer to the value (spaces skipped) or NULL. */
static const char *header_value(const char *line, const char *name)
{
    size_t n = strlen(name);
    size_t i;

    for (i = 0; i < n; i++)
        if (tolower((unsigned char)line[i]) != tolower((unsigned char)name[i]))
            return NULL;
    if (line[n] != ':')
        return NULL;
    line += n + 1;
    while (*line == ' ' || *line == '\t')
        line++;
    return line;
}

static void copy_value(char *dest, size_t size, const char *value)
{
    size_t n = strlen(value);

    if (n >= size)
        n = size - 1;
    memcpy(dest, value, n);
    dest[n] = '\0';
}

/* One complete header line (without its line end). */
static int header_line(httpresp_t *r, char *line)
{
    const char *v;

    if (r->status == 0) {                           /* the status line: HTTP/1.1 200 OK */
        char *sp = strchr(line, ' ');

        if (strncmp(line, "HTTP/", 5) != 0 || sp == NULL)
            return -1;
        r->status = atoi(sp + 1);
        sp = strchr(sp + 1, ' ');
        if (sp != NULL)
            copy_value(r->reason, sizeof r->reason, sp + 1);
        return r->status >= 100 && r->status <= 599 ? 0 : -1;
    }
    if ((v = header_value(line, "content-length")) != NULL)
        r->content_length = atol(v);
    else if ((v = header_value(line, "transfer-encoding")) != NULL) {
        char text[32];

        copy_value(text, sizeof text, v);
        for (v = text; *v; v++)
            if (strncmp(v, "chunked", 7) == 0 || strncmp(v, "Chunked", 7) == 0)
                r->chunked = true;
    } else if ((v = header_value(line, "location")) != NULL)
        copy_value(r->location, sizeof r->location, v);
    else if ((v = header_value(line, "content-type")) != NULL)
        copy_value(r->content_type, sizeof r->content_type, v);
    return 0;
}

/* The blank line ended the headers: decide how the body is delimited. */
static void headers_done(httpresp_t *r)
{
    if (r->status == 204 || r->status == 304 || (r->status >= 100 && r->status < 200))
        r->no_body = true;
    if (r->no_body) {
        r->phase = HTTP_PHASE_DONE;
        return;
    }
    r->phase = HTTP_PHASE_BODY;
    r->chunk_state = CHUNK_SIZE_LINE;
    r->body_left = r->chunked ? -1 : r->content_length;
    if (!r->chunked && r->content_length == 0)
        r->phase = HTTP_PHASE_DONE;
}

/* Feed the bytes to the header parser: returns bytes used, -1 on error. */
static int feed_headers(httpresp_t *r, const uint8_t *data, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++) {
        char c = (char)data[i];

        if (c == '\r')
            continue;
        if (c != '\n') {
            if (r->line_len >= HTTP_HEADERS_MAX)
                return -1;
            r->line[r->line_len++] = c;
            continue;
        }
        r->line[r->line_len] = '\0';
        if (r->line_len == 0) {
            if (r->status == 0)
                continue;                           /* stray blank line before the status */
            headers_done(r);
            return (int)i + 1;
        }
        if (r->headers_len + r->line_len + 1 < HTTP_HEADERS_MAX) {
            memcpy(r->headers + r->headers_len, r->line, r->line_len);
            r->headers_len += r->line_len;
            r->headers[r->headers_len++] = '\n';
            r->headers[r->headers_len] = '\0';
        }
        if (header_line(r, r->line) != 0) {
            r->phase = HTTP_PHASE_ERROR;
            return -1;
        }
        r->line_len = 0;
    }
    return (int)length;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

int httpresp_feed(httpresp_t *r, const uint8_t *data, size_t length,
                  void (*sink)(void *context, const uint8_t *data, size_t length), void *context)
{
    size_t used = 0;

    while (used < length && r->phase != HTTP_PHASE_DONE && r->phase != HTTP_PHASE_ERROR) {
        const uint8_t *p = data + used;
        size_t left = length - used;

        if (r->phase == HTTP_PHASE_HEADERS) {
            int n = feed_headers(r, p, left);

            if (n < 0)
                return -1;
            used += (size_t)n;
            continue;
        }
        if (!r->chunked) {                          /* counted, or to the end of the connection */
            size_t n = left;

            if (r->body_left >= 0 && (long)n > r->body_left)
                n = (size_t)r->body_left;
            if (n > 0) {
                sink(context, p, n);
                r->body_bytes += (uint32_t)n;
            }
            used += n;
            if (r->body_left >= 0) {
                r->body_left -= (long)n;
                if (r->body_left == 0)
                    r->phase = HTTP_PHASE_DONE;
            }
            continue;
        }
        /* chunked */
        if (r->chunk_state == CHUNK_DATA) {
            size_t n = left < r->chunk_left ? left : r->chunk_left;

            sink(context, p, n);
            r->body_bytes += (uint32_t)n;
            r->chunk_left -= (uint32_t)n;
            used += n;
            if (r->chunk_left == 0) {
                r->chunk_state = CHUNK_DATA_END;
                r->line_len = 0;
            }
            continue;
        }
        {
            char c = (char)*p;                      /* one byte of a size line, a chunk's end, or a trailer */

            used++;
            if (c == '\r')
                continue;
            if (c != '\n') {
                if (r->line_len < HTTP_HEADERS_MAX)
                    r->line[r->line_len++] = c;
                continue;
            }
            r->line[r->line_len] = '\0';
            if (r->chunk_state == CHUNK_DATA_END) {
                r->chunk_state = CHUNK_SIZE_LINE;
            } else if (r->chunk_state == CHUNK_SIZE_LINE) {
                uint32_t size = 0;
                size_t i = 0;

                if (r->line_len == 0) {
                    continue;
                }
                while (hex_value(r->line[i]) >= 0 && i < 8)
                    size = size * 16 + (uint32_t)hex_value(r->line[i++]);
                if (i == 0) {
                    r->phase = HTTP_PHASE_ERROR;
                    return -1;
                }
                if (size == 0)
                    r->chunk_state = CHUNK_TRAILERS;
                else {
                    r->chunk_left = size;
                    r->chunk_state = CHUNK_DATA;
                }
            } else if (r->line_len == 0) {          /* CHUNK_TRAILERS ended */
                r->phase = HTTP_PHASE_DONE;
            }
            r->line_len = 0;
        }
    }
    return r->phase == HTTP_PHASE_ERROR ? -1 : (int)used;
}

bool httpresp_end(httpresp_t *r)
{
    if (r->phase == HTTP_PHASE_DONE)
        return true;
    if (r->phase == HTTP_PHASE_BODY && !r->chunked && r->content_length < 0) {
        r->phase = HTTP_PHASE_DONE;                 /* ended by the connection closing */
        return true;
    }
    return false;
}
