/*
 * lan.c - LAN scan: send an ARP request for every address of a range you type
 * (the detected network, or the range used last time on this network), collect
 * the devices that answer from the ARP table after every few requests, then
 * ping each one for a time, and show the list: address, MAC address and ping.
 * A MAC address that the device made up for itself (a phone or computer
 * with a private address) is in another colour. The arithmetic is lanscan.c.
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
#include "lanscan.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static lan_device_t devices[LAN_MAX_DEVICES];
static unsigned     device_count;

/* Ask for the first and last address, offering the saved range if it is on this network. */
static bool ask_range(uint32_t self, uint32_t mask, uint32_t *first, uint32_t *last)
{
    char text_first[LAN_TEXT_MAX], text_last[LAN_TEXT_MAX];
    uint32_t saved_first, saved_last;
    settings_t *settings = tools_settings();

    lan_default_range(self, mask, first, last);
    if (lan_parse(settings->lan_first, &saved_first) && lan_parse(settings->lan_last, &saved_last) &&
        lan_same_network(saved_first, self, mask) && lan_same_network(saved_last, self, mask) &&
        saved_first <= saved_last) {
        *first = saved_first;
        *last = saved_last;
    }
    for (;;) {
        lan_format(*first, text_first, sizeof text_first);
        lan_format(*last, text_last, sizeof text_last);
        if (!ask_text("LAN SCAN", "", "FIRST ADDRESS", text_first, SETTINGS_ADDR_MAX, 1, false, false))
            return false;
        if (!ask_text("LAN SCAN", "", "LAST ADDRESS", text_last, SETTINGS_ADDR_MAX, 1, false, false))
            return false;
        if (!lan_parse(text_first, first) || !lan_parse(text_last, last) || *first > *last) {
            notice(PROMPT_ROW_NOTE, "THAT IS NOT A RANGE OF ADDRESSES", COL_YELLOW);
            continue;
        }
        if (*last - *first + 1 > LAN_MAX_ADDRESSES) {
            notice(PROMPT_ROW_NOTE, "AT MOST 1024 ADDRESSES", COL_YELLOW);
            continue;
        }
        break;
    }
    snprintf(settings->lan_first, sizeof settings->lan_first, "%s", text_first);
    snprintf(settings->lan_last, sizeof settings->lan_last, "%s", text_last);
    tools_settings_save();
    return true;
}

/* Take what the ARP table knows into the list (only addresses of the range). */
static void harvest(uint32_t first, uint32_t last)
{
    net_arp_entry_t table[ARP_COPY_MAX];
    unsigned n = net_arp_table(table, ARP_COPY_MAX), i;

    for (i = 0; i < n; i++)
        if (table[i].ip >= first && table[i].ip <= last)
            device_count = lan_add_device(devices, device_count, LAN_MAX_DEVICES, table[i].ip, table[i].mac);
}

static void show_progress(const char *what, unsigned done, unsigned total)
{
    char line[TOOL_LINE_MAX];

    snprintf(line, sizeof line, "%s %u OF %u   %u FOUND", what, done, total, device_count);
    clear_row(LAN_ROW_PROGRESS);
    put_at(1, LAN_ROW_PROGRESS, line, COL_YELLOW);
}

/* Phase 1: ask for every address in batches. Returns false if ESC stopped the scan. */
static bool sweep(uint32_t self, uint32_t first, uint32_t last)
{
    uint32_t ip = first;
    unsigned total = last - first + 1, done = 0;

    while (ip <= last && ip >= first) {
        unsigned k;

        for (k = 0; k < LAN_BATCH && ip <= last && ip >= first; ip++) {
            if (ip == self) {
                done++;
                continue;
            }
            if (net_arp_ask(ip) != 0)
                plat_sleep_ms(20);              /* the radio was busy; this address is lost, the next try may work */
            done++;
            k++;
        }
        if (tools_wait_or_escape(LAN_BATCH_WAIT_MS))
            return false;
        harvest(first, last);
        show_progress("ASKING", done, total);
    }
    if (tools_wait_or_escape(LAN_LAST_WAIT_MS))
        return false;
    harvest(first, last);
    return true;
}

/* Phase 2: one ping to each device found (not to ourselves). Returns false if ESC stopped it. */
static bool ping_all(uint32_t self)
{
    unsigned i;
    char text[LAN_TEXT_MAX];

    for (i = 0; i < device_count; i++) {
        unsigned ms = 0;
        bool stopped;

        show_progress("PINGING", i + 1, device_count);
        if (devices[i].ip == self)
            continue;
        lan_format(devices[i].ip, text, sizeof text);
        if (tools_ping_once(text, LAN_PING_TIMEOUT_MS, &ms, &stopped) == NET_DONE)
            devices[i].ping_ms = (int16_t)(ms > 9999 ? 9999 : ms);
        if (stopped)
            return false;
    }
    return true;
}

static void draw_list(unsigned top, uint32_t self, uint32_t gateway)
{
    char line[TOOL_LINE_MAX];
    unsigned row;

    snprintf(line, sizeof line, "LAN SCAN  %u DEVICE%s", device_count, device_count == 1 ? "" : "S");
    clear_row(LAN_ROW_HEADING);
    put_at(1, LAN_ROW_HEADING, line, COL_CYAN);
    put_at(0, LAN_ROW_COLUMNS, LAN_COLUMNS, COL_WHITE);
    for (row = 0; row < LAN_ROWS; row++) {
        unsigned index = top + row;

        clear_row(LAN_ROW_FIRST + (int)row);
        if (index < device_count) {
            const lan_device_t *d = &devices[index];
            char mark[2] = { ' ', '\0' };

            if (d->ip == gateway)
                mark[0] = 'R';
            else if (d->ip == self)
                mark[0] = 'T';
            put_at(LAN_COL_MARK, LAN_ROW_FIRST + (int)row, mark, COL_YELLOW);
            lan_format(d->ip, line, sizeof line);
            put_at(LAN_COL_IP, LAN_ROW_FIRST + (int)row, line, COL_WHITE);
            snprintf(line, sizeof line, "%02X:%02X:%02X:%02X:%02X:%02X", d->mac[0], d->mac[1], d->mac[2],
                     d->mac[3], d->mac[4], d->mac[5]);
            put_at(LAN_COL_MAC, LAN_ROW_FIRST + (int)row, line, lan_mac_private(d->mac) ? LAN_COL_PRIVATE : COL_WHITE);
            if (d->ping_ms == LAN_NO_PING)
                snprintf(line, sizeof line, d->ip == self ? "" : "-");
            else if (d->ping_ms == 0)
                snprintf(line, sizeof line, "<1MS");
            else
                snprintf(line, sizeof line, "%dMS", (int)d->ping_ms);
            put_at(LAN_COL_PING, LAN_ROW_FIRST + (int)row, line, d->ping_ms == LAN_NO_PING ? COL_YELLOW : COL_GREEN);
        }
    }
    put_row(LAN_ROW_HELP, LAN_HELP, COL_WHITE);
    put_row(LAN_ROW_LEGEND, LAN_LEGEND, COL_WHITE);
}

/* ===================================================================== */
/*  The map: a square for every address of the range                      */
/* ===================================================================== */
static uint32_t map_first, map_count;
static int16_t  map_index[LAN_MAX_ADDRESSES];   /* the device on each address of the range, or -1 */

/* The size of a square, the squares in a row, and where square i is. */
static unsigned map_size(void)
{
    return map_count > LANMAP_BIG_AFTER ? LANMAP_SMALL : LANMAP_BIG;
}

static unsigned map_columns(void)
{
    return map_count > LANMAP_BIG_AFTER ? 32 : 16;
}

/* Square i, with the frame of the chosen one. Its own area is cleared first, so it can be drawn again. */
static void draw_cell(unsigned i, bool selected, uint32_t self, uint32_t gateway)
{
    unsigned size = map_size(), step = size + 2, columns = map_columns();
    int x = LANMAP_X + (int)(i % columns) * (int)step, y = LANMAP_Y + (int)(i / columns) * (int)step;
    uint32_t ip = map_first + i;

    gfx_rect(COL_BG, x - 2, y - 2, (int)size + 4, (int)size + 4);
    if (map_index[i] < 0) {
        gfx_rect(COL_WHITE, x + (int)size / 2 - 1, y + (int)size / 2 - 1, 2, 2);
    } else {
        const lan_device_t *d = &devices[map_index[i]];
        uint16_t colour = ip == gateway ? COL_WHITE : (ip == self ? COL_CYAN :
                          (d->ping_ms == LAN_NO_PING ? COL_YELLOW : COL_GREEN));

        gfx_rect(colour, x, y, (int)size, (int)size);
    }
    if (selected)
        gfx_frame(COL_MAGENTA, x - 2, y - 2, (int)size + 4, (int)size + 4);
}

/* The text under the map for the chosen square. */
static void draw_map_info(unsigned selected, uint32_t self, uint32_t gateway)
{
    char line[TOOL_LINE_MAX], text[LAN_TEXT_MAX];
    unsigned row;

    for (row = 0; row < 2; row++)
        clear_row(LANMAP_ROW_INFO + (int)row);
    lan_format(map_first + selected, text, sizeof text);
    if (map_index[selected] < 0) {
        snprintf(line, sizeof line, "%s  NO DEVICE", text);
        put_at(1, LANMAP_ROW_INFO, line, COL_WHITE);
    } else {
        const lan_device_t *d = &devices[map_index[selected]];

        snprintf(line, sizeof line, "%s%s", text, d->ip == gateway ? "  ROUTER" : (d->ip == self ? "  THIS ONE" : ""));
        put_at(1, LANMAP_ROW_INFO, line, COL_CYAN);
        snprintf(line, sizeof line, "%02X:%02X:%02X:%02X:%02X:%02X  %s%s", d->mac[0], d->mac[1], d->mac[2], d->mac[3],
                 d->mac[4], d->mac[5], d->ping_ms == LAN_NO_PING ? "NO PING" : (d->ping_ms == 0 ? "<1MS" : "PING OK"),
                 lan_mac_private(d->mac) ? "  PRIVATE" : "");
        put_at(1, LANMAP_ROW_INFO + 1, line, COL_WHITE);
    }
}

/* The whole map: the screen is cleared and every square drawn. */
static void draw_map(unsigned selected, uint32_t self, uint32_t gateway)
{
    char line[TOOL_LINE_MAX];
    unsigned i;

    for (i = 0; i < map_count; i++)
        map_index[i] = -1;
    for (i = 0; i < device_count; i++)
        if (devices[i].ip >= map_first && devices[i].ip - map_first < map_count)
            map_index[devices[i].ip - map_first] = (int16_t)i;
    lcd_clear_screen();
    snprintf(line, sizeof line, "LAN MAP  %u DEVICE%s", device_count, device_count == 1 ? "" : "S");
    put_at(1, LAN_ROW_HEADING, line, COL_CYAN);
    for (i = 0; i < map_count; i++)
        draw_cell(i, i == selected, self, gateway);
    draw_map_info(selected, self, gateway);
    put_at(1, LANMAP_ROW_INFO + 2, "GREEN PING  YELLOW NONE  WHITE ROUTER", COL_WHITE);
    put_at(1, LANMAP_ROW_INFO + 3, "ARROWS  M LIST  R RESCAN  ESC BACK", COL_WHITE);
}

void tool_lan(void)
{
    uint32_t self, mask, gateway, first, last;
    uint8_t self_mac[6];
    bool stopped, map_view = false;
    unsigned top, selected = 0;

    tools_settings_load();
    if (!net_lan_self(&self, &mask, &gateway, self_mac)) {
        tools_screen("LAN SCAN", "");
        tools_out_line("NO ADDRESS YET (NOT CONNECTED)", COL_YELLOW);
        tools_finish();
        return;
    }
    if (!ask_range(self, mask, &first, &last))
        return;
    for (;;) {
        char line[TOOL_LINE_MAX];

        device_count = 0;
        lcd_clear_screen();
        put_row(2, "LAN SCAN", COL_CYAN);
        snprintf(line, sizeof line, "%u ADDRESSES", (unsigned)(last - first + 1));
        put_row(4, line, COL_WHITE);
        put_row(6, "ESC STOPS AND SHOWS WHAT IS FOUND", COL_WHITE);
        if (self >= first && self <= last)
            device_count = lan_add_device(devices, device_count, LAN_MAX_DEVICES, self, self_mac);
        drain_keys();
        stopped = !sweep(self, first, last);
        if (!stopped)
            stopped = !ping_all(self);
        lcd_clear_screen();
        top = 0;
        selected = 0;
        map_first = first;
        map_count = last - first + 1;
        if (map_view)
            draw_map(selected, self, gateway);
        else
            draw_list(top, self, gateway);
        if (stopped && !map_view)
            put_row(LAN_ROW_PROGRESS, "STOPPED, THE LIST MAY BE SHORT", COL_YELLOW);
        drain_keys();
        for (;;) {
            uint8_t key = wait_any_key();
            unsigned last_top = device_count > LAN_ROWS ? device_count - LAN_ROWS : 0;

            if (key == KEY_ESC)
                return;
            if (key == 'r' || key == 'R')
                break;
            if (key == 'm' || key == 'M') {
                map_view = !map_view;
                lcd_clear_screen();
                if (map_view)
                    draw_map(selected, self, gateway);
                else
                    draw_list(top, self, gateway);
                continue;
            }
            if (map_view) {
                unsigned columns = map_columns(), before = selected;

                if (key == KEY_RIGHT && selected + 1 < map_count)
                    selected++;
                else if (key == KEY_LEFT && selected > 0)
                    selected--;
                else if (key == KEY_DOWN && selected + columns < map_count)
                    selected += columns;
                else if (key == KEY_UP && selected >= columns)
                    selected -= columns;
                else
                    continue;
                draw_cell(before, false, self, gateway);
                draw_cell(selected, true, self, gateway);
                draw_map_info(selected, self, gateway);
                continue;
            }
            if (key == KEY_DOWN && top < last_top)
                top++;
            else if (key == KEY_UP && top > 0)
                top--;
            else if (key == KEY_PAGE_DOWN)
                top = top + LAN_ROWS - 1 < last_top ? top + LAN_ROWS - 1 : last_top;
            else if (key == KEY_PAGE_UP)
                top = top > LAN_ROWS - 1 ? top - (LAN_ROWS - 1) : 0;
            else
                continue;
            draw_list(top, self, gateway);
        }
    }
}
