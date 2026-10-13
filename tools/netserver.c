/*
 * netserver.c - a small test server for the PicoCalc's network tools, for
 * Windows (Winsock). It listens on a TCP port (7000 unless another is given
 * on the command line), reads one line from each client, answers with a
 * greeting, the line it received and the time here, and closes the connection.
 * It also serves a few web pages, and the speed and latency tests of the PicoCalc
 * (GET /stream, POST /upload, and a line LATENCY followed by lines it echoes).
 * It prints its own addresses at start, which are what to type on the PicoCalc.
 *
 *   gcc -O2 -Wall -o netthing-server-Windows.exe tools/netserver.c -lws2_32
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "netserver_const.h"

static void show_addresses(unsigned port);
static void serve_client(SOCKET client, const struct sockaddr *from, int from_size);

int main(int argc, char **argv)
{
    WSADATA wsa;
    SOCKET listener;
    struct sockaddr_in6 local;
    unsigned port = SERVER_PORT;
    int both = 0;

    if (argc > 1)
        port = (unsigned)strtoul(argv[1], NULL, 10);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        return 2;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    /* One IPv6 socket that also takes IPv4 clients; plain IPv4 if this computer has no IPv6 */
    listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (listener != INVALID_SOCKET) {
        DWORD off = 0;

        both = setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, sizeof off) == 0;
        memset(&local, 0, sizeof local);
        local.sin6_family = AF_INET6;
        local.sin6_addr = in6addr_any;
        local.sin6_port = htons((u_short)port);
        if (!both || bind(listener, (struct sockaddr *)&local, sizeof local) != 0) {
            closesocket(listener);
            listener = INVALID_SOCKET;
        }
    }
    if (listener == INVALID_SOCKET) {
        struct sockaddr_in local4;

        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        memset(&local4, 0, sizeof local4);
        local4.sin_family = AF_INET;
        local4.sin_addr.s_addr = htonl(INADDR_ANY);
        local4.sin_port = htons((u_short)port);
        if (listener == INVALID_SOCKET || bind(listener, (struct sockaddr *)&local4, sizeof local4) != 0) {
            fprintf(stderr, "cannot listen on port %u (error %d)\n", port, WSAGetLastError());
            return 1;
        }
    }
    if (listen(listener, SERVER_BACKLOG) != 0) {
        fprintf(stderr, "cannot listen on port %u (error %d)\n", port, WSAGetLastError());
        return 1;
    }
    printf("%s\n", SERVER_GREETING);
    show_addresses(port);
    printf("Waiting for connections. Close this window or press Ctrl+C to stop.\n\n");
    fflush(stdout);
    for (;;) {
        struct sockaddr_storage from;
        int size = sizeof from;
        SOCKET client = accept(listener, (struct sockaddr *)&from, &size);

        if (client == INVALID_SOCKET)
            continue;
        serve_client(client, (struct sockaddr *)&from, size);
        closesocket(client);
    }
}

/* The IPv4 and IPv6 addresses of this computer, so the PicoCalc user knows what to type. */
static void show_addresses(unsigned port)
{
    char name[256];
    struct addrinfo hints, *list, *item;

    if (gethostname(name, sizeof name) != 0)
        return;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(name, NULL, &hints, &list) != 0)
        return;
    printf("This computer (%s) answers on port %u at:\n", name, port);
    for (item = list; item != NULL; item = item->ai_next) {
        char text[NI_MAXHOST];

        if (getnameinfo(item->ai_addr, (socklen_t)item->ai_addrlen, text, sizeof text, NULL, 0, NI_NUMERICHOST) == 0)
            printf("    %s\n", text);
    }
    freeaddrinfo(list);
}

/* Send the whole buffer. */
static void send_all(SOCKET client, const char *data, int size)
{
    while (size > 0) {
        int n = send(client, data, size, 0);

        if (n <= 0)
            return;
        data += n;
        size -= n;
    }
}

/* An HTTP reply with a counted body (the body is left out for HEAD). */
static void reply(SOCKET client, const char *status, const char *type, const char *extra,
                  const char *body, int length, int head_only)
{
    char head[512];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %s\r\nServer: netthing-server\r\nContent-Type: %s\r\nContent-Length: %d\r\n%sConnection: close\r\n\r\n",
                     status, type, length, extra);

    send_all(client, head, n);
    if (!head_only)
        send_all(client, body, length);
}

/* The body of /big: BIG_BYTES bytes of numbered lines, so a download can be checked. */
static void send_big(SOCKET client, int head_only)
{
    char head[256], line[BIG_LINE_BYTES + 1];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                     BIG_BYTES);
    int i;

    send_all(client, head, n);
    if (head_only)
        return;
    for (i = 0; i < BIG_BYTES / BIG_LINE_BYTES; i++) {
        snprintf(line, sizeof line, "LINE %06d 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn\r\n", i);
        send_all(client, line, BIG_LINE_BYTES);
    }
}

static void send_chunked(SOCKET client, int head_only)
{
    static const char head[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n";
    const char *text = PAGE_CHUNKED;
    int left = (int)strlen(text);
    char piece[32];

    send_all(client, head, (int)sizeof head - 1);
    if (head_only)
        return;
    while (left > 0) {
        int n = left < CHUNK_PIECE ? left : CHUNK_PIECE;
        int m = snprintf(piece, sizeof piece, "%X\r\n", n);

        send_all(client, piece, m);
        send_all(client, text, n);
        send_all(client, "\r\n", 2);
        text += n;
        left -= n;
    }
    send_all(client, "0\r\n\r\n", 5);
}

/* The body of /stream: data until the client closes, or STREAM_BYTES, for a download speed test. */
static void send_stream(SOCKET client, int head_only)
{
    static const char head[] = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nConnection: close\r\n\r\n";
    static char block[STREAM_BLOCK];
    long long left = STREAM_BYTES;
    int i;

    send_all(client, head, (int)sizeof head - 1);
    if (head_only)
        return;
    for (i = 0; i < STREAM_BLOCK; i++)
        block[i] = (char)('A' + i % 26);
    while (left > 0) {
        int n = left < STREAM_BLOCK ? (int)left : STREAM_BLOCK;
        int sent = send(client, block, n, 0);

        if (sent <= 0)
            return;                 /* the client has had enough */
        left -= sent;
    }
}

/* An upload test: count what the client sends until it says it has finished (closes its sending
 * side), then tell it the count and the time from the first byte to the last. */
static void serve_upload(SOCKET client, long long already)
{
    static char block[STREAM_BLOCK];
    long long total = already;
    ULONGLONG first = GetTickCount64(), last = first;
    char body[96], head[192];
    int n, head_n, body_n;

    for (;;) {
        n = recv(client, block, sizeof block, 0);
        if (n <= 0)
            break;
        total += n;
        last = GetTickCount64();
    }
    if (last == first)
        last = first + 1;
    body_n = snprintf(body, sizeof body, "bytes=%lld ms=%llu\r\n", total, (unsigned long long)(last - first));
    head_n = snprintf(head, sizeof head, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", body_n);
    send_all(client, head, head_n);
    send_all(client, body, body_n);
    printf("    upload: %lld bytes in %llu ms\n", total, (unsigned long long)(last - first));
}

/* A latency test: answer READY, then send every byte back at once until the client closes. */
static void serve_latency(SOCKET client)
{
    char block[256];
    int n, one = 1, count = 0;

    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    send_all(client, "READY\r\n", 7);
    while ((n = recv(client, block, sizeof block, 0)) > 0) {
        send_all(client, block, n);
        count++;
    }
    printf("    latency: %d echoes\n", count);
}

/* Answer a web request: the first line of `request` is "GET /path HTTP/1.1". */
static void serve_http(SOCKET client, const char *request)
{
    char path[128];
    int head_only = strncmp(request, "HEAD ", 5) == 0;
    const char *p = request + (head_only ? 5 : 4);
    size_t n = strcspn(p, " \r\n");

    if (n >= sizeof path)
        n = sizeof path - 1;
    memcpy(path, p, n);
    path[n] = '\0';
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
        reply(client, "200 OK", "text/html", "", PAGE_INDEX, (int)strlen(PAGE_INDEX), head_only);
    else if (strcmp(path, "/text") == 0)
        reply(client, "200 OK", "text/plain", "", PAGE_TEXT, (int)strlen(PAGE_TEXT), head_only);
    else if (strcmp(path, "/redirect") == 0)
        reply(client, "302 Found", "text/html", "Location: /redirect2\r\n", "", 0, head_only);
    else if (strcmp(path, "/redirect2") == 0)
        reply(client, "301 Moved Permanently", "text/html", "Location: /\r\n", "", 0, head_only);
    else if (strcmp(path, "/chunked") == 0)
        send_chunked(client, head_only);
    else if (strcmp(path, "/big") == 0)
        send_big(client, head_only);
    else if (strcmp(path, "/stream") == 0)
        send_stream(client, head_only);
    else
        reply(client, "404 Not Found", "text/html", "", PAGE_404, (int)strlen(PAGE_404), head_only);
    printf("    -> %s\n", path);
}

/* Read a line (or what arrives in SERVER_WAIT_MS), answer, and return. */
static void serve_client(SOCKET client, const struct sockaddr *from, int from_size)
{
    char line[SERVER_LINE_MAX + 1], reply[SERVER_REPLY_MAX], text[NI_MAXHOST];
    char stamp[32];
    int got = 0, n, body_start = 0;
    DWORD wait = SERVER_WAIT_MS;
    time_t now = time(NULL);

    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&wait, sizeof wait);
    while (got < SERVER_LINE_MAX) {
        n = recv(client, line + got, SERVER_LINE_MAX - got, 0);
        if (n <= 0)
            break;
        got += n;
        line[got] = '\0';
        if ((strncmp(line, "GET ", 4) == 0 || strncmp(line, "HEAD ", 5) == 0 || strncmp(line, "POST ", 5) == 0) ? strstr(line, "\r\n\r\n") != NULL
                                                                            : memchr(line, '\n', (size_t)got) != NULL)
            break;
    }
    line[got] = '\0';
    if (strncmp(line, "POST ", 5) == 0 && strstr(line, "\r\n\r\n") != NULL)
        body_start = got - (int)(strstr(line, "\r\n\r\n") + 4 - line);       /* body bytes that came with the headers */
    while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r'))
        line[--got] = '\0';
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    if (getnameinfo(from, (socklen_t)from_size, text, sizeof text, NULL, 0, NI_NUMERICHOST) != 0)
        strcpy(text, "?");
    printf("[%s] %s sent: %.60s\n", stamp, text, line);
    fflush(stdout);
    if (strncmp(line, "GET ", 4) == 0 || strncmp(line, "HEAD ", 5) == 0) {
        serve_http(client, line);
        fflush(stdout);
        return;
    }
    if (strncmp(line, "POST /upload", 12) == 0) {
        wait = SERVER_UPLOAD_WAIT_MS;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&wait, sizeof wait);
        serve_upload(client, body_start);
        fflush(stdout);
        return;
    }
    if (strncmp(line, "LATENCY", 7) == 0) {
        wait = SERVER_UPLOAD_WAIT_MS;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&wait, sizeof wait);
        serve_latency(client);
        fflush(stdout);
        return;
    }
    n = snprintf(reply, sizeof reply, "%s\r\nYOU SENT: %s\r\nSERVER TIME: %s\r\n", SERVER_GREETING, line, stamp);
    if (n > 0)
        send(client, reply, n < (int)sizeof reply ? n : (int)sizeof reply - 1, 0);
}
