/*
 * pinglive.c - continuous ping with a graph and the Wi-Fi signal strength: one
 * request a second until ESC, a bar for each answer (tall is slow, white is a
 * request with no answer), running figures, and an optional log of every
 * sample to the SD card as a CSV file. Meant for finding a weak spot or an
 * unreliable link by walking around with the PicoCalc.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "wifi.h"
#include "net.h"
#include "storage.h"
#include "ui.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

/* The samples, oldest first, in milliseconds or LIVE_LOST */
static uint16_t samples[LIVE_SAMPLES];
static unsigned sample_count;

static unsigned sent, lost, shortest, longest, last_ms, jitter_total, jitter_count;
static uint32_t total_ms;
static bool     have_last;

/* The log file */
static bool     log_on;
static char     log_buf[LIVE_LOG_BUF];
static unsigned log_len;

static void log_flush(void)
{
    if (log_len > 0 && storage_out_write(log_buf, log_len) != 0)
        log_on = false;                 /* the card refused: stop logging */
    log_len = 0;
}

static void log_add(const char *line)
{
    size_t n = strlen(line);

    if (log_len + n > sizeof log_buf)
        log_flush();
    if (log_on) {
        memcpy(log_buf + log_len, line, n);
        log_len += (unsigned)n;
    }
}

static void log_stop(void)
{
    if (log_on)
        log_flush();
    (void)storage_out_close();
    log_on = false;
}

static bool log_start(void)
{
    if (storage_out_open(LIVE_LOG_FILE) != 0)
        return false;
    log_on = true;
    log_len = 0;
    log_add(LIVE_LOG_HEADER);
    return log_on;
}

static void clear_figures(void)
{
    sample_count = 0;
    sent = lost = shortest = longest = last_ms = jitter_total = jitter_count = 0;
    total_ms = 0;
    have_last = false;
}

static void push_sample(uint16_t value)
{
    if (sample_count == LIVE_SAMPLES) {
        memmove(samples, samples + 1, (LIVE_SAMPLES - 1) * sizeof samples[0]);
        sample_count--;
    }
    samples[sample_count++] = value;
}

static void add_result(bool answered, unsigned ms)
{
    sent++;
    if (!answered) {
        lost++;
        push_sample(LIVE_LOST);
        return;
    }
    if (sent - lost == 1 || ms < shortest)
        shortest = ms;
    if (ms > longest)
        longest = ms;
    total_ms += ms;
    if (have_last) {
        jitter_total += ms > last_ms ? ms - last_ms : last_ms - ms;
        jitter_count++;
    }
    last_ms = ms;
    have_last = true;
    push_sample((uint16_t)(ms > 65000 ? 65000 : ms));
}

/* The tallest bar for what is on the graph: the smallest listed scale that holds the slowest answer. */
static unsigned graph_scale(void)
{
    static const unsigned scales[] = LIVE_SCALES;
    unsigned i, top = 0;

    for (i = 0; i < sample_count; i++)
        if (samples[i] != LIVE_LOST && samples[i] > top)
            top = samples[i];
    for (i = 0; scales[i] != 0; i++)
        if (top <= scales[i])
            return scales[i];
    return scales[i - 1];
}

static void draw_graph(void)
{
    unsigned scale = graph_scale(), i;
    char line[TOOL_LINE_MAX];

    lcd_solid_rectangle(COL_BG, LIVE_GRAPH_X - 2, LIVE_GRAPH_Y - 2, LIVE_SAMPLES * LIVE_BAR_STEP + 4,
                        LIVE_GRAPH_H + 4);
    lcd_solid_rectangle(COL_WHITE, LIVE_GRAPH_X - 2, LIVE_GRAPH_Y + LIVE_GRAPH_H, LIVE_SAMPLES * LIVE_BAR_STEP + 4, 1);
    for (i = 0; i < sample_count; i++) {
        unsigned x = LIVE_GRAPH_X + (LIVE_SAMPLES - sample_count + i) * LIVE_BAR_STEP, h;
        uint16_t colour;

        if (samples[i] == LIVE_LOST) {
            h = LIVE_GRAPH_H;
            colour = COL_WHITE;
        } else {
            h = (unsigned)samples[i] * LIVE_GRAPH_H / scale;
            if (h < 2)
                h = 2;
            if (h > LIVE_GRAPH_H)
                h = LIVE_GRAPH_H;
            colour = samples[i] <= LIVE_GOOD_MS ? COL_GREEN : (samples[i] <= LIVE_FAIR_MS ? COL_YELLOW : COL_RED);
        }
        lcd_solid_rectangle(colour, x, LIVE_GRAPH_Y + LIVE_GRAPH_H - h, LIVE_BAR_W, h);
    }
    snprintf(line, sizeof line, "BAR TOP = %u MS  WHITE = NO ANSWER", scale);
    clear_row(LIVE_ROW_SCALE);
    put_at(1, LIVE_ROW_SCALE, line, COL_WHITE);
}

static void draw_figures(const wifi_info_t *info, bool have_info)
{
    char line[TOOL_LINE_MAX];
    unsigned answered = sent - lost, bars;

    clear_row(LIVE_ROW_SIGNAL);
    if (have_info) {
        unsigned i;

        bars = bars_for(info->rssi);
        for (i = 0; i < BAR_CELLS; i++)
            line[i] = i < bars ? BAR_FULL : BAR_EMPTY;
        snprintf(line + BAR_CELLS, sizeof line - BAR_CELLS, "  %d DBM", (int)info->rssi);
        put_at(1, LIVE_ROW_SIGNAL, "SIGNAL", COL_CYAN);
        put_at(9, LIVE_ROW_SIGNAL, line, bars_colour(bars));
    } else {
        put_at(1, LIVE_ROW_SIGNAL, "SIGNAL  NOT CONNECTED", COL_YELLOW);
    }
    snprintf(line, sizeof line, "SENT %u  LOST %u (%u%%)", sent, lost, sent ? lost * 100u / sent : 0u);
    clear_row(LIVE_ROW_STATS);
    put_at(1, LIVE_ROW_STATS, line, lost ? COL_YELLOW : COL_WHITE);
    if (answered > 0) {
        snprintf(line, sizeof line, "LAST %u  MIN %u  AVG %u  MAX %u MS", last_ms, shortest,
                 (unsigned)(total_ms / answered), longest);
        clear_row(LIVE_ROW_STATS + 1);
        put_at(1, LIVE_ROW_STATS + 1, line, COL_WHITE);
        snprintf(line, sizeof line, "JITTER %u MS", jitter_count ? jitter_total / jitter_count : 0u);
        clear_row(LIVE_ROW_STATS + 2);
        put_at(1, LIVE_ROW_STATS + 2, line, COL_WHITE);
    }
}

static void draw_log_state(void)
{
    clear_row(LIVE_ROW_LOG);
    if (log_on)
        put_at(1, LIVE_ROW_LOG, "LOGGING TO /netthing/PINGLOG.CSV", COL_GREEN);
    else
        put_at(1, LIVE_ROW_LOG, "NOT LOGGING (L STARTS A LOG FILE)", COL_WHITE);
}

/* A key while the ping runs: ESC stops (true), C clears the figures, L starts or ends the log file. */
static bool live_key(uint8_t key)
{
    if (key == KEY_ESC)
        return true;
    if (key == 'c' || key == 'C') {
        clear_figures();
    } else if (key == 'l' || key == 'L') {
        if (log_on)
            log_stop();
        else if (!log_start())
            notice(LIVE_ROW_LOG, "COULD NOT START THE LOG FILE", COL_YELLOW);
        draw_log_state();
    }
    return false;
}

void tool_ping_live(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], line[TOOL_LINE_MAX];
    wifi_info_t info;
    uint32_t began;
    bool stop = false;
    int how;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", tools_settings()->ping);
    if (host[0] == '\0' && wifi_info_get(&info))
        snprintf(host, sizeof host, "%s", info.gateway);
    if (!ask_text("CONTINUOUS PING", "", "HOST OR ADDRESS", host, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(tools_settings()->ping, sizeof tools_settings()->ping, "%s", host);
    tools_settings_save();

    tools_screen("CONTINUOUS PING", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    screen_clear();
    put_row(1, "CONTINUOUS PING", COL_CYAN);
    snprintf(line, sizeof line, "%.*s", TCOLS - 2, addr);
    put_at(1, LIVE_ROW_HOST, line, COL_WHITE);
    put_row(LIVE_ROW_HELP, LIVE_HELP, COL_WHITE);
    clear_figures();
    log_on = false;
    draw_log_state();
    drain_keys();
    began = plat_now_ms();
    while (!stop) {
        uint32_t started = plat_now_ms();
        unsigned ms = 0;
        int state = NET_BUSY;
        bool have_info;

        if (net_ping_start(addr) == 0) {
            while (state == NET_BUSY && plat_now_ms() - started < PING_TIMEOUT_MS && !stop) {
                state = net_ping_poll(&ms);
                stop = live_key(poll_key());
                plat_sleep_ms(POLL_MS);
            }
        } else {
            state = NET_FAILED;
        }
        if (stop)
            break;
        add_result(state == NET_DONE, ms);
        have_info = wifi_info_get(&info);
        if (log_on) {
            snprintf(line, sizeof line, "%u,%u,%d,%d,%u\r\n", sent, (unsigned)((started - began) / 1000u),
                     state == NET_DONE ? (int)ms : -1, have_info ? (int)info.rssi : 0,
                     (unsigned)tools_clock_seconds());
            log_add(line);
        }
        draw_graph();
        draw_figures(&info, have_info);
        while (!stop && plat_now_ms() - started < PING_INTERVAL_MS) {
            stop = live_key(poll_key());
            plat_sleep_ms(POLL_MS);
        }
    }
    log_stop();
}
