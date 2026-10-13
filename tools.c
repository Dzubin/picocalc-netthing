/*
 * tools.c - see tools.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "wifi.h"
#include "link.h"
#include "net.h"
#include "storage.h"
#include "settings.h"
#include "timeutil.h"
#include "ui.h"
#include "gfx.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

/* ===================================================================== */
/*  The settings file and the clock                                       */
/* ===================================================================== */
static settings_t settings;
static bool       settings_loaded;

static bool       clock_is_set;
static uint32_t   clock_unix;         /* the time at clock_ms, UTC */
static uint32_t   clock_ms;           /* plat_now_ms() when it was set */
static int        clock_offset;       /* minutes east of UTC */

void tools_settings_load(void)
{
    static char file[SETTINGS_FILE_MAX];
    size_t len = 0;

    if (settings_loaded)
        return;
    settings_loaded = true;
    settings_defaults(&settings);
    if (storage_read(SETTINGS_PATH, (uint8_t *)file, sizeof file, &len) == 0)
        settings_parse(&settings, file, len);
}

void tools_settings_save(void)
{
    static char file[SETTINGS_FILE_MAX];
    size_t len = settings_format(&settings, file, sizeof file);

    if (len != 0)
        (void)storage_write(SETTINGS_PATH, (const uint8_t *)file, len);
}

settings_t *tools_settings(void)
{
    return &settings;
}

/* The time of the clock as seconds since 1970 (UTC), or 0 if it has not been set. */
static uint32_t clock_now_unix(void)
{
    return clock_is_set ? clock_unix + (plat_now_ms() - clock_ms) / 1000u : 0;
}

uint32_t tools_clock_seconds(void)
{
    return clock_now_unix();
}

void tools_register_clock(void)
{
    net_set_clock(clock_now_unix);
}

bool tools_clock_text(char *out, size_t size)
{
    timeutil_t t;
    char zone[16];
    uint32_t now;

    if (!clock_is_set)
        return false;
    now = clock_unix + (plat_now_ms() - clock_ms) / 1000u;
    timeutil_civil(now, clock_offset, &t);
    timeutil_format_offset(clock_offset, zone, sizeof zone);
    snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u %s", t.year, t.month, t.day, t.hour,
             t.minute, t.second, zone);
    return true;
}

/* ===================================================================== */
/*  Output lines and waiting                                              */
/* ===================================================================== */
int tools_out_row;

void tools_screen(const char *heading, const char *name)
{
    screen_clear();
    put_row(TOOL_ROW_HEADING, heading, COL_CYAN);
    if (name != NULL && name[0])
        put_row(TOOL_ROW_HEADING + 1, name, COL_WHITE);
    tools_out_row = TOOL_ROW_FIRST;
}

/* One line of output on the next row (nothing is printed past the last row). */
void tools_out_line(const char *text, uint16_t colour)
{
    char line[TOOL_WRAP + 1];

    if (tools_out_row > TOOL_ROW_LAST)
        return;
    snprintf(line, sizeof line, "%.*s", TOOL_WRAP, text);
    clear_row(tools_out_row);
    put_at(TOOL_COL, tools_out_row, line, colour);
    tools_out_row++;
}

/* Like tools_out_line(), but when the screen is full it is cleared and the output goes on at the top. */
void tools_out_page(const char *text, uint16_t colour)
{
    int row;

    if (tools_out_row > TOOL_ROW_LAST) {
        for (row = TOOL_ROW_FIRST; row <= TOOL_ROW_LAST; row++)
            clear_row(row);
        tools_out_row = TOOL_ROW_FIRST;
    }
    tools_out_line(text, colour);
}

void tools_finish(void)
{
    put_row(TOOL_ROW_HELP, TOOL_AGAIN_HELP, COL_WHITE);
    (void)wait_any_key();
}

/* Wait `ms`, but return true at once if ESC was pressed. */
bool tools_wait_or_escape(uint32_t ms)
{
    uint32_t until = plat_now_ms() + ms;

    while ((int32_t)(until - plat_now_ms()) > 0) {
        if (poll_key() == KEY_ESC)
            return true;
        plat_sleep_ms(POLL_MS);
    }
    return false;
}

/* After net_ping_start(): wait for the answer for up to timeout_ms. Returns NET_DONE with the time in
 * *ms, or NET_FAILED if none came; *cancelled is set if ESC was pressed. */
int tools_ping_wait(uint32_t timeout_ms, unsigned *ms, bool *cancelled)
{
    uint32_t started = plat_now_ms();
    int state = NET_BUSY;

    *cancelled = false;
    while (state == NET_BUSY && plat_now_ms() - started < timeout_ms) {
        state = net_ping_poll(ms);
        if (poll_key() == KEY_ESC) {
            *cancelled = true;
            return NET_FAILED;
        }
        plat_sleep_ms(POLL_MS);
    }
    return state == NET_DONE ? NET_DONE : NET_FAILED;
}

/* One echo request and the wait for its answer (see tools_ping_wait). */
int tools_ping_once(const char *addr, uint32_t timeout_ms, unsigned *ms, bool *cancelled)
{
    *cancelled = false;
    if (net_ping_start(addr) != 0)
        return NET_FAILED;
    return tools_ping_wait(timeout_ms, ms, cancelled);
}

/* After net_stream_open(): wait until the connection is open (and, for https, the handshake is
 * done). Returns 0 when it is open, 1 if it failed or timed out (the reason is written on the tool
 * screen; the caller closes the stream), 2 if ESC was pressed. */
int tools_stream_wait_open(void)
{
    uint32_t started = plat_now_ms();

    drain_keys();
    for (;;) {
        int state = net_stream_state();

        if (state == NET_STREAM_OPEN)
            return 0;
        if (state != NET_STREAM_CONNECTING) {
            tools_out_line(net_stream_error()[0] ? net_stream_error() : "COULD NOT CONNECT", COL_YELLOW);
            return 1;
        }
        if (plat_now_ms() - started > FETCH_CONNECT_TIMEOUT_MS) {
            tools_out_line("THE SERVER DID NOT ANSWER", COL_YELLOW);
            return 1;
        }
        if (poll_key() == KEY_ESC)
            return 2;
        plat_sleep_ms(POLL_MS);
    }
}

/* Look a host up. Returns 0 with the dotted address in addr, 1 if the name
 * could not be found, 2 if ESC was pressed. */
int tools_resolve(const char *host, char *addr, size_t size)
{
    uint32_t started = plat_now_ms();

    tools_out_line("LOOKING UP THE NAME...", COL_YELLOW);
    (void)net_resolve_start(host);
    drain_keys();
    for (;;) {
        int state = net_resolve_poll(addr, size);

        if (state == NET_DONE)
            return 0;
        if (state == NET_FAILED || plat_now_ms() - started > RESOLVE_TIMEOUT_MS)
            return 1;
        if (poll_key() == KEY_ESC)
            return 2;
        plat_sleep_ms(POLL_MS);
    }
}

/* Say why a lookup failed. */
void tools_resolve_failed(int how)
{
    tools_out_line(how == 2 ? "STOPPED" : "THE NAME WAS NOT FOUND", COL_YELLOW);
    tools_finish();
}

/* ===================================================================== */
/*  Ping                                                                  */
/* ===================================================================== */
/* The bars of the pings so far, under the text. */
static void draw_ping_bars(const uint16_t *seen, unsigned count)
{
    char line[TOOL_LINE_MAX];
    unsigned scale = gfx_scale_up(gfx_top(seen, count));

    gfx_rect(COL_BG, PINGBARS_X - 4, PINGBARS_Y - 2, PING_COUNT * PINGBARS_STEP + 8, PINGBARS_H + 6);
    gfx_bars(seen, count, PINGBARS_X, PINGBARS_Y, PINGBARS_W, PINGBARS_STEP, PINGBARS_H, scale, true, COL_GREEN);
    snprintf(line, sizeof line, "BAR TOP = %u MS  WHITE = LOST", scale);
    clear_row(PINGBARS_ROW_LABEL);
    put_at(TOOL_COL, PINGBARS_ROW_LABEL, line, COL_WHITE);
}

/* Four echo requests to a host, with the summary, on a screen titled `title`. */
static void ping_host(const char *title, const char *host)
{
    char addr[NET_ADDR_MAX], line[TOOL_LINE_MAX];
    unsigned sent = 0, received = 0, shortest = 0, longest = 0, total = 0, i;
    uint16_t seen[PING_COUNT];
    unsigned seen_count = 0;
    int how;

    tools_screen(title, host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    snprintf(line, sizeof line, "PINGING %s", addr);
    tools_out_line(line, COL_WHITE);
    tools_out_row++;
    drain_keys();
    for (i = 0; i < PING_COUNT; i++) {
        uint32_t started = plat_now_ms();
        unsigned ms = 0;
        int state = NET_BUSY;
        bool stop = false;

        if (net_ping_start(addr) != 0) {
            tools_out_line("COULD NOT SEND", COL_YELLOW);
            sent++;
            seen[seen_count++] = GFX_LOST;
            draw_ping_bars(seen, seen_count);
        } else {
            sent++;
            state = tools_ping_wait(PING_TIMEOUT_MS, &ms, &stop);
            if (stop)
                break;
            if (state == NET_DONE) {
                if (received == 0 || ms < shortest)
                    shortest = ms;
                if (ms > longest)
                    longest = ms;
                total += ms;
                received++;
                if (ms == 0)
                    snprintf(line, sizeof line, "REPLY FROM %s  TIME <1 MS", addr);
                else
                    snprintf(line, sizeof line, "REPLY FROM %s  TIME %u MS", addr, ms);
                tools_out_line(line, COL_GREEN);
                seen[seen_count++] = (uint16_t)(ms > 65000 ? 65000 : ms);
            } else {
                tools_out_line("REQUEST TIMED OUT", COL_YELLOW);
                seen[seen_count++] = GFX_LOST;
            }
            draw_ping_bars(seen, seen_count);
        }
        if (i + 1 < PING_COUNT && tools_wait_or_escape(started + PING_INTERVAL_MS > plat_now_ms()
                                                     ? started + PING_INTERVAL_MS - plat_now_ms() : 0))
            break;
    }
    tools_out_row++;
    snprintf(line, sizeof line, "SENT %u  RECEIVED %u  LOST %u", sent, received, sent - received);
    tools_out_line(line, COL_WHITE);
    if (received > 0) {
        snprintf(line, sizeof line, "MIN %u  MAX %u  AVERAGE %u MS", shortest, longest,
                 total / received);
        tools_out_line(line, COL_WHITE);
    }
    tools_finish();
}

void tool_ping(void)
{
    char host[TOOL_HOST_MAX + 1];
    wifi_info_t info;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", settings.ping);
    if (host[0] == '\0' && link_info_get(&info))
        snprintf(host, sizeof host, "%s", info.gateway);
    if (!ask_text("PING", "", "HOST OR ADDRESS", host, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(settings.ping, sizeof settings.ping, "%s", host);
    tools_settings_save();
    ping_host("PING", host);
}

/* Ping the router (the gateway the access point gave) at once, with no questions. */
void tool_ping_gateway(void)
{
    wifi_info_t info;

    if (!link_info_get(&info) || info.gateway[0] == '\0' || strcmp(info.gateway, "0.0.0.0") == 0) {
        tools_screen("PING THE ROUTER", "");
        tools_out_line("NO GATEWAY ADDRESS YET", COL_YELLOW);
        tools_finish();
        return;
    }
    ping_host("PING THE ROUTER", info.gateway);
}

/* ===================================================================== */
/*  Name lookup                                                           */
/* ===================================================================== */
void tool_lookup(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], line[TOOL_LINE_MAX];
    int how;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", settings.lookup);
    if (!ask_text("NAME LOOKUP", "", "HOST NAME", host, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(settings.lookup, sizeof settings.lookup, "%s", host);
    tools_settings_save();

    tools_screen("NAME LOOKUP", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    tools_out_row++;
    tools_out_line("ADDRESS", COL_CYAN);
    snprintf(line, sizeof line, "%s", addr);
    tools_out_line(line, COL_GREEN);
    tools_finish();
}

#if CHIP_SERVER_TOOLS
/* ===================================================================== */
/*  Talking to a test server                                              */
/* ===================================================================== */
/* Show text in lines of TOOL_WRAP characters: CR dropped, LF starts a line. */
static void out_text(const char *text, uint16_t colour)
{
    char line[TOOL_LINE_MAX];
    unsigned n = 0;

    for (; *text; text++) {
        if (*text == '\r')
            continue;
        if (*text == '\n' || n == TOOL_WRAP) {
            line[n] = '\0';
            tools_out_line(line, colour);
            n = 0;
            if (*text == '\n')
                continue;
        }
        line[n++] = (*text >= 0x20 && *text < 0x7F) ? *text : '.';
    }
    if (n > 0) {
        line[n] = '\0';
        tools_out_line(line, colour);
    }
}

void tool_server(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], port_text[SETTINGS_PORT_MAX + 1];
    char message[TOOL_HOST_MAX + 1], send[TOOL_HOST_MAX + 3], line[TOOL_LINE_MAX];
    static char reply[NET_REPLY_MAX];
    size_t len = 0, last_len = 0;
    uint32_t started, last_data;
    int how, state = NET_BUSY;
    long port;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", settings.server);
    if (!ask_text("TALK TO A SERVER", "", "HOST OR ADDRESS", host, TOOL_HOST_MAX, 1, false, false))
        return;
    for (;;) {
        snprintf(port_text, sizeof port_text, "%s", settings.port);
        if (!ask_text("TALK TO A SERVER", host, "PORT", port_text, SETTINGS_PORT_MAX, 1, false, false))
            return;
        port = strtol(port_text, NULL, 10);
        if (port >= 1 && port <= 65535)
            break;
        snprintf(settings.port, sizeof settings.port, "%s", SETTINGS_DEFAULT_PORT);
        notice(PROMPT_ROW_NOTE, "A PORT IS 1 TO 65535", COL_YELLOW);
    }
    snprintf(message, sizeof message, "%s", SERVER_DEFAULT_MESSAGE);
    if (!ask_text("TALK TO A SERVER", host, "MESSAGE TO SEND", message, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(settings.server, sizeof settings.server, "%s", host);
    snprintf(settings.port, sizeof settings.port, "%s", port_text);
    tools_settings_save();

    snprintf(line, sizeof line, "%s:%ld", host, port);
    tools_screen("TALK TO A SERVER", line);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    snprintf(send, sizeof send, "%s\r\n", message);
    tools_out_line("CONNECTING...", COL_YELLOW);
    if (net_tcp_start(addr, (uint16_t)port, send) != 0) {
        tools_out_line("COULD NOT START", COL_YELLOW);
        tools_finish();
        return;
    }
    started = last_data = plat_now_ms();
    drain_keys();
    for (;;) {
        uint32_t now = plat_now_ms();

        state = net_tcp_poll(reply, sizeof reply, &len);
        if (len != last_len) {
            last_len = len;
            last_data = now;
        }
        if (state != NET_BUSY)
            break;
        if (len > 0 && now - last_data > TCP_QUIET_MS)
            break;                                  /* the reply is complete, the server keeps the line open */
        if (len == 0 && now - started > TCP_TIMEOUT_MS)
            break;
        if (poll_key() == KEY_ESC) {
            net_tcp_stop();
            return;
        }
        plat_sleep_ms(POLL_MS);
    }
    net_tcp_stop();
    if (state == NET_FAILED && len == 0) {
        tools_out_line("COULD NOT CONNECT", COL_YELLOW);
        tools_out_line("IS THE SERVER RUNNING?", COL_WHITE);
    } else if (len == 0) {
        tools_out_line("NO REPLY", COL_YELLOW);
    } else {
        tools_out_row = TOOL_ROW_FIRST;
        tools_out_line("THE SERVER REPLIED", COL_GREEN);
        tools_out_row++;
        out_text(reply, COL_WHITE);
        snprintf(line, sizeof line, "%u BYTES", (unsigned)len);
        if (tools_out_row <= TOOL_ROW_LAST - 1) {
            tools_out_row++;
            tools_out_line(line, COL_CYAN);
        }
    }
    tools_finish();
}
#endif /* CHIP_SERVER_TOOLS */

/* ===================================================================== */
/*  The time                                                              */
/* ===================================================================== */
/* Ask an NTP server (a dotted address) for the time, up to NTP_TRIES times.
 * Returns 0 with the seconds, 1 for no answer, 2 if ESC was pressed. */
static int ntp_ask(const char *addr, uint32_t *seconds)
{
    char line[TOOL_LINE_MAX];
    unsigned attempt;

    for (attempt = 0; attempt < NTP_TRIES; attempt++) {
        uint32_t started = plat_now_ms();
        int state = NET_BUSY;

        snprintf(line, sizeof line, "ASKING THE SERVER (%u OF %u)", attempt + 1, NTP_TRIES);
        tools_out_row = TOOL_ROW_FIRST + 2;
        tools_out_line(line, COL_YELLOW);
        if (net_ntp_start(addr) != 0)
            continue;
        drain_keys();
        while (state == NET_BUSY && plat_now_ms() - started < NTP_TIMEOUT_MS) {
            state = net_ntp_poll(seconds);
            if (poll_key() == KEY_ESC) {
                net_ntp_stop();
                return 2;
            }
            plat_sleep_ms(POLL_MS);
        }
        net_ntp_stop();
        if (state == NET_DONE)
            return 0;
    }
    return 1;
}

static void clock_set(uint32_t seconds, int offset)
{
    clock_unix = seconds;
    clock_ms = plat_now_ms();
    clock_offset = offset;
    clock_is_set = true;
}

bool tools_ensure_clock(void)
{
    char addr[NET_ADDR_MAX];
    uint32_t seconds = 0;
    int offset = 0;

    if (clock_is_set)
        return true;
    tools_settings_load();
    tools_out_line("SETTING THE CLOCK FIRST...", COL_YELLOW);
    if (tools_resolve(settings.ntp, addr, sizeof addr) != 0 || ntp_ask(addr, &seconds) != 0) {
        tools_out_row = TOOL_ROW_FIRST + 2;
        tools_out_line("COULD NOT SET THE CLOCK (USE W)", COL_YELLOW);
        return false;
    }
    (void)timeutil_parse_offset(settings.offset, &offset);
    clock_set(seconds, offset);
    return true;
}

void tool_time(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], offset_text[SETTINGS_OFFSET_MAX + 1];
    char line[TOOL_LINE_MAX];
    uint32_t seconds = 0;
    int how, offset = 0;
    bool got = false;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", settings.ntp);
    if (!ask_text("SET THE CLOCK", "", "TIME SERVER (NTP)", host, TOOL_HOST_MAX, 1, false, false))
        return;
    for (;;) {
        snprintf(offset_text, sizeof offset_text, "%s", settings.offset);
        if (!ask_text("SET THE CLOCK", host, "HOURS FROM UTC, E.G. -6", offset_text,
                      SETTINGS_OFFSET_MAX, 1, false, false))
            return;
        if (timeutil_parse_offset(offset_text, &offset))
            break;
        snprintf(settings.offset, sizeof settings.offset, "%s", SETTINGS_DEFAULT_OFFSET);
        notice(PROMPT_ROW_NOTE, "TRY -6, +5:30, OR 0", COL_YELLOW);
    }
    snprintf(settings.ntp, sizeof settings.ntp, "%s", host);
    snprintf(settings.offset, sizeof settings.offset, "%s", offset_text);
    tools_settings_save();

    tools_screen("SET THE CLOCK", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    how = ntp_ask(addr, &seconds);
    if (how == 2)
        return;
    got = how == 0;
    tools_out_row = TOOL_ROW_FIRST + 2;
    if (!got) {
        tools_out_line("NO ANSWER FROM THE TIME SERVER", COL_YELLOW);
    } else {
        timeutil_t utc, local;
        char zone[16];

        clock_set(seconds, offset);
        timeutil_civil(seconds, 0, &utc);
        timeutil_civil(seconds, offset, &local);
        timeutil_format_offset(offset, zone, sizeof zone);
        clear_row(tools_out_row);
        tools_out_line("THE CLOCK IS SET", COL_GREEN);
        tools_out_row++;
        snprintf(line, sizeof line, "UTC    %04u-%02u-%02u %02u:%02u:%02u", utc.year, utc.month, utc.day,
                 utc.hour, utc.minute, utc.second);
        tools_out_line(line, COL_WHITE);
        snprintf(line, sizeof line, "LOCAL  %04u-%02u-%02u %02u:%02u:%02u", local.year, local.month,
                 local.day, local.hour, local.minute, local.second);
        tools_out_line(line, COL_GREEN);
        snprintf(line, sizeof line, "(%s)", zone);
        tools_out_line(line, COL_WHITE);
    }
    tools_finish();
}
