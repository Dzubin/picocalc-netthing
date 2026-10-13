/*
 * srvtools.c - the tools that need the Windows test server (netthing-server): the
 * download speed from it, the upload speed to it and a latency test of many
 * small round trips. They are in a menu of their own, because most people who
 * run the program have no such server. The server tells the upload speed in its
 * own reply, as it measured it; the other two the PicoCalc times itself.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "net.h"
#include "ui.h"
#include "fetch.h"
#include "gfx.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

#if CHIP_SERVER_TOOLS

/* Ask for the server's host and port (the ones the server tool used last are offered). */
static bool ask_server(const char *title, char *host, size_t host_size, unsigned *port)
{
    char port_text[SETTINGS_PORT_MAX + 1];
    settings_t *settings = tools_settings();
    long value;

    (void)host_size;
    tools_settings_load();
    snprintf(host, TOOL_HOST_MAX + 1, "%s", settings->server);
    if (!ask_text(title, "", "SERVER HOST OR ADDRESS", host, TOOL_HOST_MAX, 1, false, false))
        return false;
    for (;;) {
        snprintf(port_text, sizeof port_text, "%s", settings->port);
        if (!ask_text(title, host, "PORT", port_text, SETTINGS_PORT_MAX, 1, false, false))
            return false;
        value = strtol(port_text, NULL, 10);
        if (value >= 1 && value <= 65535)
            break;
        snprintf(settings->port, sizeof settings->port, "%s", SETTINGS_DEFAULT_PORT);
        notice(PROMPT_ROW_NOTE, "A PORT IS 1 TO 65535", COL_YELLOW);
    }
    snprintf(settings->server, sizeof settings->server, "%s", host);
    snprintf(settings->port, sizeof settings->port, "%s", port_text);
    tools_settings_save();
    *port = (unsigned)value;
    return true;
}

/* Open a plain connection and wait for it. Returns 0, 1 on failure (the reason is on the screen), 2 for ESC. */
static int connect_plain(const char *addr, unsigned port)
{
    int result;

    tools_out_line("CONNECTING...", COL_YELLOW);
    if (net_stream_open(addr, (uint16_t)port, NULL, NULL, 0) != 0) {
        tools_out_line("COULD NOT START THE CONNECTION", COL_YELLOW);
        return 1;
    }
    result = tools_stream_wait_open();
    if (result == 1)
        tools_out_line("(IS THE TEST SERVER RUNNING?)", COL_WHITE);
    if (result != 0)
        net_stream_close();
    return result;
}

/* ===================================================================== */
/*  Download speed                                                        */
/* ===================================================================== */
void tool_server_download(void)
{
    char host[TOOL_HOST_MAX + 1];
    unsigned port;

    if (!ask_server("DOWNLOAD FROM THE SERVER", host, sizeof host, &port))
        return;
    fetch_speed_server(host, port);
}

/* ===================================================================== */
/*  Upload speed                                                          */
/* ===================================================================== */
/* Find "name=number" in the server's reply; returns false if it is not there. */
static bool reply_number(const char *text, const char *name, unsigned long *value)
{
    const char *p = strstr(text, name);

    if (p == NULL)
        return false;
    *value = strtoul(p + strlen(name), NULL, 10);
    return true;
}

void tool_server_upload(void)
{
    static uint8_t block[UPLOAD_CHUNK];
    static char reply[NET_REPLY_MAX], request[TOOL_HOST_MAX + 160];
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], line[TOOL_LINE_MAX];
    unsigned port, i;
    uint32_t sent = 0, began, last_draw = 0, now;
    size_t reply_len = 0;
    int how;
    bool cancelled = false;

    if (!ask_server("UPLOAD TO THE SERVER", host, sizeof host, &port))
        return;
    tools_screen("UPLOAD TO THE SERVER", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    how = connect_plain(addr, port);
    if (how != 0) {
        if (how == 1)
            tools_finish();
        return;
    }
    for (i = 0; i < sizeof block; i++)
        block[i] = (uint8_t)('A' + i % 26);
    snprintf(request, sizeof request,
             "POST " SPEED_UPLOAD_PATH " HTTP/1.1\r\nHost: %s\r\nContent-Type: application/octet-stream\r\n"
             "Connection: close\r\n\r\n", host);
    if (net_stream_send(request, strlen(request)) != 0) {
        tools_out_line("COULD NOT SEND", COL_YELLOW);
        net_stream_close();
        tools_finish();
        return;
    }
    began = plat_now_ms();
    while (!cancelled) {
        size_t n;

        now = plat_now_ms();
        if (now - began >= UPLOAD_MS || sent >= UPLOAD_MAX_BYTES)
            break;
        if (net_stream_state() != NET_STREAM_OPEN) {
            tools_out_line("THE SERVER CLOSED THE CONNECTION", COL_YELLOW);
            net_stream_close();
            tools_finish();
            return;
        }
        n = net_stream_write_some(block, sizeof block);
        sent += (uint32_t)n;
        if (n == 0)
            plat_sleep_ms(POLL_MS);
        if (poll_key() == KEY_ESC)
            cancelled = true;
        if (now - last_draw >= FETCH_UPDATE_MS) {
            last_draw = now;
            snprintf(line, sizeof line, "SENDING... %u KB", (unsigned)(sent / 1024u));
            clear_row(TOOL_ROW_LAST);
            put_at(TOOL_COL, TOOL_ROW_LAST, line, COL_WHITE);
        }
    }
    clear_row(TOOL_ROW_LAST);
    if (cancelled) {
        net_stream_close();
        return;
    }
    net_stream_finish_sending();
    tools_out_line("WAITING FOR THE SERVER'S COUNT...", COL_YELLOW);
    now = plat_now_ms();
    while (plat_now_ms() - now < UPLOAD_REPLY_MS && reply_len < sizeof reply - 1) {
        uint8_t piece[128];
        size_t got = net_stream_read(piece, sizeof piece);

        if (got > 0) {
            size_t room = sizeof reply - 1 - reply_len;

            if (got > room)
                got = room;
            memcpy(reply + reply_len, piece, got);
            reply_len += got;
            reply[reply_len] = '\0';
        } else if (net_stream_state() != NET_STREAM_OPEN) {
            break;
        } else {
            plat_sleep_ms(POLL_MS);
        }
        if (poll_key() == KEY_ESC)
            break;
    }
    net_stream_close();
    reply[reply_len] = '\0';
    {
        unsigned long bytes = 0, ms = 0;

        tools_out_row = TOOL_ROW_FIRST + 6;
        if (reply_number(reply, "bytes=", &bytes) && reply_number(reply, "ms=", &ms) && ms > 0) {
            unsigned kbs = (unsigned)((uint64_t)bytes * 1000u / ms / 1024u);
            unsigned tenths = (unsigned)((uint64_t)bytes * 8u / (ms * 100u));

            snprintf(line, sizeof line, "THE SERVER GOT %lu KB", bytes / 1024u);
            tools_out_line(line, COL_WHITE);
            snprintf(line, sizeof line, "IN %lu.%lu SECONDS", ms / 1000u, ms % 1000u / 100u);
            tools_out_line(line, COL_WHITE);
            tools_out_row++;
            snprintf(line, sizeof line, "UPLOAD %u KB/S", kbs);
            tools_out_line(line, COL_GREEN);
            snprintf(line, sizeof line, "= %u.%u MBIT/S", tenths / 10u, tenths % 10u);
            tools_out_line(line, COL_GREEN);
        } else {
            tools_out_line("THE SERVER DID NOT SEND ITS COUNT.", COL_YELLOW);
            tools_out_line("(AN OLDER TEST SERVER? NEEDS V0.04A)", COL_WHITE);
            snprintf(line, sizeof line, "%u KB WERE SENT", (unsigned)(sent / 1024u));
            tools_out_line(line, COL_WHITE);
        }
    }
    tools_finish();
}

/* ===================================================================== */
/*  Latency                                                               */
/* ===================================================================== */
/* The histogram of the round trips: how many fell in each band of time. */
static void draw_histogram(const uint16_t *times, unsigned count)
{
    static const unsigned limits[] = LATHIST_LIMITS;          /* the upper edges, in ms, of all but the last band */
    static const char *const names[] = LATHIST_NAMES;
    static const uint16_t colours[] = LATHIST_COLOURS;
    unsigned counts[LATHIST_BANDS] = { 0 }, i, band, top = 0;
    char text[16];

    for (i = 0; i < count; i++) {
        for (band = 0; band < LATHIST_BANDS - 1 && times[i] >= limits[band]; band++)
            ;
        counts[band]++;
    }
    for (band = 0; band < LATHIST_BANDS; band++)
        if (counts[band] > top)
            top = counts[band];
    put_at(TOOL_COL, LATHIST_ROW_TITLE, "ROUND TRIPS BY TIME (MS)", COL_CYAN);
    gfx_rect(COL_WHITE, LATHIST_X - 4, LATHIST_Y + LATHIST_H, LATHIST_BANDS * LATHIST_STEP + 4, 1);
    for (band = 0; band < LATHIST_BANDS; band++) {
        int x = LATHIST_X + (int)band * LATHIST_STEP, col = x / 8;
        unsigned h = top ? counts[band] * LATHIST_H / top : 0;

        if (counts[band] > 0 && h < 3)
            h = 3;
        if (h > 0)
            gfx_rect(colours[band], x, LATHIST_Y + LATHIST_H - (int)h, LATHIST_W, (int)h);
        snprintf(text, sizeof text, "%u", counts[band]);
        put_at(col + 1, (LATHIST_Y + LATHIST_H - (int)h) / 10 - 1, text, COL_WHITE);
        put_at(col + 1, LATHIST_ROW_NAMES, names[band], COL_WHITE);
    }
}

void tool_server_latency(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], line[TOOL_LINE_MAX];
    unsigned port, i, lost = 0, answered = 0, shortest = 0, longest = 0, slow = 0, jitter_total = 0, jitter_n = 0;
    unsigned last_ms = 0;
    uint16_t times[LATENCY_COUNT];
    uint32_t total = 0;
    bool cancelled = false, have_last = false;
    int how;

    if (!ask_server("LATENCY TO THE SERVER", host, sizeof host, &port))
        return;
    tools_screen("LATENCY TO THE SERVER", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    how = connect_plain(addr, port);
    if (how != 0) {
        if (how == 1)
            tools_finish();
        return;
    }
    if (net_stream_send("LATENCY\n", 8) != 0) {
        tools_out_line("COULD NOT SEND", COL_YELLOW);
        net_stream_close();
        tools_finish();
        return;
    }
    /* the server answers READY\r\n */
    {
        uint32_t started = plat_now_ms();
        uint8_t piece[32];
        size_t got = 0;

        while (got == 0 && plat_now_ms() - started < LATENCY_TIMEOUT_MS * 3) {
            got = net_stream_read(piece, sizeof piece);
            if (got == 0)
                plat_sleep_ms(POLL_MS);
            if (poll_key() == KEY_ESC) {
                net_stream_close();
                return;
            }
        }
        if (got == 0) {
            tools_out_line("THE SERVER DID NOT ANSWER.", COL_YELLOW);
            tools_out_line("(AN OLDER TEST SERVER? NEEDS V0.04A)", COL_WHITE);
            net_stream_close();
            tools_finish();
            return;
        }
    }
    for (i = 0; i < LATENCY_COUNT && !cancelled; i++) {
        char out[16];
        uint32_t started = plat_now_ms();
        size_t want, got = 0, n;
        uint8_t piece[32];
        unsigned ms;

        want = (size_t)snprintf(out, sizeof out, "P%u\n", i);
        if (net_stream_send(out, want) != 0)
            break;
        while (got < want && plat_now_ms() - started < LATENCY_TIMEOUT_MS) {
            n = net_stream_read(piece, sizeof piece);
            got += n;
            if (n == 0) {
                if (net_stream_state() != NET_STREAM_OPEN)
                    break;
                plat_sleep_ms(1);
            }
            if (poll_key() == KEY_ESC) {
                cancelled = true;
                break;
            }
        }
        ms = (unsigned)(plat_now_ms() - started);
        if (cancelled)
            break;
        if (got >= want) {
            if (answered == 0 || ms < shortest)
                shortest = ms;
            if (ms > longest)
                longest = ms;
            total += ms;
            times[answered] = (uint16_t)(ms > 65000 ? 65000 : ms);
            answered++;
            if (ms > LATENCY_SLOW_MS)
                slow++;
            if (have_last) {
                jitter_total += ms > last_ms ? ms - last_ms : last_ms - ms;
                jitter_n++;
            }
            last_ms = ms;
            have_last = true;
        } else {
            lost++;
        }
        snprintf(line, sizeof line, "%u OF %u  LOST %u", i + 1, LATENCY_COUNT, lost);
        clear_row(TOOL_ROW_LAST);
        put_at(TOOL_COL, TOOL_ROW_LAST, line, COL_WHITE);
        if (tools_wait_or_escape(LATENCY_GAP_MS)) {
            cancelled = true;
            break;
        }
    }
    net_stream_close();
    clear_row(TOOL_ROW_LAST);
    tools_out_row = TOOL_ROW_FIRST + 4;
    snprintf(line, sizeof line, "SENT %u  ANSWERED %u  LOST %u", i, answered, lost);
    tools_out_line(line, lost ? COL_YELLOW : COL_GREEN);
    if (answered > 0) {
        snprintf(line, sizeof line, "MIN %u  AVG %u  MAX %u MS", shortest, (unsigned)(total / answered), longest);
        tools_out_line(line, COL_WHITE);
        snprintf(line, sizeof line, "JITTER %u MS", jitter_n ? jitter_total / jitter_n : 0u);
        tools_out_line(line, COL_WHITE);
        snprintf(line, sizeof line, "OVER %u MS: %u TIMES", LATENCY_SLOW_MS, slow);
        tools_out_line(line, slow ? COL_YELLOW : COL_WHITE);
        draw_histogram(times, answered);
    }
    if (cancelled)
        tools_out_line("STOPPED", COL_YELLOW);
    tools_finish();
}

/* ===================================================================== */
/*  The menu of the server tools                                          */
/* ===================================================================== */
void tool_server_menu(void)
{
    static const char *const labels[] = SERVER_MENU_LABELS;
    static const char keys[] = SERVER_MENU_KEYS;

    for (;;) {
        char line[TCOLS + 1];
        unsigned i;
        uint8_t key;
        char up;
        const char *at;

        screen_clear();
        put_row(2, "WINDOWS TEST SERVER TOOLS", COL_CYAN);
        put_row(4, "NEED NETTHING-SERVER ON A PC", COL_WHITE);
        for (i = 0; labels[i] != NULL; i++) {
            snprintf(line, sizeof line, "%c  %s", keys[i], labels[i]);
            put_at(MENU_COL, SERVER_MENU_ROW_FIRST + (int)i * 2, line, COL_WHITE);
        }
        put_row(TOOL_ROW_HELP, MENU_HELP, COL_WHITE);
        drain_keys();
        key = wait_any_key();
        if (key == KEY_ESC)
            return;
        up = (key >= 'a' && key <= 'z') ? (char)(key - 'a' + 'A') : (char)key;
        at = up ? strchr(keys, up) : NULL;
        if (at == NULL)
            continue;
        switch (*at) {
        case 'T': tool_server(); break;
        case 'D': tool_server_download(); break;
        case 'U': tool_server_upload(); break;
        case 'L': tool_server_latency(); break;
        default: break;
        }
    }
}

#endif /* CHIP_SERVER_TOOLS */
