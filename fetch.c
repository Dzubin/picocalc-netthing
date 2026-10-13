/*
 * fetch.c - the web check: type an address, connect over http or https,
 * follow redirects, and show the status the server answers with, a few of its
 * headers and how long each step took. The page itself is not shown and is not
 * kept. The same request code also downloads a file to the SD card for the
 * certificate tool. The address is split by url.c, the response read by
 * httpresp.c; the connection itself is the stream of net.h.
 *
 * Author: Thomas Dzubin
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "net.h"
#include "storage.h"
#include "url.h"
#include "httpresp.h"
#include "fetch.h"
#include "ui.h"
#include "gfx.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"
#include "ca_builtin.h"

typedef struct {
    int         mode;
    httpresp_t *resp;
    bool        file_open;
    bool        file_failed;
    uint32_t    saved;
    const char *save_path;
    uint32_t    connect_ms;     /* until the connection (and TLS handshake) was open */
    uint32_t    reply_ms;       /* until the status line and headers were in */
    /* the speed test */
    uint32_t    bytes;                  /* body bytes so far */
    uint32_t    start_bytes;            /* ... when the clock started (the first piece came with the headers) */
    uint32_t    body_started_ms;
    uint32_t    elapsed_ms;             /* the measured time, set when the test ends */
    uint32_t    mark_ms, mark_bytes;    /* the start of the interval of the live figure */
    uint32_t    now_kbs, peak_kbs;
    uint16_t    history[SPEED_BARS];    /* the live figure of each interval, KB/s */
    unsigned    history_count;
    bool        body_began;
    bool        hit_limit;              /* it stopped at the time or size limit rather than at the end of the file */
} sink_t;

/* The root certificates, read from the card when a connection is opened. */
static char ca_pem[CA_MAX + 1];
static bool ca_truncated;               /* the file had more certificates than fit */

/* ===================================================================== */
/*  The root certificates                                                 */
/* ===================================================================== */
/* The length of the start of a PEM text that ends with a whole certificate (what follows the last
 * END line is cut off), or 0 if there is none. */
static size_t ca_boundary(const char *pem, size_t length)
{
    static const char end[] = "-----END CERTIFICATE-----";
    size_t i, marker = sizeof end - 1;

    if (length < marker)
        return 0;
    for (i = length - marker + 1; i-- > 0;) {
        if (memcmp(pem + i, end, marker) == 0) {
            i += marker;
            if (i < length && pem[i] == '\r')
                i++;
            if (i < length && pem[i] == '\n')
                i++;
            return i;
        }
    }
    return 0;
}

/* Read CA.PEM into the buffer: the whole file, or on a chip with little RAM as many of its first
 * certificates as fit. Returns the length of the text, 0 if there is no usable file. */
static size_t load_ca(void)
{
    size_t length = 0, got;
    int rc = storage_read(CA_FILE, (uint8_t *)ca_pem, sizeof ca_pem - 1, &length);

    ca_truncated = false;
    if (rc == 3) {
        ca_truncated = true;
        length = 0;
        if (storage_in_open(CA_FILE) != 0)
            return 0;
        while (length < sizeof ca_pem - 1 &&
               (got = storage_in_read(ca_pem + length, sizeof ca_pem - 1 - length)) > 0)
            length += got;
        storage_in_close();
    } else if (rc != 0) {
        return 0;
    }
    return ca_boundary(ca_pem, length);
}

/* ===================================================================== */
/*  The response body                                                     */
/* ===================================================================== */
static bool status_ok(const httpresp_t *r)
{
    return r->status >= 200 && r->status < 300;
}

static bool is_redirect(const httpresp_t *r)
{
    return (r->status == 301 || r->status == 302 || r->status == 303 || r->status == 307 ||
            r->status == 308) && r->location[0] != '\0';
}

static void body_sink(void *context, const uint8_t *data, size_t length)
{
    sink_t *s = context;

    if (s->mode == FETCH_MODE_SPEED) {
        if (status_ok(s->resp))
            s->bytes += (uint32_t)length;
        return;
    }
    if (s->mode != FETCH_MODE_SAVE || !status_ok(s->resp) || s->file_failed)
        return;
    if (!s->file_open) {
        if (storage_out_open(s->save_path) != 0) {
            s->file_failed = true;
            return;
        }
        s->file_open = true;
    }
    if (storage_out_write(data, length) != 0)
        s->file_failed = true;
    else
        s->saved += (uint32_t)length;
}

/* The graph of the speed test: a bar for each half second. */
static void draw_speed_graph(const sink_t *sink)
{
    unsigned scale = gfx_scale_up(gfx_top(sink->history, sink->history_count));
    char line[TOOL_LINE_MAX];

    gfx_rect(COL_BG, SPEED_GRAPH_X - 4, SPEED_GRAPH_Y - 2, SPEED_BARS * SPEED_BAR_STEP + 8, SPEED_GRAPH_H + 6);
    gfx_bars(sink->history, sink->history_count, SPEED_GRAPH_X, SPEED_GRAPH_Y, SPEED_BAR_W, SPEED_BAR_STEP,
             SPEED_GRAPH_H, scale, false, COL_CYAN);
    snprintf(line, sizeof line, "TOP = %u KB/S, ONE BAR = 0.5 S", scale);
    clear_row(SPEED_ROW_LABEL);
    put_at(TOOL_COL, SPEED_ROW_LABEL, line, COL_WHITE);
}

/* ===================================================================== */
/*  One request                                                           */
/* ===================================================================== */
static void status_line(const char *text, uint16_t colour)
{
    clear_row(TOOL_ROW_LAST);
    put_at(TOOL_COL, TOOL_ROW_LAST, text, colour);
}

/* Wait for the connection to open (or fail). Returns FETCH_OK, FETCH_CANCEL or FETCH_ERROR. */
static int wait_open(const url_t *url)
{
    int result;

    status_line(url->tls ? "CONNECTING, SECURE HANDSHAKE..." : "CONNECTING...", COL_YELLOW);
    result = tools_stream_wait_open();
    if (result == 2)
        return FETCH_CANCEL;
    if (result != 0) {
        if (url->tls && ca_truncated)
            tools_out_line("(ONLY THE FIRST ROOTS OF CA.PEM FIT)", COL_WHITE);
        return FETCH_ERROR;
    }
    return FETCH_OK;
}

/* Fetch one address. The response (and its body, through the sink) is left in
 * *resp; FETCH_OK means a complete response arrived (any status), or, for a check,
 * that the headers did. */
static int fetch_one(const url_t *url, int mode, httpresp_t *resp, sink_t *sink)
{
    static char request[512];
    static uint8_t chunk[FETCH_CHUNK];
    char addr[NET_ADDR_MAX], line[TOOL_LINE_MAX], host_header[URL_HOST_MAX + 10];
    uint32_t started, last_data, last_draw = 0, total = 0, begun = plat_now_ms();
    size_t ca_len = 0;
    int how, result;

    if ((how = tools_resolve(url->host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return how == 2 ? FETCH_CANCEL : FETCH_ERROR;
    }
    if (url->tls) {
        if (!tools_ensure_clock())
            return FETCH_ERROR;
        ca_len = load_ca();
        if (ca_len == 0) {
            if (!fetch_offer_builtin_ca())
                return FETCH_ERROR;
            ca_len = load_ca();
            if (ca_len == 0) {
                tools_out_line("COULD NOT READ IT BACK", COL_YELLOW);
                return FETCH_ERROR;
            }
        }
        ca_pem[ca_len++] = '\0';
        tools_register_clock();
    }
    begun = plat_now_ms();
    if (net_stream_open(addr, url->port, url->tls ? url->host : NULL, ca_pem, ca_len) != 0) {
        tools_out_line(net_stream_error()[0] ? net_stream_error() : "COULD NOT OPEN THE CONNECTION", COL_YELLOW);
        return FETCH_ERROR;
    }
    if ((result = wait_open(url)) != FETCH_OK) {
        net_stream_close();
        return result;
    }
    sink->connect_ms = plat_now_ms() - begun;
    url_host_text(url, host_header, sizeof host_header);
    if (url->port != (url->tls ? 443 : 80)) {
        size_t n = strlen(host_header);

        snprintf(host_header + n, sizeof host_header - n, ":%u", (unsigned)url->port);
    }
    snprintf(request, sizeof request,
             "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " FETCH_USER_AGENT "\r\n"
             "Accept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
             url->path, host_header);
    if (net_stream_send(request, strlen(request)) != 0) {
        tools_out_line("COULD NOT SEND THE REQUEST", COL_YELLOW);
        net_stream_close();
        return FETCH_ERROR;
    }
    httpresp_init(resp, false);
    started = last_data = plat_now_ms();
    for (;;) {
        uint32_t now = plat_now_ms();
        size_t n = net_stream_read(chunk, sizeof chunk);

        if (n > 0) {
            last_data = now;
            total += (uint32_t)n;
            if (httpresp_feed(resp, chunk, n, body_sink, sink) < 0) {
                tools_out_line("THE SERVER'S REPLY WAS NOT HTTP", COL_YELLOW);
                net_stream_close();
                return FETCH_ERROR;
            }
        } else {
            int state = net_stream_state();

            if (state == NET_STREAM_CLOSED)
                break;
            if (state == NET_STREAM_FAILED) {
                if (resp->phase != HTTP_PHASE_DONE && !(resp->phase == HTTP_PHASE_BODY && !resp->chunked && resp->content_length < 0)) {
                    tools_out_line(net_stream_error()[0] ? net_stream_error() : "THE CONNECTION BROKE", COL_YELLOW);
                    net_stream_close();
                    return FETCH_ERROR;
                }
                break;
            }
            plat_sleep_ms(POLL_MS);
        }
        if (resp->phase != HTTP_PHASE_HEADERS && sink->reply_ms == 0)
            sink->reply_ms = now - started + 1;
        if (mode == FETCH_MODE_SPEED && resp->phase == HTTP_PHASE_BODY && status_ok(resp)) {
            uint32_t elapsed, measured;

            if (!sink->body_began) {
                sink->body_began = true;
                sink->body_started_ms = sink->mark_ms = now;
                sink->start_bytes = sink->mark_bytes = sink->bytes;
            }
            elapsed = now - sink->body_started_ms;
            measured = sink->bytes - sink->start_bytes;
            if (now - sink->mark_ms >= SPEED_MARK_MS) {
                sink->now_kbs = (uint32_t)((uint64_t)(sink->bytes - sink->mark_bytes) * 1000u / (now - sink->mark_ms) / 1024u);
                if (sink->now_kbs > sink->peak_kbs)
                    sink->peak_kbs = sink->now_kbs;
                if (sink->history_count < SPEED_BARS)
                    sink->history[sink->history_count++] = (uint16_t)(sink->now_kbs > 65000u ? 65000u : sink->now_kbs);
                draw_speed_graph(sink);
                sink->mark_ms = now;
                sink->mark_bytes = sink->bytes;
            }
            if (elapsed >= SPEED_MAX_MS || measured >= SPEED_MAX_BYTES) {
                sink->hit_limit = true;
                sink->elapsed_ms = elapsed;
                break;
            }
        }
        if (resp->phase == HTTP_PHASE_DONE)
            break;
        if (resp->phase == HTTP_PHASE_BODY && (mode == FETCH_MODE_CHECK || is_redirect(resp)))
            break;                                      /* the headers are all a check or a redirect needs */
        if (poll_key() == KEY_ESC) {
            net_stream_close();
            return FETCH_CANCEL;
        }
        if (now - last_data > FETCH_IDLE_TIMEOUT_MS) {
            tools_out_line("THE SERVER WENT QUIET", COL_YELLOW);
            net_stream_close();
            return FETCH_ERROR;
        }
        if (now - last_draw >= FETCH_UPDATE_MS) {
            uint32_t ms = now - started;

            last_draw = now;
            if (resp->phase == HTTP_PHASE_HEADERS)
                snprintf(line, sizeof line, "WAITING FOR THE REPLY...");
            else if (mode == FETCH_MODE_SPEED)
                snprintf(line, sizeof line, "NOW %u KB/S  %u KB", (unsigned)sink->now_kbs,
                         (unsigned)((sink->bytes - sink->start_bytes) / 1024u));
            else
                snprintf(line, sizeof line, "%u BYTES  %u KB/S", (unsigned)resp->body_bytes,
                         ms ? (unsigned)((uint64_t)total * 1000u / ms / 1024u) : 0u);
            status_line(line, COL_WHITE);
        }
    }
    if (mode == FETCH_MODE_SAVE)
        (void)httpresp_end(resp);
    if (mode == FETCH_MODE_SPEED && sink->body_began && sink->elapsed_ms == 0)
        sink->elapsed_ms = plat_now_ms() - sink->body_started_ms;
    if (sink->reply_ms == 0)
        sink->reply_ms = plat_now_ms() - started + 1;
    net_stream_close();
    clear_row(TOOL_ROW_LAST);
    return FETCH_OK;
}

/* ===================================================================== */
/*  The tool                                                              */
/* ===================================================================== */
static bool same_name(const char *text, const char *name, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++)
        if (tolower((unsigned char)text[i]) != tolower((unsigned char)name[i]))
            return false;
    return true;
}

/* The value of a header (the name without its colon) from the header text, or "" if there is none. */
static void header_value(const char *headers, const char *name, char *out, size_t size)
{
    size_t name_len = strlen(name);
    const char *p = headers;

    out[0] = '\0';
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);

        if (len > name_len && p[name_len] == ':' && same_name(p, name, name_len)) {
            const char *v = p + name_len + 1;

            while (v < p + len && *v == ' ')
                v++;
            len = (size_t)(p + len - v);
            while (len > 0 && (v[len - 1] == '\r' || v[len - 1] == ' '))
                len--;
            if (len >= size)
                len = size - 1;
            memcpy(out, v, len);
            out[len] = '\0';
            return;
        }
        if (!end)
            break;
        p = end + 1;
    }
}

/* Fetch an address, following redirects, and show what the server said. */
static void run_check(url_t url)
{
    char line[TOOL_LINE_MAX], shown_url[URL_TEXT_MAX + 1], value[HTTP_TYPE_MAX + 1];
    static httpresp_t resp;
    sink_t sink;
    url_t next;
    unsigned redirects;
    int result;

    tools_screen("WEB PAGE CHECK", "");
    for (redirects = 0;; redirects++) {
        memset(&sink, 0, sizeof sink);
        sink.mode = FETCH_MODE_CHECK;
        sink.resp = &resp;
        result = fetch_one(&url, FETCH_MODE_CHECK, &resp, &sink);
        if (result == FETCH_CANCEL)
            return;
        if (result == FETCH_ERROR) {
            tools_finish();
            return;
        }
        if (is_redirect(&resp) && redirects < MAX_REDIRECTS) {
            if (!url_resolve(&url, resp.location, &next)) {
                tools_out_line("THE REDIRECT ADDRESS IS BAD", COL_YELLOW);
                tools_finish();
                return;
            }
            url_format(&next, shown_url, sizeof shown_url);
            snprintf(line, sizeof line, "%d -> %.70s", resp.status, shown_url);
            tools_out_line(line, COL_CYAN);
            url = next;
            continue;
        }
        break;
    }
    url_format(&url, shown_url, sizeof shown_url);
    tools_out_line(shown_url, COL_WHITE);
    snprintf(line, sizeof line, "%d %s", resp.status, resp.reason);
    tools_out_line(line, status_ok(&resp) ? COL_GREEN : COL_YELLOW);
    if (is_redirect(&resp))
        tools_out_line("TOO MANY REDIRECTS", COL_YELLOW);
    else if (resp.status == 404 || resp.status == 410)
        tools_out_line("THE PAGE DOES NOT EXIST", COL_WHITE);
    else if (status_ok(&resp))
        tools_out_line("THE PAGE EXISTS", COL_WHITE);
    if (url.tls)
        tools_out_line("HTTPS, CERTIFICATE CHECKED", COL_GREEN);
    header_value(resp.headers, "Server", value, sizeof value);
    if (value[0]) {
        snprintf(line, sizeof line, "SERVER  %.*s", TOOL_WRAP - 8, value);
        tools_out_line(line, COL_WHITE);
    }
    header_value(resp.headers, "Content-Type", value, sizeof value);
    if (value[0]) {
        snprintf(line, sizeof line, "TYPE    %.*s", TOOL_WRAP - 8, value);
        tools_out_line(line, COL_WHITE);
    }
    if (resp.content_length >= 0) {
        snprintf(line, sizeof line, "LENGTH  %ld BYTES", resp.content_length);
        tools_out_line(line, COL_WHITE);
    } else if (resp.chunked) {
        tools_out_line("LENGTH  NOT GIVEN (CHUNKED)", COL_WHITE);
    }
    snprintf(line, sizeof line, "CONNECTED IN %u MS", (unsigned)sink.connect_ms);
    tools_out_line(line, COL_CYAN);
    snprintf(line, sizeof line, "REPLY IN %u MS MORE", (unsigned)sink.reply_ms);
    tools_out_line(line, COL_CYAN);
    tools_finish();
}

void tool_get(void)
{
    char text[SETTINGS_URL_MAX + 1];
    settings_t *settings;
    url_t url;

    tools_settings_load();
    settings = tools_settings();
    snprintf(text, sizeof text, "%s", settings->url);
    for (;;) {
        if (!ask_text("WEB PAGE CHECK", "", "ADDRESS (URL)", text, SETTINGS_URL_MAX, 1, false, false))
            return;
        if (url_parse(text, &url))
            break;
        notice(PROMPT_ROW_NOTE, "THAT IS NOT A WEB ADDRESS", COL_YELLOW);
    }
    snprintf(settings->url, sizeof settings->url, "%s", text);
    tools_settings_save();
    run_check(url);
}

/* ===================================================================== */
/*  Speed test                                                            */
/* ===================================================================== */
/* Download an address for a few seconds, throwing the data away, and show how fast it came. */
static void run_speed(url_t url, const char *title)
{
    static httpresp_t resp;
    sink_t sink;
    url_t next;
    char line[TOOL_LINE_MAX], shown_url[URL_TEXT_MAX + 1];
    unsigned redirects, avg_kbs, tenths;
    uint32_t bytes;
    int result;

    url_format(&url, shown_url, sizeof shown_url);
    tools_screen(title, "");
    for (redirects = 0;; redirects++) {
        memset(&sink, 0, sizeof sink);
        sink.mode = FETCH_MODE_SPEED;
        sink.resp = &resp;
        result = fetch_one(&url, FETCH_MODE_SPEED, &resp, &sink);
        if (result == FETCH_CANCEL)
            return;
        if (result == FETCH_ERROR) {
            tools_finish();
            return;
        }
        if (is_redirect(&resp) && redirects < MAX_REDIRECTS) {
            if (!url_resolve(&url, resp.location, &next)) {
                tools_out_line("THE REDIRECT ADDRESS IS BAD", COL_YELLOW);
                tools_finish();
                return;
            }
            url_format(&next, shown_url, sizeof shown_url);
            snprintf(line, sizeof line, "%d -> %.70s", resp.status, shown_url);
            tools_out_line(line, COL_CYAN);
            url = next;
            continue;
        }
        break;
    }
    if (!status_ok(&resp)) {
        snprintf(line, sizeof line, "%d %s", resp.status, resp.reason);
        tools_out_line(line, COL_YELLOW);
        tools_finish();
        return;
    }
    bytes = sink.bytes - sink.start_bytes;
    tools_out_row = SPEED_ROW_RESULT;
    if (!sink.body_began || sink.elapsed_ms < SPEED_MIN_MS || bytes == 0) {
        tools_out_line("TOO LITTLE DATA TO MEASURE.", COL_YELLOW);
        snprintf(line, sizeof line, "%u BYTES IN %u MS", (unsigned)bytes, (unsigned)sink.elapsed_ms);
        tools_out_line(line, COL_WHITE);
        tools_out_line("TRY A BIGGER FILE.", COL_WHITE);
        tools_finish();
        return;
    }
    avg_kbs = (unsigned)((uint64_t)bytes * 1000u / sink.elapsed_ms / 1024u);
    tenths = (unsigned)((uint64_t)bytes * 8u / ((uint64_t)sink.elapsed_ms * 100u));
    snprintf(line, sizeof line, "%u KB IN %u.%u SECONDS", (unsigned)(bytes / 1024u), (unsigned)(sink.elapsed_ms / 1000u),
             (unsigned)(sink.elapsed_ms % 1000u / 100u));
    tools_out_line(line, COL_WHITE);
    tools_out_row++;
    snprintf(line, sizeof line, "AVERAGE %u KB/S", avg_kbs);
    tools_out_line(line, COL_GREEN);
    snprintf(line, sizeof line, "= %u.%u MBIT/S", tenths / 10u, tenths % 10u);
    tools_out_line(line, COL_GREEN);
    snprintf(line, sizeof line, "PEAK    %u KB/S", (unsigned)sink.peak_kbs);
    tools_out_line(line, COL_WHITE);
    tools_out_row++;
    if (!sink.hit_limit) {
        tools_out_line("THE FILE ENDED BEFORE THE LIMIT: A", COL_YELLOW);
        tools_out_line("BIGGER FILE GIVES A BETTER TEST.", COL_YELLOW);
    }
    if (url.tls)
        tools_out_line("OVER HTTPS (SLOWER THAN HTTP HERE)", COL_WHITE);
    tools_finish();
}

void tool_speed(void)
{
    char text[SETTINGS_URL_MAX + 1];
    settings_t *settings;
    url_t url;

    tools_settings_load();
    settings = tools_settings();
    snprintf(text, sizeof text, "%s", settings->speed_url);
    for (;;) {
        if (!ask_text("SPEED TEST", "", "ADDRESS OF A BIG FILE", text, SETTINGS_URL_MAX, 1, false, false))
            return;
        if (url_parse(text, &url))
            break;
        notice(PROMPT_ROW_NOTE, "THAT IS NOT A WEB ADDRESS", COL_YELLOW);
    }
    snprintf(settings->speed_url, sizeof settings->speed_url, "%s", text);
    tools_settings_save();
    run_speed(url, "SPEED TEST");
}

/* The same test against the Windows test server's /stream page (host and port as the server tools keep them). */
void fetch_speed_server(const char *host, unsigned port)
{
    char text[URL_TEXT_MAX + 1];
    url_t url;

    snprintf(text, sizeof text, "http://%s:%u%s", host, port, SPEED_SERVER_PATH);
    if (host[0] != '\0' && strchr(host, ':') != NULL)
        snprintf(text, sizeof text, "http://[%s]:%u%s", host, port, SPEED_SERVER_PATH);
    if (url_parse(text, &url))
        run_speed(url, "DOWNLOAD FROM THE SERVER");
}

/* Fetch an address into a file of the given name (no screens of its own): FETCH_OK if it came
 * whole with a 2xx status; the reason is written on the current tool screen otherwise. */
int fetch_file(url_t *url, const char *path, uint32_t *bytes)
{
    static httpresp_t resp;
    sink_t sink;
    url_t next;
    char line[TOOL_LINE_MAX], shown_url[URL_TEXT_MAX + 1];
    unsigned redirects;
    int result;

    for (redirects = 0;; redirects++) {
        memset(&sink, 0, sizeof sink);
        sink.mode = FETCH_MODE_SAVE;
        sink.resp = &resp;
        sink.save_path = path;
        result = fetch_one(url, FETCH_MODE_SAVE, &resp, &sink);
        if (sink.file_open)
            (void)storage_out_close();
        if (result != FETCH_OK)
            return result;
        if (is_redirect(&resp) && redirects < MAX_REDIRECTS) {
            if (!url_resolve(url, resp.location, &next)) {
                tools_out_line("THE REDIRECT ADDRESS IS BAD", COL_YELLOW);
                return FETCH_ERROR;
            }
            url_format(&next, shown_url, sizeof shown_url);
            snprintf(line, sizeof line, "%d -> %.70s", resp.status, shown_url);
            tools_out_line(line, COL_CYAN);
            *url = next;
            continue;
        }
        break;
    }
    if (!status_ok(&resp)) {
        snprintf(line, sizeof line, "%d %s", resp.status, resp.reason);
        tools_out_line(line, COL_YELLOW);
        return FETCH_ERROR;
    }
    if (sink.file_failed || (!sink.file_open && resp.content_length != 0)) {
        tools_out_line("COULD NOT WRITE THE FILE", COL_YELLOW);
        return FETCH_ERROR;
    }
    *bytes = sink.saved;
    return FETCH_OK;
}

/* There is no CA.PEM on the card: offer to write the root certificates that are built into the
 * program. Returns true if the file was written. Messages go on the current tool screen. */
bool fetch_offer_builtin_ca(void)
{
    size_t done = 0, total;
    uint8_t key;

    tools_out_line("THERE IS NO CERTIFICATE FILE ON THE CARD:", COL_YELLOW);
    tools_out_line(CA_FILE, COL_WHITE);
    tools_out_line("WRITE THE BUILT-IN ONE TO THE CARD?", COL_WHITE);
    tools_out_line("Y = YES   ANY OTHER KEY = NO", COL_CYAN);
    key = wait_any_key();
    if (key != 'y' && key != 'Y') {
        tools_out_line("NO FILE, NO HTTPS.", COL_YELLOW);
        return false;
    }
    total = ca_boundary(ca_builtin_pem, CA_BUILTIN_SIZE < CA_MAX ? CA_BUILTIN_SIZE : CA_MAX);   /* the whole list, or what fits */
    if (storage_out_open(CA_FILE) != 0) {
        tools_out_line("COULD NOT WRITE TO THE CARD", COL_YELLOW);
        return false;
    }
    while (done < total) {
        size_t n = total - done < FETCH_CA_PIECE ? total - done : FETCH_CA_PIECE;

        if (storage_out_write(ca_builtin_pem + done, n) != 0) {
            (void)storage_out_close();
            tools_out_line("COULD NOT WRITE TO THE CARD", COL_YELLOW);
            return false;
        }
        done += n;
    }
    if (storage_out_close() != 0) {
        tools_out_line("COULD NOT WRITE TO THE CARD", COL_YELLOW);
        return false;
    }
    tools_out_line("WRITTEN.", COL_GREEN);
    return true;
}
