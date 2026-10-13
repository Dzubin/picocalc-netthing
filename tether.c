/*
 * tether.c - see tether.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"

#include "platform.h"
#include "ui.h"
#include "btui.h"
#include "netthing_const.h"
#include "tether.h"

#define STATE_OFF       0
#define STATE_STARTING  1
#define STATE_READY     2
#define STATE_FAILED    3

typedef struct {
    char    label[TETHER_NAME_W + 1];
    int8_t  rssi;
} slot_t;

static int      state = STATE_OFF;
static uint32_t started_ms, looked_ms;
static slot_t   slots[TETHER_SLOTS], shown[TETHER_SLOTS];
static unsigned slot_count, shown_count;

void tether_open(void)
{
    if (state != STATE_OFF)
        return;
    bt_power_on();
    state = STATE_STARTING;
    started_ms = plat_now_ms();
    looked_ms = started_ms;
    slot_count = shown_count = 0;
}

void tether_close(void)
{
    if (state != STATE_OFF) {
        bt_inquiry_stop();
        bt_power_off();
        bt_devices_clear();
    }
    state = STATE_OFF;
    slot_count = shown_count = 0;
}

bool tether_ready(void)
{
    return state == STATE_READY;
}

bool tether_search(void)
{
    return state == STATE_READY && bt_inquiry_start(TETHER_INQUIRY_SECONDS) == 0;
}

bool tether_busy(void)
{
    return state == STATE_READY && bt_inquiry_busy();
}

/* Pick the phones that were heard lately, strongest signal first. */
static void collect(void)
{
    unsigned i, count = bt_device_count(), j;
    uint32_t now = plat_now_ms();

    slot_count = 0;
    for (i = 0; i < count; i++) {
        bt_device_t d;
        char label[BT_TEXT_MAX + 1];

        if (!bt_device_get(i, &d) || !d.classic)
            continue;
        if (TETHER_CLASS_MAJOR(d.class_of_device) != TETHER_MAJOR_PHONE)
            continue;
        if (now - d.last_ms > TETHER_HIDE_MS)
            continue;
        bt_device_label(&d, label, sizeof label);
        if (slot_count == TETHER_SLOTS && slots[TETHER_SLOTS - 1].rssi >= d.rssi)
            continue;                   /* weaker than all four already chosen */
        j = slot_count == TETHER_SLOTS ? TETHER_SLOTS - 1 : slot_count;
        for (; j > 0 && slots[j - 1].rssi < d.rssi; j--)
            slots[j] = slots[j - 1];
        snprintf(slots[j].label, sizeof slots[j].label, "%.*s", TETHER_NAME_W, label);
        slots[j].rssi = d.rssi;
        if (slot_count < TETHER_SLOTS)
            slot_count++;
    }
}

bool tether_poll(void)
{
    uint32_t now = plat_now_ms();
    bool changed = false;

    if (state == STATE_STARTING) {
        if (bt_power_state() == BT_POWER_READY) {
            state = STATE_READY;
            changed = true;
        } else if (bt_power_state() == BT_POWER_FAILED || now - started_ms > TETHER_START_TIMEOUT_MS) {
            bt_power_off();
            state = STATE_FAILED;
            return true;
        }
    }
    if (state != STATE_READY)
        return changed;
    if (now - looked_ms >= TETHER_REFRESH_MS) {
        looked_ms = now;
        collect();
        if (slot_count != shown_count || memcmp(slots, shown, sizeof slots) != 0) {
            memcpy(shown, slots, sizeof shown);
            shown_count = slot_count;
            changed = true;
        }
    }
    return changed;
}

void tether_draw(const char *what)
{
    char text[TCOLS + 1];
    unsigned i, k, bars;

    clear_row(TETHER_ROW_HEADING);
    put_at(0, TETHER_ROW_HEADING, TETHER_HEADING, COL_CYAN);
    put_at(TCOLS - (int)strlen(what), TETHER_ROW_HEADING, what, COL_CYAN);
    for (i = 0; i < TETHER_SLOTS; i++) {
        int row = TETHER_ROW_FIRST + (int)i;

        clear_row(row);
        if (i < shown_count) {
            text[0] = (char)('1' + i);
            text[1] = '\0';
            put_at(TETHER_COL_MARKER, row, text, COL_GREEN);
            put_at(TETHER_COL_NAME, row, shown[i].label, COL_WHITE);
            bars = bars_for(shown[i].rssi);
            for (k = 0; k < BAR_CELLS; k++)
                text[k] = k < bars ? BAR_FULL : BAR_EMPTY;
            text[BAR_CELLS] = '\0';
            put_at(TETHER_COL_BARS, row, text, bars_colour(bars));
            snprintf(text, sizeof text, "%4d", (int)shown[i].rssi);
            put_at(TETHER_COL_DBM, row, text, COL_WHITE);
        }
    }
    if (shown_count == 0) {
        if (state == STATE_FAILED)
            put_at(2, TETHER_ROW_FIRST, TETHER_UNAVAILABLE, COL_YELLOW);
        else if (state == STATE_READY)
            put_at(2, TETHER_ROW_FIRST, TETHER_NONE_FOUND, COL_WHITE);
    }
}
