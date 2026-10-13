/*
 * bttrack.c - tracker alert: scan for the tags that can be found with a phone
 * (Apple's Find My network, Tile, Samsung SmartTag) and show how long each
 * has been in range. A tag that stays in range for many minutes while you move
 * about is probably with you, and may not be yours. A tag changes its address
 * every so often, so a long time is the sign, not an address. This sees only
 * tags that announce themselves in the usual way.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "ui.h"
#include "btui.h"
#include "tools.h"
#include "netthing_const.h"

static bt_device_t found[BT_DEVICES_MAX];
static unsigned    found_count;

/* Collect the tracker-like devices heard lately, the longest in range first. */
static void gather(void)
{
    unsigned i, n = bt_device_count(), j;
    uint32_t now = plat_now_ms();

    found_count = 0;
    for (i = 0; i < n; i++) {
        bt_device_t d;

        if (!bt_device_get(i, &d) || d.classic || d.adv.kind != BT_KIND_TRACKER || now - d.last_ms > BTT_HIDE_MS)
            continue;
        for (j = found_count; j > 0 && (found[j - 1].last_ms - found[j - 1].first_ms) < (d.last_ms - d.first_ms); j--)
            found[j] = found[j - 1];
        found[j] = d;
        found_count++;
    }
}

static void draw(uint32_t now)
{
    unsigned i, shown = 0;
    char line[TOOL_LINE_MAX];

    snprintf(line, sizeof line, "TRACKER ALERT  %u FOUND", found_count);
    clear_row(BTT_ROW_HEADING);
    put_at(1, BTT_ROW_HEADING, line, found_count ? COL_YELLOW : COL_CYAN);
    for (i = 0; i < BTT_ROWS; i++) {
        int y = BTT_ROW_FIRST + (int)i;

        clear_row(y);
        if (i < found_count) {
            const bt_device_t *d = &found[i];
            unsigned seconds = (d->last_ms - d->first_ms) / 1000u, bars = bars_for(d->rssi), k;
            uint16_t colour = seconds >= BTT_ALERT_S ? COL_RED : (seconds >= BTT_WARN_S ? COL_YELLOW : COL_WHITE);
            bool stale = now - d->last_ms > BTT_STALE_MS;

            snprintf(line, sizeof line, "%-11.11s", btd_tracker_name(d->adv.tracker));
            put_at(1, y, line, colour);
            for (k = 0; k < BAR_CELLS; k++)
                line[k] = k < bars ? BAR_FULL : BAR_EMPTY;
            line[BAR_CELLS] = '\0';
            put_at(BTT_COL_BARS, y, line, bars_colour(bars));
            snprintf(line, sizeof line, "%4d", (int)d->rssi);
            put_at(BTT_COL_DBM, y, line, COL_WHITE);
            snprintf(line, sizeof line, "%2um%02us", seconds / 60u, seconds % 60u);
            put_at(BTT_COL_TIME, y, line, colour);
            if (stale)
                put_at(BTT_COL_FLAG, y, "GONE", COL_WHITE);
            else if (seconds >= BTT_WARN_S)
                put_at(BTT_COL_FLAG, y, "WATCH", colour);
            shown++;
        }
    }
    (void)shown;
}

void tool_bt_tracker(void)
{
    uint32_t last = 0;

    lcd_clear_screen();
    bt_devices_clear();
    (void)bt_le_scan_start();
    put_row(BTT_ROW_NOTE, "A TAG IN RANGE FOR MANY MINUTES WHILE", COL_WHITE);
    put_row(BTT_ROW_NOTE + 1, "YOU MOVE ABOUT MAY BE FOLLOWING YOU.", COL_WHITE);
    put_row(BTT_ROW_NOTE + 2, "A SIGNAL STAYING STRONG MEANS IT IS CLOSE.", COL_WHITE);
    put_row(BTT_ROW_HELP, "C CLEAR THE TIMES  ESC BACK", COL_WHITE);
    drain_keys();
    for (;;) {
        uint8_t key = poll_key();
        uint32_t now = plat_now_ms();

        if (key == KEY_ESC)
            break;
        if (key == 'c' || key == 'C') {
            bt_devices_clear();
            found_count = 0;
            last = 0;
        }
        if (now - last >= BTT_REFRESH_MS) {
            last = now;
            gather();
            draw(now);
        }
        plat_sleep_ms(POLL_MS);
    }
    bt_le_scan_stop();
}
