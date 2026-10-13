/*
 * btscan.c - the list of Bluetooth devices around, strongest signal first: Low
 * Energy devices as they announce themselves, or Classic devices found by
 * inquiry. Enter shows the details of one; from the list a device can be
 * followed with the signal meter or connected to and read.
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
#include "toolsui.h"
#include "netthing_const.h"

static bt_device_t view[BT_DEVICES_MAX];
static unsigned    order[BT_DEVICES_MAX];       /* indexes into view[], strongest signal first */
static unsigned    view_count;

void bt_device_label(const bt_device_t *d, char *out, unsigned size)
{
    const char *maker = d->adv.has_company ? btd_company_name(d->adv.company) : "";

    if (d->adv.has_name)
        snprintf(out, size, "%s", d->adv.name);
    else if (maker[0])
        snprintf(out, size, "%s %02X:%02X", maker, d->address[4], d->address[5]);
    else
        snprintf(out, size, "%02X:%02X:%02X", d->address[3], d->address[4], d->address[5]);
}

/* Copy the table and sort it: only the Classic devices or only the Low Energy ones, only those heard
 * lately, the strongest first. */
static void refresh_view(bool classic)
{
    unsigned i, count = bt_device_count(), j;
    uint32_t now = plat_now_ms();

    view_count = 0;
    for (i = 0; i < count && view_count < BT_DEVICES_MAX; i++) {
        bt_device_t d;

        if (!bt_device_get(i, &d) || d.classic != classic)
            continue;
        if (now - d.last_ms > BTL_HIDE_MS)
            continue;
        view[view_count] = d;
        for (j = view_count; j > 0 && view[order[j - 1]].rssi < d.rssi; j--)
            order[j] = order[j - 1];
        order[j] = view_count;
        view_count++;
    }
}

/* ===================================================================== */
/*  The details of one device                                             */
/* ===================================================================== */
static void show_details(const bt_device_t *d)
{
    char line[TOOL_LINE_MAX], text[BT_ADDR_TEXT_MAX];
    unsigned row = BTD_ROW_FIRST, i;
    const char *maker;

    lcd_clear_screen();
    put_row(2, "DEVICE DETAILS", COL_CYAN);
    put_at(BTD_COL_LABEL, (int)row, "NAME", COL_CYAN);
    put_at(BTD_COL_VALUE, (int)row++, d->adv.has_name ? d->adv.name : "(NONE)", COL_WHITE);
    btd_format_address(d->address, text, sizeof text);
    put_at(BTD_COL_LABEL, (int)row, "ADDRESS", COL_CYAN);
    put_at(BTD_COL_VALUE, (int)row++, text, COL_WHITE);
    put_at(BTD_COL_LABEL, (int)row, "TYPE", COL_CYAN);
    if (d->classic)
        snprintf(line, sizeof line, "CLASSIC, CLASS %06X", (unsigned)d->class_of_device);
    else
        snprintf(line, sizeof line, "%s", d->address_type == 0 ? "LOW ENERGY, PUBLIC ADDRESS" :
                 (btd_address_is_private(d->address_type, d->address) ? "LOW ENERGY, PRIVATE ADDRESS" :
                  "LOW ENERGY, FIXED RANDOM ADDRESS"));
    put_at(BTD_COL_VALUE, (int)row++, line, COL_WHITE);
    put_at(BTD_COL_LABEL, (int)row, "KIND", COL_CYAN);
    if (d->adv.tracker != BT_TRACKER_NONE)
        snprintf(line, sizeof line, "%s (%s)", btd_kind_name(d->adv.kind), btd_tracker_name(d->adv.tracker));
    else
        snprintf(line, sizeof line, "%s", btd_kind_name(d->adv.kind));
    put_at(BTD_COL_VALUE, (int)row++, line, d->adv.kind == BT_KIND_TRACKER ? COL_YELLOW : COL_WHITE);
    maker = d->adv.has_company ? btd_company_name(d->adv.company) : "";
    put_at(BTD_COL_LABEL, (int)row, "MAKER", COL_CYAN);
    if (d->adv.has_company && maker[0])
        snprintf(line, sizeof line, "%s", maker);
    else if (d->adv.has_company)
        snprintf(line, sizeof line, "COMPANY 0x%04X", (unsigned)d->adv.company);
    else
        snprintf(line, sizeof line, "NOT GIVEN");
    put_at(BTD_COL_VALUE, (int)row++, line, COL_WHITE);
    put_at(BTD_COL_LABEL, (int)row, "SIGNAL", COL_CYAN);
    snprintf(line, sizeof line, "%d DBM  BEST %d", (int)d->rssi, (int)d->rssi_best);
    put_at(BTD_COL_VALUE, (int)row++, line, bars_colour(bars_for(d->rssi)));
    if (d->sightings > 1) {
        put_at(BTD_COL_LABEL, (int)row, "HEARD", COL_CYAN);
        snprintf(line, sizeof line, "%u TIMES, EVERY %u MS", (unsigned)d->sightings,
                 (unsigned)((d->last_ms - d->first_ms) / (d->sightings - 1u)));
        put_at(BTD_COL_VALUE, (int)row++, line, COL_WHITE);
    }
    if (d->adv.has_tx_power) {
        put_at(BTD_COL_LABEL, (int)row, "TX POWER", COL_CYAN);
        snprintf(line, sizeof line, "%d DBM", (int)d->adv.tx_power);
        put_at(BTD_COL_VALUE, (int)row++, line, COL_WHITE);
    }
    row++;
    if (d->adv.service_count > 0) {
        put_at(BTD_COL_LABEL, (int)row++, "SERVICES IT ANNOUNCES", COL_CYAN);
        for (i = 0; i < d->adv.service_count && row < BTD_ROW_LAST; i++) {
            const char *name = btd_service_name(d->adv.services[i]);

            if (name[0])
                snprintf(line, sizeof line, "%04X  %s", (unsigned)d->adv.services[i], name);
            else
                snprintf(line, sizeof line, "%04X", (unsigned)d->adv.services[i]);
            put_at(BTD_COL_LABEL + 2, (int)row++, line, COL_WHITE);
        }
    }
    if (d->adv.tracker == BT_TRACKER_FIND_MY)
{
        put_at(BTD_COL_LABEL, BTD_ROW_NOTE, "A FIND MY TAG, OR AN APPLE DEVICE", COL_YELLOW);
        put_at(BTD_COL_LABEL, BTD_ROW_NOTE + 1, "AWAY FROM ITS OWNER.", COL_YELLOW);
    }
    put_row(BTD_ROW_HELP, d->classic ? "ESC BACK" : "F FIND IT  I READ IT  ESC BACK", COL_WHITE);
}

/* ===================================================================== */
/*  The list                                                              */
/* ===================================================================== */
static void draw_row(unsigned row, const bt_device_t *d, bool selected, uint32_t now)
{
    char line[TOOL_LINE_MAX], label[BTL_NAME_W + 1];
    unsigned bars = bars_for(d->rssi), i;
    bool stale = now - d->last_ms > BTL_STALE_MS;
    int y = BTL_ROW_FIRST + (int)row;

    clear_row(y);
    put_at(0, y, selected ? ">" : " ", COL_YELLOW);
    bt_device_label(d, line, sizeof line);
    snprintf(label, sizeof label, "%.*s", BTL_NAME_W, line);
    put_at(2, y, label, stale ? COL_YELLOW : COL_WHITE);
    for (i = 0; i < BAR_CELLS; i++)
        line[i] = i < bars ? BAR_FULL : BAR_EMPTY;
    line[BAR_CELLS] = '\0';
    put_at(BTL_COL_BARS, y, line, bars_colour(bars));
    snprintf(line, sizeof line, "%4d", (int)d->rssi);
    put_at(BTL_COL_DBM, y, line, COL_WHITE);
    put_at(BTL_COL_KIND, y, btd_kind_name(d->adv.kind), d->adv.kind == BT_KIND_TRACKER ? COL_YELLOW : COL_WHITE);
}

static void draw_list(unsigned top, unsigned chosen, bool classic, bool paused, int mode)
{
    char line[TOOL_LINE_MAX];
    unsigned row;
    uint32_t now = plat_now_ms();

    snprintf(line, sizeof line, "%s  %u DEVICE%s  %s", classic ? "CLASSIC" : "BLUETOOTH", view_count,
             view_count == 1 ? "" : "S", paused ? "PAUSED" : (classic ? (bt_inquiry_busy() ? "SEARCHING" : "DONE") : "SCANNING"));
    clear_row(BTL_ROW_HEADING);
    put_at(1, BTL_ROW_HEADING, line, COL_CYAN);
    for (row = 0; row < BTL_ROWS; row++) {
        unsigned index = top + row;

        if (index < view_count)
            draw_row(row, &view[order[index]], index == chosen, now);
        else
            clear_row(BTL_ROW_FIRST + (int)row);
    }
    if (classic)
        put_row(BTL_ROW_HELP, "UP DOWN  ENTER DETAILS", COL_WHITE);
    else
        put_row(BTL_ROW_HELP, mode == BT_LIST_FIND ? "UP DOWN  ENTER FOLLOWS THE SIGNAL" :
                (mode == BT_LIST_INSPECT ? "UP DOWN  ENTER CONNECTS AND READS" : "UP DOWN  ENTER DETAILS  F FIND  I READ"), COL_WHITE);
    put_row(BTL_ROW_HELP + 1, classic ? "R SEARCH AGAIN  C CLEAR  ESC BACK" : "C CLEAR  P PAUSE  ESC BACK", COL_WHITE);
}

static void list_screen(bool classic, int mode)
{
    unsigned top = 0, chosen = 0;
    uint8_t selected[6] = { 0 };
    bool have_selected = false, paused = false, redraw = true;
    uint32_t last = 0;

    lcd_clear_screen();
    bt_devices_clear();
    if (classic)
        (void)bt_inquiry_start(BTC_SECONDS);
    else
        (void)bt_le_scan_start();
    drain_keys();
    for (;;) {
        uint8_t key = poll_key();
        uint32_t now = plat_now_ms();

        if (key == KEY_ESC)
            break;
        if (now - last >= BTL_REFRESH_MS) {
            unsigned i;

            last = now;
            refresh_view(classic);
            if (have_selected) {
                chosen = 0;
                for (i = 0; i < view_count; i++)
                    if (memcmp(view[order[i]].address, selected, 6) == 0)
                        chosen = i;
            }
            if (chosen >= view_count)
                chosen = view_count ? view_count - 1 : 0;
            if (chosen < top)
                top = chosen;
            if (chosen >= top + BTL_ROWS)
                top = chosen - BTL_ROWS + 1;
            redraw = true;
        }
        if (key == KEY_DOWN && chosen + 1 < view_count) {
            chosen++;
            memcpy(selected, view[order[chosen]].address, 6);
            have_selected = true;
            redraw = true;
        } else if (key == KEY_UP && chosen > 0) {
            chosen--;
            memcpy(selected, view[order[chosen]].address, 6);
            have_selected = true;
            redraw = true;
        } else if (key == 'c' || key == 'C') {
            bt_devices_clear();
            view_count = 0;
            have_selected = false;
            chosen = top = 0;
            redraw = true;
        } else if (classic && (key == 'r' || key == 'R')) {
            if (!bt_inquiry_busy())
                (void)bt_inquiry_start(BTC_SECONDS);
            redraw = true;
        } else if (!classic && (key == 'p' || key == 'P')) {
            paused = !paused;
            if (paused)
                bt_le_scan_stop();
            else
                (void)bt_le_scan_start();
            redraw = true;
        } else if (view_count > 0 && chosen < view_count &&
                   (key == KEY_ENTER || key == KEY_RETURN || (!classic && (key == 'f' || key == 'F' || key == 'i' || key == 'I')))) {
            bt_device_t d = view[order[chosen]];
            int action = mode;

            if (key == 'f' || key == 'F')
                action = BT_LIST_FIND;
            else if (key == 'i' || key == 'I')
                action = BT_LIST_INSPECT;
            if (!classic && action == BT_LIST_FIND)
                bt_find_device(&d);
            else if (!classic && action == BT_LIST_INSPECT)
                bt_inspect_device(&d);
            else {
                uint8_t k;

                show_details(&d);
                drain_keys();
                k = wait_any_key();
                if (!classic && (k == 'f' || k == 'F'))
                    bt_find_device(&d);
                else if (!classic && (k == 'i' || k == 'I'))
                    bt_inspect_device(&d);
            }
            lcd_clear_screen();
            if (!classic && !paused)
                (void)bt_le_scan_start();
            drain_keys();
            last = 0;
            redraw = true;
        }
        if (redraw) {
            draw_list(top, chosen, classic, paused, mode);
            redraw = false;
        }
        plat_sleep_ms(POLL_MS);
    }
    if (classic)
        bt_inquiry_stop();
    else
        bt_le_scan_stop();
}

void tool_bt_list(int mode)
{
    list_screen(false, mode);
}

void tool_bt_classic(void)
{
    list_screen(true, BT_LIST_BROWSE);
}
