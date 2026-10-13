/*
 * btcrowd.c - the 2.4 GHz crowding report: Wi-Fi and Bluetooth share the same
 * band (and this chip's antenna), so a place with many Wi-Fi networks on the
 * same channels and many Bluetooth devices talking is a crowded one, which is a
 * common cause of a slow link or choppy audio. The report scans both for a few
 * seconds, draws the Wi-Fi networks per channel and the Bluetooth devices
 * heard, and says whether the band is quiet, busy or crowded.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "wifi.h"
#include "gfx.h"
#include "ui.h"
#include "btui.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static unsigned wifi_on[BTX_CHANNELS + 1];      /* Wi-Fi networks on each channel, from 1 */

void tool_bt_crowding(void)
{
    uint32_t started = plat_now_ms(), shown_s = 99;
    unsigned i, total_wifi = 0, busiest = 0, busiest_channel = 1, c;
    unsigned ble_total = 0, ble_strong = 0, ble_adverts = 0, score;
    const char *level;
    uint16_t level_colour;
    char line[TOOL_LINE_MAX];
    wifi_ap_t ap;

    screen_clear();
    put_row(2, "2.4 GHZ CROWDING", COL_CYAN);
    put_row(4, "LISTENING TO WI-FI AND BLUETOOTH", COL_WHITE);
    bt_devices_clear();
    (void)wifi_scan_start();
    (void)bt_le_scan_start();
    drain_keys();
    for (;;) {
        uint32_t elapsed = plat_now_ms() - started;
        unsigned left = elapsed >= BTX_SECONDS * 1000u ? 0 : BTX_SECONDS - elapsed / 1000u;

        if (poll_key() == KEY_ESC) {
            bt_le_scan_stop();
            return;
        }
        if (left != shown_s) {
            shown_s = left;
            snprintf(line, sizeof line, "%u SECONDS LEFT", left);
            clear_row(6);
            put_row(6, line, COL_YELLOW);
        }
        if (left == 0 && !wifi_scan_busy())
            break;
        plat_sleep_ms(POLL_MS);
    }
    bt_le_scan_stop();

    /* what was heard */
    memset(wifi_on, 0, sizeof wifi_on);
    for (i = 0; wifi_ap_get(i, &ap); i++) {
        if (ap.channel >= 1 && ap.channel <= BTX_CHANNELS) {
            wifi_on[ap.channel]++;
            total_wifi++;
        }
    }
    for (c = 1; c <= BTX_CHANNELS; c++) {
        unsigned near_count = 0, k;

        for (k = (c > BTX_REACH ? c - BTX_REACH : 1); k <= c + BTX_REACH && k <= BTX_CHANNELS; k++)
            near_count += wifi_on[k];
        if (near_count > busiest) {
            busiest = near_count;
            busiest_channel = c;
        }
    }
    for (i = 0; i < bt_device_count(); i++) {
        bt_device_t d;

        if (!bt_device_get(i, &d) || d.classic)
            continue;
        ble_total++;
        ble_adverts += d.sightings;
        if (d.rssi > BTX_STRONG_DBM)
            ble_strong++;
    }
    score = busiest + ble_strong + ble_total / 4;
    if (score < BTX_BUSY_SCORE) {
        level = "QUIET";
        level_colour = COL_GREEN;
    } else if (score < BTX_CROWDED_SCORE) {
        level = "BUSY";
        level_colour = COL_YELLOW;
    } else {
        level = "CROWDED";
        level_colour = COL_RED;
    }

    screen_clear();
    put_row(1, "2.4 GHZ CROWDING", COL_CYAN);
    put_at(1, 2, "WI-FI NETWORKS ON EACH CHANNEL", COL_WHITE);
    {
        unsigned top = 1;
        int base = BTX_GRAPH_Y + BTX_GRAPH_H;

        for (c = 1; c <= BTX_CHANNELS; c++)
            if (wifi_on[c] > top)
                top = wifi_on[c];
        gfx_rect(COL_WHITE, BTX_GRAPH_X - 3, base, BTX_CHANNELS * BTX_STEP + 4, 1);
        for (c = 1; c <= BTX_CHANNELS; c++) {
            int x = BTX_GRAPH_X + (int)(c - 1) * BTX_STEP;
            unsigned h = wifi_on[c] * BTX_GRAPH_H / top;

            if (wifi_on[c] > 0) {
                if (h < 3)
                    h = 3;
                gfx_rect(wifi_on[c] <= 1 ? COL_GREEN : (wifi_on[c] <= 3 ? COL_YELLOW : COL_RED), x, base - (int)h, BTX_BAR_W, (int)h);
                snprintf(line, sizeof line, "%u", wifi_on[c] > 9 ? 9u : wifi_on[c]);
                put_at((x + BTX_BAR_W / 2) / 8, (base - (int)h) / 10 - 1, line, COL_WHITE);
            }
            snprintf(line, sizeof line, "%u", c);
            put_at((x + BTX_BAR_W / 2) / 8 - (c >= 10 ? 1 : 0), BTX_ROW_LABEL, line, COL_WHITE);
        }
    }
    snprintf(line, sizeof line, "%u WI-FI, BUSIEST AROUND CHANNEL %u", total_wifi, busiest_channel);
    put_at(1, BTX_ROW_TEXT, line, COL_WHITE);
    snprintf(line, sizeof line, "%u BLUETOOTH LE DEVICES, %u STRONG", ble_total, ble_strong);
    put_at(1, BTX_ROW_TEXT + 1, line, COL_WHITE);
    snprintf(line, sizeof line, "%u ADVERTS IN %u SECONDS", ble_adverts, (unsigned)BTX_SECONDS);
    put_at(1, BTX_ROW_TEXT + 2, line, COL_WHITE);
    gfx_hbar(BTX_GRAPH_X, BTX_ROW_BAR * 10, 10, BTX_BAR_LENGTH, score > BTX_CROWDED_SCORE * 2 ? BTX_CROWDED_SCORE * 2 : score,
             BTX_CROWDED_SCORE * 2, level_colour);
    gfx_frame(COL_WHITE, BTX_GRAPH_X - 1, BTX_ROW_BAR * 10 - 1, BTX_BAR_LENGTH + 2, 12);
    snprintf(line, sizeof line, "THE 2.4 GHZ BAND IS %s", level);
    put_at(1, BTX_ROW_VERDICT, line, level_colour);
    if (score >= BTX_BUSY_SCORE) {
        put_at(1, BTX_ROW_VERDICT + 1, "TRY THE QUIETEST CHANNEL OF 1, 6, 11", COL_WHITE);
        put_at(1, BTX_ROW_VERDICT + 2, "(CHANNEL SURVEY) OR MOVE CLOSER.", COL_WHITE);
    } else {
        put_at(1, BTX_ROW_VERDICT + 1, "NOT A LIKELY CAUSE OF A SLOW LINK.", COL_WHITE);
    }
    put_row(BTX_ROW_HELP, "BLE ADVERTISES AT 2402, 2426, 2480 MHZ", COL_WHITE);
    put_row(BTX_ROW_HELP + 1, "ANY KEY", COL_WHITE);
    (void)wait_any_key();
}
