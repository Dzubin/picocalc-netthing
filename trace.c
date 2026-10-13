/*
 * trace.c - trace route: echo requests with a hop limit that grows by one, so
 * that each router on the way answers "time exceeded" in turn and shows
 * itself. Three requests per hop, with their times, until the target answers
 * or the limit is reached. Works for IPv4 and IPv6 (the address decides).
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "net.h"
#include "ui.h"
#include "gfx.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

/* One request: NET_DONE with the time and the answering address, or NET_FAILED for no answer.
 * Sets *cancelled when ESC was pressed. */
static int probe_once(const char *addr, unsigned ttl, unsigned *ms, char *hop, size_t hop_size,
                      bool *reached, bool *cancelled)
{
    net_ping_set_ttl(ttl);
    if (tools_ping_once(addr, TRACE_TIMEOUT_MS, ms, cancelled) != NET_DONE)
        return NET_FAILED;
    net_ping_hop(hop, hop_size, reached);
    return NET_DONE;
}

/* What the three requests of each hop gave: the average time (GFX_LOST if none came back) and how many came. */
static uint16_t hop_average[TRACE_MAX_HOPS];
static uint8_t  hop_answers[TRACE_MAX_HOPS];

/* One bar for each hop, as long as its time, so a jump in the delay shows at once. */
static void draw_chart(unsigned hops, bool reached)
{
    unsigned i, top = gfx_scale_up(gfx_top(hop_average, hops));
    char line[TOOL_LINE_MAX];

    screen_clear();
    snprintf(line, sizeof line, "TRACE TIMES  FULL BAR = %u MS", top);
    put_at(1, 1, line, COL_CYAN);
    for (i = 0; i < hops; i++) {
        int row = TRACECHART_ROW_FIRST + (int)i;

        snprintf(line, sizeof line, "%2u", i + 1);
        put_at(0, row, line, COL_WHITE);
        if (hop_average[i] == GFX_LOST) {
            put_at(TRACECHART_X / 8, row, "* NO ANSWER", COL_YELLOW);
            continue;
        }
        gfx_hbar(TRACECHART_X, row * 10 + 2, TRACECHART_H, TRACECHART_W, hop_average[i], top,
                 (reached && i == hops - 1) ? COL_CYAN : gfx_ms_colour(hop_average[i]));
        snprintf(line, sizeof line, "%u", (unsigned)hop_average[i]);
        put_at((TRACECHART_X + TRACECHART_W) / 8 + 1, row, line, COL_WHITE);
        if (hop_answers[i] < TRACE_PROBES) {
            snprintf(line, sizeof line, "%u/%u", (unsigned)hop_answers[i], TRACE_PROBES);
            put_at(TCOLS - 4, row, line, COL_YELLOW);
        }
    }
}

void tool_trace(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], line[TOOL_LINE_MAX];
    unsigned ttl, silent = 0, hops_done = 0;
    bool done = false, cancelled = false;
    int how;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", tools_settings()->ping);
    if (!ask_text("TRACE ROUTE", "", "HOST OR ADDRESS", host, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(tools_settings()->ping, sizeof tools_settings()->ping, "%s", host);
    tools_settings_save();

    tools_screen("TRACE ROUTE", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    snprintf(line, sizeof line, "TO %s, %u HOPS AT MOST", addr, TRACE_MAX_HOPS);
    tools_out_line(line, COL_WHITE);
    drain_keys();
    for (ttl = 1; ttl <= TRACE_MAX_HOPS && !done && !cancelled; ttl++) {
        char times[40], hop[NET_ADDR_MAX] = "", seen[NET_ADDR_MAX] = "";
        unsigned probe, answers = 0, n = 0, sum = 0;

        times[0] = '\0';
        for (probe = 0; probe < TRACE_PROBES && !cancelled; probe++) {
            unsigned ms = 0;
            bool reached = false;

            if (probe_once(addr, ttl, &ms, hop, sizeof hop, &reached, &cancelled) == NET_DONE) {
                n += (unsigned)snprintf(times + n, sizeof times - n, "%u ", ms);
                answers++;
                sum += ms;
                snprintf(seen, sizeof seen, "%s", hop);
                if (reached)
                    done = true;
            } else {
                n += (unsigned)snprintf(times + n, sizeof times - n, "* ");
            }
        }
        if (cancelled)
            break;
        hop_answers[ttl - 1] = (uint8_t)answers;
        hop_average[ttl - 1] = answers ? (uint16_t)(sum / answers > 65000 ? 65000 : sum / answers) : GFX_LOST;
        hops_done = ttl;
        /* "12  1 2 1 MS" then the address, on the same line when it fits */
        if (answers == 0) {
            snprintf(line, sizeof line, "%2u  * * *  NO ANSWER", ttl);
            tools_out_page(line, COL_YELLOW);
            silent++;
        } else {
            silent = 0;
            snprintf(line, sizeof line, "%2u  %s", ttl, times);
            if (strlen(line) + strlen(seen) + 1 <= TOOL_WRAP) {
                strcat(line, seen);
                tools_out_page(line, done ? COL_GREEN : COL_WHITE);
            } else {
                tools_out_page(line, done ? COL_GREEN : COL_WHITE);
                snprintf(line, sizeof line, "    %s", seen);
                tools_out_page(line, done ? COL_GREEN : COL_WHITE);
            }
        }
        if (silent >= TRACE_SILENT_STOP) {
            tools_out_page("NOTHING ANSWERS BEYOND HERE", COL_YELLOW);
            break;
        }
    }
    net_ping_set_ttl(0);
    if (cancelled)
        tools_out_page("STOPPED", COL_YELLOW);
    else if (done)
        tools_out_page("THE TARGET ANSWERED", COL_GREEN);
    if (hops_done > 0) {
        put_row(TOOL_ROW_HELP, "ANY KEY: THE CHART", COL_WHITE);
        (void)wait_any_key();
        draw_chart(hops_done, done);
    }
    tools_finish();
}
