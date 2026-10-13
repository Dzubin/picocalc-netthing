/*
 * web_test.c - PC test of url.c and httpresp.c.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/web_test.c url.c httpresp.c -o web_test && ./web_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "url.h"
#include "httpresp.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

/* ---- the body sink ---- */
static char body[8192];
static size_t body_len;

static void sink(void *context, const uint8_t *data, size_t length)
{
    (void)context;
    if (body_len + length < sizeof body) {
        memcpy(body + body_len, data, length);
        body_len += length;
        body[body_len] = '\0';
    }
}

/* Feed a response in pieces of `piece` bytes (all at once if 0); returns the
 * parser's phase at the end. */
static int run(httpresp_t *r, const char *text, size_t piece, bool head_only)
{
    size_t len = strlen(text), pos = 0;

    body_len = 0;
    body[0] = '\0';
    httpresp_init(r, head_only);
    while (pos < len) {
        size_t n = piece && len - pos > piece ? piece : len - pos;

        if (httpresp_feed(r, (const uint8_t *)text + pos, n, sink, NULL) < 0)
            return r->phase;
        pos += n;
    }
    (void)httpresp_end(r);
    return r->phase;
}

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

int main(void)
{
    url_t u, v;
    char text[URL_TEXT_MAX + 1], name[16];
    httpresp_t r;
    size_t piece;

    /* ---- urls ---- */
    check("http with path", url_parse("http://example.com/a/b.html", &u) && !u.tls && u.port == 80 &&
          streq(u.host, "example.com") && streq(u.path, "/a/b.html"));
    check("https default port", url_parse("https://example.com", &u) && u.tls && u.port == 443 && streq(u.path, "/"));
    check("a port", url_parse("http://192.168.1.50:7000/x?y=1", &u) && u.port == 7000 && streq(u.path, "/x?y=1"));
    check("no scheme means http", url_parse("example.com/z", &u) && !u.tls && streq(u.host, "example.com"));
    check("query straight after the host", url_parse("http://h.com?q=1", &u) && streq(u.path, "/?q=1"));
    check("fragment dropped", url_parse("http://h.com/p#frag", &u) && streq(u.path, "/p"));
    check("ftp refused", !url_parse("ftp://h.com/", &u));
    check("empty refused", !url_parse("", &u));
    check("bad port refused", !url_parse("http://h.com:99999/", &u) && !url_parse("http://h.com:/", &u));
    check("ipv6 address", url_parse("http://[2001:db8::1]/a", &u) && streq(u.host, "2001:db8::1") && u.port == 80 &&
          streq(u.path, "/a"));
    check("ipv6 address and port", url_parse("https://[::1]:8443/x?y=1", &u) && u.tls && streq(u.host, "::1") &&
          u.port == 8443 && streq(u.path, "/x?y=1"));
    check("ipv6 bad forms refused", !url_parse("http://[::1/", &u) && !url_parse("http://[]/", &u) &&
          !url_parse("http://[::1]x/", &u) && !url_parse("http://[::1]:/", &u) && !url_parse("http://[::1]:70000/", &u));
    url_parse("http://[fe80::2]:7000/p", &u);
    url_format(&u, text, sizeof text);
    check("ipv6 format", streq(text, "http://[fe80::2]:7000/p"));
    url_host_text(&u, text, sizeof text);
    check("ipv6 host text", streq(text, "[fe80::2]"));
    url_parse("https://h.com/dir/page.html?x=1", &v);
    check("scheme-relative ipv6", url_resolve(&v, "//[::1]/s", &u) && streq(u.host, "::1") && u.tls);
    check("absolute redirect", url_resolve(&v, "http://other.org/q", &u) && !u.tls && streq(u.host, "other.org") && streq(u.path, "/q"));
    check("scheme-relative", url_resolve(&v, "//cdn.h.com/s", &u) && u.tls && streq(u.host, "cdn.h.com"));
    check("root-relative", url_resolve(&v, "/top", &u) && streq(u.host, "h.com") && streq(u.path, "/top") && u.tls);
    check("relative", url_resolve(&v, "next.html", &u) && streq(u.path, "/dir/next.html"));
    check("query only", url_resolve(&v, "?y=2", &u) && streq(u.path, "/dir/page.html?y=2"));
    url_parse("http://h.com:8080/a", &u);
    url_format(&u, text, sizeof text);
    check("format with a port", streq(text, "http://h.com:8080/a"));
    url_parse("https://h.com/a", &u);
    url_format(&u, text, sizeof text);
    check("format without the own port", streq(text, "https://h.com/a"));
    url_parse("http://h.com/files/readme.html?x=1", &u);
    url_file_name(&u, name, sizeof name);
    check("file name", streq(name, "README.HTM"));
    url_parse("http://h.com/", &u);
    url_file_name(&u, name, sizeof name);
    check("no file name: INDEX.HTM", streq(name, "INDEX.HTM"));
    url_parse("http://h.com/a-very-long-file-name.tar.gz", &u);
    url_file_name(&u, name, sizeof name);
    check("long names cut to 8.3", streq(name, "TARGZ.GZ") || streq(name, "AVERYLON.GZ") || strlen(name) <= 12);

    /* ---- responses ---- */
    for (piece = 0; piece <= 7; piece += 7) {
        char label[64];

        snprintf(label, sizeof label, "counted body (pieces of %u)", (unsigned)piece);
        check(label, run(&r, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 5\r\n\r\nHello", piece, false) == HTTP_PHASE_DONE &&
              r.status == 200 && streq(r.reason, "OK") && streq(body, "Hello") && streq(r.content_type, "text/html"));
        snprintf(label, sizeof label, "chunked body (pieces of %u)", (unsigned)piece);
        check(label, run(&r, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n7\r\n, world\r\n0\r\n\r\n", piece, false) == HTTP_PHASE_DONE &&
              streq(body, "Hello, world") && r.chunked);
        snprintf(label, sizeof label, "chunk with an extension and trailer (pieces of %u)", (unsigned)piece);
        check(label, run(&r, "HTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked\r\n\r\nA;x=1\r\n0123456789\r\n0\r\nX-T: 1\r\n\r\n", piece, false) == HTTP_PHASE_DONE &&
              streq(body, "0123456789"));
        snprintf(label, sizeof label, "body to the end of the connection (pieces of %u)", (unsigned)piece);
        check(label, run(&r, "HTTP/1.0 200 OK\r\n\r\nuntil close", piece, false) == HTTP_PHASE_DONE && streq(body, "until close"));
    }
    check("redirect headers", run(&r, "HTTP/1.1 302 Found\r\nLocation: https://x.org/n\r\nContent-Length: 0\r\n\r\n", 0, false) == HTTP_PHASE_DONE &&
          r.status == 302 && streq(r.location, "https://x.org/n") && body_len == 0);
    check("404", run(&r, "HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\n\r\nno!", 0, false) == HTTP_PHASE_DONE && r.status == 404 && streq(body, "no!"));
    check("head only has no body", run(&r, "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n", 0, true) == HTTP_PHASE_DONE && body_len == 0);
    check("204 has no body", run(&r, "HTTP/1.1 204 No Content\r\n\r\n", 0, false) == HTTP_PHASE_DONE);
    check("a short body is not complete", run(&r, "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc", 0, false) == HTTP_PHASE_BODY);
    check("garbage is an error", run(&r, "this is not http\r\n\r\n", 0, false) == HTTP_PHASE_ERROR);
    check("headers kept", run(&r, "HTTP/1.1 200 OK\r\nServer: test\r\nContent-Length: 0\r\n\r\n", 0, false) == HTTP_PHASE_DONE &&
          strstr(r.headers, "Server: test") != NULL);
    {
        const char *two = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nokEXTRA";
        int used;

        httpresp_init(&r, false);
        used = httpresp_feed(&r, (const uint8_t *)two, strlen(two), sink, NULL);
        check("bytes after the body are left unused", used == (int)(strlen(two) - 5) && r.phase == HTTP_PHASE_DONE);
    }

    printf("\n%d check(s) failed\n", failures);
    return failures;
}
