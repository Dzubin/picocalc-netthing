/*
 * meter.c - signal meter, for walking around to find the best spot: one wide
 * bar that follows the signal strength of the connected network several
 * times a second, the strength in dBm and in words, the best and worst
 * seen, and a strip of the recent past below it. The picture itself
 * (meter_frame, meter_show) is shared with the Bluetooth device finder.
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
#include "meter.h"
#include "ui.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static int8_t   history[METER_SAMPLES];     /* dBm, oldest first */
static unsigned history_count;
static int      best = -127, worst;

void meter_clear_best(void)
{
    best = -127;
    worst = 0;
}

void meter_frame(const char *title, const char *help)
{
    static const int ticks[] = METER_TICKS;
    unsigned i;
    char line[16];

    lcd_clear_screen();
    put_row(0, title, COL_CYAN);
    for (i = 0; i < sizeof ticks / sizeof ticks[0]; i++) {
        int x = METER_X + (int)gfx_signal_height(ticks[i], METER_W);

        snprintf(line, sizeof line, "%d", ticks[i]);
        put_at((x - 12) / 8, METER_ROW_TICKS, line, COL_WHITE);
        gfx_rect(COL_WHITE, x, METER_Y + METER_H + 2, 1, 4);
    }
    put_row(METER_ROW_HELP, help, COL_WHITE);
    history_count = 0;
    meter_clear_best();
}

void meter_message(const char *text, uint16_t colour)
{
    clear_row(METER_ROW_NUMBER);
    put_row(METER_ROW_NUMBER, text, colour);
}

void meter_hint(const char *text, uint16_t colour)
{
    clear_row(METER_ROW_HINT);
    put_row(METER_ROW_HINT, text, colour);
}

void meter_show(int rssi, const char *name, bool add)
{
    static const char *const words[] = SIGNAL_WORDS;
    unsigned bars = bars_for(rssi), i;
    uint16_t colour = bars_colour(bars);
    char line[TOOL_LINE_MAX];

    if (add) {
        if (rssi > best)
            best = rssi;
        if (rssi < worst)
            worst = rssi;
        if (history_count == METER_SAMPLES) {
            memmove(history, history + 1, METER_SAMPLES - 1);
            history_count--;
        }
        history[history_count++] = (int8_t)(rssi < -127 ? -127 : (rssi > 0 ? 0 : rssi));
    }
    clear_row(METER_ROW_NAME);
    snprintf(line, sizeof line, "%.*s", TCOLS - 2, name);
    put_at(1, METER_ROW_NAME, line, COL_WHITE);
    gfx_rect(COL_BG, METER_X, METER_Y, METER_W, METER_H);
    gfx_rect(colour, METER_X, METER_Y, (int)gfx_signal_height(rssi, METER_W), METER_H);
    gfx_frame(COL_WHITE, METER_X - 1, METER_Y - 1, METER_W + 2, METER_H + 2);
    clear_row(METER_ROW_NUMBER);
    snprintf(line, sizeof line, "%d DBM  %s", rssi, words[bars > 0 ? bars - 1 : 0]);
    put_row(METER_ROW_NUMBER, line, colour);
    clear_row(METER_ROW_BEST);
    snprintf(line, sizeof line, "BEST %d   WORST %d", best, worst);
    put_row(METER_ROW_BEST, line, COL_WHITE);
    /* the recent past: a bar for each reading, as tall as the signal */
    gfx_rect(COL_BG, METER_X - 2, METER_STRIP_Y - 2, METER_SAMPLES * METER_STEP + 4, METER_STRIP_H + 4);
    gfx_rect(COL_WHITE, METER_X - 2, METER_STRIP_Y + METER_STRIP_H, METER_SAMPLES * METER_STEP + 3, 1);
    for (i = 0; i < history_count; i++) {
        unsigned h = gfx_signal_height(history[i], METER_STRIP_H);

        if (h < 2)
            h = 2;
        gfx_rect(bars_colour(bars_for(history[i])), METER_X + (int)(METER_SAMPLES - history_count + i) * METER_STEP,
                 METER_STRIP_Y + METER_STRIP_H - (int)h, METER_STEP - 1, (int)h);
    }
}

void tool_meter(void)
{
    wifi_info_t info;
    uint32_t last = 0;

    if (!wifi_info_get(&info)) {
        tools_screen("SIGNAL METER", "");
        tools_out_line("NOT CONNECTED", COL_YELLOW);
        tools_finish();
        return;
    }
    meter_frame("SIGNAL METER", "C CLEAR BEST AND WORST  ESC BACK");
    drain_keys();
    for (;;) {
        uint8_t key = poll_key();
        uint32_t now = plat_now_ms();

        if (key == KEY_ESC)
            return;
        if (key == 'c' || key == 'C')
            meter_clear_best();
        if (now - last >= METER_MS) {
            last = now;
            if (!wifi_info_get(&info)) {
                meter_message("CONNECTION LOST", COL_YELLOW);
                pause_ms(NOTICE_MS);
                return;
            }
            meter_show(info.rssi, info.ssid, true);
        }
        plat_sleep_ms(POLL_MS);
    }
}
