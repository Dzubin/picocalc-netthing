/*
 * survey.c - channel survey: scan, then draw a bar for each 2.4 GHz channel as
 * tall as the strongest signal on it, with the number of networks under it. The
 * channel of the connected network is yellow. Below, a crowding score for
 * channels 1, 6 and 11 (the three that do not overlap) names the quietest.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "wifi.h"
#include "ui.h"
#include "gfx.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static bool     curves;                             /* the overlap view rather than one bar for a channel */
static unsigned count_on[SURVEY_CHANNELS + 1];      /* networks on each channel, index 1 up */
static int      strongest[SURVEY_CHANNELS + 1];     /* the strongest signal there, dBm */

/* The crowding of a channel: every network within SURVEY_REACH channels counts, the nearer and
 * the stronger the more. Lower is better. */
static unsigned crowding(unsigned channel)
{
    unsigned i, total = 0;
    wifi_ap_t ap;

    for (i = 0; wifi_ap_get(i, &ap); i++) {
        int distance = (int)ap.channel - (int)channel;

        if (distance < 0)
            distance = -distance;
        if (distance <= SURVEY_REACH && ap.rssi > SURVEY_FLOOR)
            total += (unsigned)(SURVEY_REACH + 1 - distance) * (unsigned)(ap.rssi - SURVEY_FLOOR) / 5u;
    }
    return total;
}

/* The channel of the network we are connected to (0 if unknown). */
static unsigned own_channel(void)
{
    wifi_info_t info;
    wifi_ap_t ap;
    unsigned i;

    if (!wifi_info_get(&info))
        return 0;
    for (i = 0; wifi_ap_get(i, &ap); i++)
        if (strcmp(ap.ssid, info.ssid) == 0)
            return ap.channel;
    return 0;
}

/* The colour of the n-th strongest network in the overlap view. */
static uint16_t curve_colour(unsigned n, bool own)
{
    static const uint16_t palette[] = SURVEY_PALETTE;

    return own ? COL_YELLOW : palette[n % (sizeof palette / sizeof palette[0])];
}

/* Every network as a hill over the channels it covers (its signal needs about five channels), the
 * weakest first so that the strong ones are not hidden. Names of the strongest below. */
static void draw_curves(const wifi_info_t *info, bool connected)
{
    unsigned total = wifi_ap_count(), i, shown = 0;
    int half = SURVEY_BAR_STEP * 5 / 2;
    char line[TOOL_LINE_MAX];
    wifi_ap_t ap;

    for (i = total; i-- > 0;) {
        int centre, dx;
        unsigned h;

        if (!wifi_ap_get(i, &ap) || ap.channel < 1 || ap.channel > SURVEY_CHANNELS)
            continue;
        centre = SURVEY_GRAPH_X + (ap.channel - 1) * SURVEY_BAR_STEP + SURVEY_BAR_W / 2;
        h = gfx_signal_height(ap.rssi, SURVEY_GRAPH_H);
        for (dx = -half; dx <= half; dx += 2) {
            unsigned hh = h * (unsigned)(half * half - dx * dx) / (unsigned)(half * half);

            gfx_rect(curve_colour(i, connected && strcmp(ap.ssid, info->ssid) == 0), centre + dx,
                     SURVEY_GRAPH_Y + SURVEY_GRAPH_H - (int)hh - 2, 2, 2);
        }
    }
    for (i = 0; i < total && shown < SURVEY_LEGEND_ROWS; i++) {
        if (!wifi_ap_get(i, &ap))
            break;
        snprintf(line, sizeof line, "%-16.16s %d DBM CH %u", ap.ssid[0] ? ap.ssid : "<HIDDEN>", (int)ap.rssi,
                 (unsigned)ap.channel);
        put_at(1, SURVEY_ROW_LEGEND + (int)shown, line, curve_colour(i, connected && strcmp(ap.ssid, info->ssid) == 0));
        shown++;
    }
}

static void draw_survey(void)
{
    static const unsigned best_of[] = { 1, 6, 11 };
    unsigned c, own = own_channel(), best = 1, best_score = 0xFFFFFFFFu, i;
    char line[TOOL_LINE_MAX];
    wifi_ap_t ap;
    wifi_info_t info;
    bool connected = wifi_info_get(&info);

    screen_clear();
    put_row(1, "CHANNEL SURVEY", COL_CYAN);
    snprintf(line, sizeof line, curves ? "%u NETWORKS.  HILL = ONE NETWORK" : "%u NETWORKS.  BAR = STRONGEST SIGNAL", wifi_ap_count());
    put_at(1, SURVEY_ROW_NOTE, line, COL_WHITE);
    memset(count_on, 0, sizeof count_on);
    for (c = 0; c <= SURVEY_CHANNELS; c++)
        strongest[c] = -127;
    for (i = 0; wifi_ap_get(i, &ap); i++) {
        if (ap.channel >= 1 && ap.channel <= SURVEY_CHANNELS) {
            count_on[ap.channel]++;
            if (ap.rssi > strongest[ap.channel])
                strongest[ap.channel] = ap.rssi;
        }
    }
    lcd_solid_rectangle(COL_WHITE, SURVEY_GRAPH_X - 3, SURVEY_GRAPH_Y + SURVEY_GRAPH_H, SURVEY_CHANNELS * SURVEY_BAR_STEP + 4, 1);
    for (c = 1; c <= SURVEY_CHANNELS; c++) {
        unsigned x = SURVEY_GRAPH_X + (c - 1) * SURVEY_BAR_STEP, col = (x + SURVEY_BAR_W / 2) / 8;

        if (!curves && count_on[c] > 0) {
            unsigned h = gfx_signal_height(strongest[c], SURVEY_GRAPH_H);

            if (h < 3)
                h = 3;
            lcd_solid_rectangle(c == own ? COL_YELLOW : COL_CYAN, x, SURVEY_GRAPH_Y + SURVEY_GRAPH_H - h, SURVEY_BAR_W, h);
        }
        snprintf(line, sizeof line, "%u", c);
        put_at((int)col - (c >= 10 ? 1 : 0), SURVEY_ROW_LABEL, line, c == own ? COL_YELLOW : COL_WHITE);
        snprintf(line, sizeof line, "%c", count_on[c] == 0 ? '.' : (count_on[c] > 9 ? '+' : (char)('0' + count_on[c])));
        put_at((int)col, SURVEY_ROW_COUNT, line, COL_WHITE);
    }
    if (curves)
        draw_curves(&info, connected);
    for (i = 0; i < sizeof best_of / sizeof best_of[0]; i++) {
        unsigned score = crowding(best_of[i]);

        if (score < best_score) {
            best_score = score;
            best = best_of[i];
        }
    }
    snprintf(line, sizeof line, "QUIETEST OF 1, 6, 11: CHANNEL %u", best);
    put_at(1, SURVEY_ROW_RESULT, line, COL_GREEN);
    if (own != 0) {
        snprintf(line, sizeof line, "YOURS IS %u, CROWDING %u (%u BEST)", own, crowding(own), best_score);
        put_at(1, SURVEY_ROW_RESULT + 1, line, own == best || crowding(own) <= best_score ? COL_GREEN : COL_YELLOW);
    } else {
        put_at(1, SURVEY_ROW_RESULT + 1, "YOUR CHANNEL IS NOT IN THE LIST", COL_WHITE);
    }
    put_row(SURVEY_ROW_HELP, SURVEY_HELP, COL_WHITE);
}

/* Scan and wait for the result; false if ESC stopped it. */
static bool run_scan(void)
{
    uint32_t started = plat_now_ms();

    screen_clear();
    put_row(1, "CHANNEL SURVEY", COL_CYAN);
    put_row(15, "SCANNING...", COL_YELLOW);
    if (wifi_scan_start() != 0) {
        put_row(17, "THE SCAN WOULD NOT START", COL_YELLOW);
        pause_ms(NOTICE_MS);
        return false;
    }
    drain_keys();
    while (wifi_scan_busy() && plat_now_ms() - started < SCAN_TIMEOUT_MS) {
        if (poll_key() == KEY_ESC)
            return false;
        plat_sleep_ms(POLL_MS);
    }
    return true;
}

void tool_survey(void)
{
    if (!run_scan())
        return;
    draw_survey();
    drain_keys();
    for (;;) {
        uint8_t key = wait_any_key();

        if (key == KEY_ESC)
            return;
        if (key == 'r' || key == 'R') {
            if (!run_scan())
                return;
            draw_survey();
        } else if (key == 'v' || key == 'V') {
            curves = !curves;
            draw_survey();
        }
    }
}
