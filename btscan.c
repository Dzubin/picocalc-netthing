/*
 * btscan.c - the Bluetooth screen: the Low Energy devices around, strongest
 * signal first, and below them the Classic devices found by inquiry. The Low
 * Energy scan runs in windows, and after a few of them it stops for one Classic
 * inquiry. The cursor runs down the first list and on into the second; Enter is
 * the connect key (not built yet), D shows the details of a device, and the other
 * Bluetooth tools are keys on this screen.
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
#include "link.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static bt_device_t view[BT_DEVICES_MAX];        /* what was heard lately, both kinds */
static unsigned    order[BT_DEVICES_MAX];       /* indexes into view[]: the Low Energy devices, strongest signal first */
static unsigned    le_count;
static unsigned    corder[BT_DEVICES_MAX];      /* indexes into view[]: the Classic devices, strongest signal first */
static unsigned    classic_count;
static bt_device_t pan_target;                  /* the phone to tether to */
static bool        paired[BT_DEVICES_MAX];      /* by index into view[]: a link key is saved for it */
static uint32_t    shown_now;                   /* the clock the lists are judged by; it stands still while paused */

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

/* Copy the table of devices heard lately and sort it into the two lists, the strongest signal first. */
static void refresh_view(void)
{
    unsigned i, count = bt_device_count(), j, n = 0;
    uint32_t now = shown_now;

    le_count = classic_count = 0;
    for (i = 0; i < count && n < BT_DEVICES_MAX; i++) {
        bt_device_t d;
        unsigned *list, *size;

        if (!bt_device_get(i, &d) || now - d.last_ms > BTL_HIDE_MS)
            continue;
        view[n] = d;
        paired[n] = d.classic && bt_device_paired(d.address);
        list = d.classic ? corder : order;
        size = d.classic ? &classic_count : &le_count;
        /* paired devices first, so the phone to connect to stays at the top; then the strongest signal */
        for (j = *size; j > 0 && (paired[list[j - 1]] < paired[n] ||
                                  (paired[list[j - 1]] == paired[n] && view[list[j - 1]].rssi < d.rssi)); j--)
            list[j] = list[j - 1];
        list[j] = n;
        (*size)++;
        n++;
    }
}

/* The device under the cursor: the Low Energy list first, then the Classic one. */
static const bt_device_t *device_at(unsigned index)
{
    return index < le_count ? &view[order[index]] : &view[corder[index - le_count]];
}

/* ===================================================================== */
/*  The details of one device                                             */
/* ===================================================================== */
static void show_details(const bt_device_t *d)
{
    char line[TOOL_LINE_MAX], text[BT_ADDR_TEXT_MAX];
    unsigned row = BTD_ROW_FIRST, i;
    const char *maker;

    screen_clear();
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
    put_row(BTD_ROW_HELP, d->classic ? "ESC=BACK" : "F=FIND I=READ ESC=BACK", COL_WHITE);
}

/* ===================================================================== */
/*  The list                                                              */
/* ===================================================================== */
static void draw_row(int y, const bt_device_t *d, bool selected, uint32_t now, bool saved)
{
    char line[TOOL_LINE_MAX], label[BTL_NAME_W + 1];
    unsigned bars = bars_for(d->rssi), i;
    bool stale = now - d->last_ms > BTL_STALE_MS;

    clear_row(y);
    put_at(0, y, selected ? ">" : " ", COL_YELLOW);
    bt_device_label(d, line, sizeof line);
    snprintf(label, sizeof label, "%.*s", BTL_NAME_W, line);
    put_at(2, y, label, saved ? COL_GREEN : (stale ? COL_YELLOW : COL_WHITE));
    for (i = 0; i < BAR_CELLS; i++)
        line[i] = i < bars ? BAR_FULL : BAR_EMPTY;
    line[BAR_CELLS] = '\0';
    put_at(BTL_COL_BARS, y, line, bars_colour(bars));
    snprintf(line, sizeof line, "%4d", (int)d->rssi);
    put_at(BTL_COL_DBM, y, line, COL_WHITE);
    put_at(BTL_COL_KIND, y, btd_kind_name(d->adv.kind), d->adv.kind == BT_KIND_TRACKER ? COL_YELLOW : COL_WHITE);
}

/* The Classic area: a heading with the state of the cycle at its right end, and
 * the devices from number `first` on. */
static void draw_classic_area(const char *cycle, unsigned first, unsigned chosen, uint32_t now)
{
    char line[TOOL_LINE_MAX];
    unsigned i;

    clear_row(BTX_ROW_HEADING);
    if (classic_count > BTX_ROWS)
        snprintf(line, sizeof line, "CLASSIC  %u-%u OF %u", first + 1, first + BTX_ROWS, classic_count);
    else
        snprintf(line, sizeof line, "CLASSIC  %u", classic_count);
    put_at(1, BTX_ROW_HEADING, line, COL_CYAN);
    put_at(TCOLS - (int)strlen(cycle), BTX_ROW_HEADING, cycle, COL_CYAN);
    for (i = 0; i < BTX_ROWS; i++) {
        if (first + i < classic_count)
            draw_row(BTX_ROW_FIRST + (int)i, &view[corder[first + i]], chosen == le_count + first + i, now, paired[corder[first + i]]);
        else
            clear_row(BTX_ROW_FIRST + (int)i);
    }
    if (classic_count == 0)
        put_at(2, BTX_ROW_FIRST, "NONE HEARD YET", COL_WHITE);
}

static void draw_list(unsigned top, unsigned cfirst, unsigned chosen, bool paused, const char *cycle)
{
    char line[TOOL_LINE_MAX];
    unsigned row;
    const char *state = paused ? "PAUSED" : "SCANNING";
    uint32_t now = shown_now;

    clear_row(BTL_ROW_HEADING);
    snprintf(line, sizeof line, "BT SCAN  %u FOUND", le_count);
    put_at(0, BTL_ROW_HEADING, line, COL_CYAN);
    put_at(TCOLS - (int)strlen(state), BTL_ROW_HEADING, state, paused ? COL_YELLOW : COL_CYAN);
    for (row = 0; row < BTX_LE_ROWS; row++) {
        unsigned index = top + row;

        if (index < le_count)
            draw_row(BTL_ROW_FIRST + (int)row, &view[order[index]], index == chosen, now, false);
        else
            clear_row(BTL_ROW_FIRST + (int)row);
    }
    draw_classic_area(cycle, cfirst, chosen, now);
    put_row(BTX_ROW_PAUSE, "P=PAUSE", COL_WHITE);
    put_row(BTX_ROW_HELP, "ENTER=CONNECT D=DETAILS L=LAN", COL_WHITE);
    put_row(BTX_ROW_HELP + 1, "F=FIND I=READ T=TRACKER X=CROWD", COL_WHITE);
    put_row(BTX_ROW_HELP + 2, "R=RESET W=WIFI ESC=TITLE", COL_WHITE);
}

const bt_device_t *tool_bt_pan_target(void)
{
    return &pan_target;
}

/* Returns why it ended: BT_EXIT_BACK, BT_EXIT_WIFI, or the key 'T', 'X', 'L' or 'P' (tether to pan_target). */
int tool_bt_nearby(void)
{
    unsigned top = 0, cfirst = 0, chosen = 0;
    uint8_t selected[6] = { 0 };
    bool have_selected = false, paused = false, redraw = true, inquiring = false;
    unsigned windows = 0;
    uint32_t last = 0, window_start = plat_now_ms();
    int exit_code = BT_EXIT_BACK;
    char cycle[16];

    screen_clear();
    bt_devices_clear();
    (void)bt_le_scan_start();
    shown_now = window_start;
    drain_keys();
    for (;;) {
        uint8_t key = poll_key();
        uint32_t now = plat_now_ms();

        if (!paused)
            shown_now = now;        /* paused: nothing ages, so the list stays as it was */
        if (key == KEY_ESC)
            break;
        if (now - last >= BTL_REFRESH_MS) {
            unsigned i, total;

            last = now;
            refresh_view();
            total = le_count + classic_count;
            if (have_selected) {
                chosen = 0;
                for (i = 0; i < le_count; i++)
                    if (memcmp(view[order[i]].address, selected, 6) == 0)
                        chosen = i;
                for (i = 0; i < classic_count; i++)
                    if (memcmp(view[corder[i]].address, selected, 6) == 0)
                        chosen = le_count + i;
            }
            if (chosen >= total)
                chosen = total ? total - 1 : 0;
            if (chosen < le_count) {
                if (chosen < top)
                    top = chosen;
                if (chosen >= top + BTX_LE_ROWS)
                    top = chosen - BTX_LE_ROWS + 1;
            } else {
                unsigned c = chosen - le_count;

                if (c < cfirst)
                    cfirst = c;
                if (c >= cfirst + BTX_ROWS)
                    cfirst = c - BTX_ROWS + 1;
            }
            if (classic_count <= BTX_ROWS)
                cfirst = 0;
            else if (cfirst > classic_count - BTX_ROWS)
                cfirst = classic_count - BTX_ROWS;
            redraw = true;
        }
        if (!paused) {
            /* the windows of Low Energy scanning, then one Classic inquiry */
            if (inquiring) {
                if (!bt_inquiry_busy()) {
                    inquiring = false;
                    windows = 0;
                    window_start = now;
                    (void)bt_le_scan_start();
                    redraw = true;
                }
            } else if (now - window_start >= BTX_WINDOW_MS) {
                window_start = now;
                if (++windows >= BTX_WINDOWS) {
                    bt_le_scan_stop();
                    if (bt_inquiry_start(BTC_SECONDS) == 0) {
                        inquiring = true;
                    } else {
                        (void)bt_le_scan_start();
                        windows = 0;
                    }
                }
                redraw = true;
            }
        }
        if (key == KEY_DOWN && chosen + 1 < le_count + classic_count) {
            chosen++;
            memcpy(selected, device_at(chosen)->address, 6);
            have_selected = true;
            last = 0;
        } else if (key == KEY_UP && chosen > 0) {
            chosen--;
            memcpy(selected, device_at(chosen)->address, 6);
            have_selected = true;
            last = 0;
        } else if (key == 'l' || key == 'L') {
            if (link_kind() == LINK_NONE) {
                notice(BTX_ROW_MESSAGE, LINK_NEEDED, COL_YELLOW);
                redraw = true;
            } else {
                exit_code = 'L';
                break;
            }
        } else if (key == 'w' || key == 'W') {
            exit_code = BT_EXIT_WIFI;
            break;
        } else if (key == 't' || key == 'T' || key == 'x' || key == 'X') {
            exit_code = key >= 'a' ? key - 'a' + 'A' : key;
            break;
        } else if (key == KEY_ENTER || key == KEY_RETURN) {
#if CHIP_BT_TETHER
            if (le_count + classic_count > 0 && chosen >= le_count) {
                pan_target = *device_at(chosen);
                exit_code = 'P';
                break;
            }
            if (le_count + classic_count > 0)
                notice(BTX_ROW_MESSAGE, BTX_NO_TETHER, COL_YELLOW);
#else
            if (le_count + classic_count > 0)
                notice(BTX_ROW_MESSAGE, BTX_NO_TETHER_BUILD, COL_YELLOW);
#endif
            redraw = true;
        } else if (key == 'r' || key == 'R') {
            bt_devices_clear();
            le_count = classic_count = 0;
            have_selected = false;
            chosen = top = cfirst = 0;
            redraw = true;
        } else if (key == 'p' || key == 'P') {
            paused = !paused;
            if (paused) {
                bt_le_scan_stop();
                bt_inquiry_stop();
            } else {
                if (!inquiring)
                    (void)bt_le_scan_start();
                window_start = now;
            }
            redraw = true;
        } else if (le_count + classic_count > 0 && (key == 'd' || key == 'D' || key == 'f' || key == 'F' || key == 'i' || key == 'I')) {
            bt_device_t d = *device_at(chosen);
            bool le = chosen < le_count;
            uint8_t k = key;

            if (key == 'd' || key == 'D') {
                show_details(&d);
                drain_keys();
                k = wait_any_key();
            }
            if (le && (k == 'f' || k == 'F'))
                bt_find_device(&d);
            else if (le && (k == 'i' || k == 'I'))
                bt_inspect_device(&d);
            screen_clear();
            paused = false;                 /* back from another screen: scanning goes on */
            window_start = plat_now_ms();
            if (!inquiring)
                (void)bt_le_scan_start();
            drain_keys();
            last = 0;
            redraw = true;
        }
        if (redraw) {
            if (paused)
                snprintf(cycle, sizeof cycle, "PAUSED");
            else if (inquiring)
                snprintf(cycle, sizeof cycle, "SEARCHING");
            else
                snprintf(cycle, sizeof cycle, "LE %u/%u", windows + 1, (unsigned)BTX_WINDOWS);
            draw_list(top, cfirst, chosen, paused, cycle);
            redraw = false;
        }
        plat_sleep_ms(POLL_MS);
    }
    bt_inquiry_stop();
    bt_le_scan_stop();
    return exit_code;
}
