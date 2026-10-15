/*
 * netserver_const.h - constants of netserver.c, the test server that the
 * PicoCalc's "talk to a server" tool connects to.
 *
 * Author: Thomas Dzubin
 */
#ifndef NETSERVER_CONST_H
#define NETSERVER_CONST_H

#define SERVER_VERSION      "V1.12A"
#define SERVER_PORT         7000        /* the default; a port number on the command line replaces it */
#define SERVER_BACKLOG      4
#define SERVER_LINE_MAX     512         /* the most of a request line that is kept */
#define SERVER_WAIT_MS      5000        /* how long to wait for the client's line */
#define SERVER_GREETING     "NETTHING TEST SERVER " SERVER_VERSION
#define SERVER_REPLY_MAX    1024
#define SERVER_UPLOAD_WAIT_MS 15000     /* the longest quiet gap in an upload or a latency test */

/* The speed test: /stream sends this much (the PicoCalc stops it sooner) */
#define STREAM_BYTES        (64LL * 1024 * 1024)
#define STREAM_BLOCK        16384

/* The web pages (an HTTP GET or HEAD request is answered with one of these) */
#define BIG_BYTES           1048576     /* the size of /big, in bytes */
#define BIG_LINE_BYTES      64          /* each line of /big, with its line end */
#define CHUNK_PIECE         7           /* bytes in each chunk of /chunked */
#define PAGE_INDEX \
    "<!DOCTYPE html>\r\n<html><head><title>NetThing test server</title>\r\n" \
    "<style>body { color: red; }</style></head><body>\r\n" \
    "<h1>NetThing test server</h1>\r\n" \
    "<p>This page comes from <b>netthing-server-Windows.exe</b>. Fish &amp; chips &lt;3 &#65;BC.</p>\r\n" \
    "<ul>\r\n" \
    "<li><a href=\"/text\">a plain text page</a></li>\r\n" \
    "<li><a href=\"/redirect\">a redirect (two steps back to this page)</a></li>\r\n" \
    "<li><a href=\"/missing\">a page that is not there (404)</a></li>\r\n" \
    "<li><a href=\"/chunked\">a page sent in chunks</a></li>\r\n" \
    "<li><a href=\"/big\">a file of exactly one megabyte</a></li>\r\n" \
    "</ul>\r\n<script>var hidden = 1 < 2;</script>\r\n</body></html>\r\n"
#define PAGE_TEXT \
    "This is a plain text page.\r\nLine two.\r\nLine three, a little longer so that it has to wrap on a narrow screen of forty columns.\r\n"
#define PAGE_CHUNKED \
    "This page was sent in chunks of seven bytes each, so a program that reads it must put the pieces back together.\r\n"
#define PAGE_404 "<html><body><h1>404</h1><p>No such page.</p></body></html>\r\n"

#endif /* NETSERVER_CONST_H */
