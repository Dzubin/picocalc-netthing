/*
 * ports.c - port check: try to open a TCP connection to each port of a list on
 * a host and say whether it is open (it answered), closed (the host refused it)
 * or filtered (no answer: a firewall drops it, or the host is off). The list
 * is typed as "80,443,20-25" (portlist.c reads it).
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
#include "portlist.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

/* What each port gave, for the grid. */
static uint16_t result_port[PORTLIST_MAX];
static uint16_t result_ms[PORTLIST_MAX];
static uint8_t  result_state[PORTLIST_MAX];     /* PORTS_OPEN, PORTS_CLOSED or PORTS_SILENT */

static void draw_grid(unsigned tested, unsigned selected)
{
    unsigned i;
    char line[TOOL_LINE_MAX];

    lcd_clear_screen();
    put_row(2, "PORT CHECK", COL_CYAN);
    for (i = 0; i < tested; i++) {
        int x = PORTGRID_X + (int)(i % PORTGRID_COLUMNS) * PORTGRID_STEP;
        int y = PORTGRID_Y + (int)(i / PORTGRID_COLUMNS) * PORTGRID_STEP;

        if (result_state[i] == PORTS_OPEN)
            gfx_rect(COL_GREEN, x, y, PORTGRID_SQUARE, PORTGRID_SQUARE);
        else if (result_state[i] == PORTS_SILENT)
            gfx_rect(COL_YELLOW, x, y, PORTGRID_SQUARE, PORTGRID_SQUARE);
        else
            gfx_frame(COL_WHITE, x, y, PORTGRID_SQUARE, PORTGRID_SQUARE);
        if (i == selected)
            gfx_frame(COL_MAGENTA, x - 3, y - 3, PORTGRID_SQUARE + 6, PORTGRID_SQUARE + 6);
    }
    put_at(1, PORTGRID_ROW_LEGEND, "GREEN OPEN  WHITE FRAME CLOSED", COL_WHITE);
    put_at(1, PORTGRID_ROW_LEGEND + 1, "YELLOW NO ANSWER   ARROWS MOVE", COL_WHITE);
    if (selected < tested) {
        snprintf(line, sizeof line, "PORT %u  %s", (unsigned)result_port[selected], portlist_name(result_port[selected]));
        put_at(1, PORTGRID_ROW_INFO, line, COL_CYAN);
        if (result_state[selected] == PORTS_OPEN)
            snprintf(line, sizeof line, "OPEN, CONNECTED IN %u MS", (unsigned)result_ms[selected]);
        else
            snprintf(line, sizeof line, result_state[selected] == PORTS_CLOSED ? "CLOSED (THE HOST REFUSED IT)" : "NO ANSWER (FILTERED OR HOST OFF)");
        put_at(1, PORTGRID_ROW_INFO + 1, line, result_state[selected] == PORTS_OPEN ? COL_GREEN : COL_YELLOW);
    }
    put_row(TOOL_ROW_HELP, "ESC BACK", COL_WHITE);
}

/* The ports as a grid of squares; the arrow keys choose one. */
static void show_grid(unsigned tested)
{
    unsigned selected = 0;

    drain_keys();
    for (;;) {
        uint8_t key;

        draw_grid(tested, selected);
        key = wait_any_key();
        if (key == KEY_ESC)
            return;
        if (key == KEY_RIGHT && selected + 1 < tested)
            selected++;
        else if (key == KEY_LEFT && selected > 0)
            selected--;
        else if (key == KEY_DOWN && selected + PORTGRID_COLUMNS < tested)
            selected += PORTGRID_COLUMNS;
        else if (key == KEY_UP && selected >= PORTGRID_COLUMNS)
            selected -= PORTGRID_COLUMNS;
    }
}

void tool_ports(void)
{
    char host[TOOL_HOST_MAX + 1], addr[NET_ADDR_MAX], line[TOOL_LINE_MAX], text[SETTINGS_PORTS_MAX + 1];
    uint16_t ports[PORTLIST_MAX];
    unsigned open_count = 0, i, tested = 0;
    int count, how;
    bool cancelled = false;

    tools_settings_load();
    snprintf(host, sizeof host, "%s", tools_settings()->ping);
    if (!ask_text("PORT CHECK", "", "HOST OR ADDRESS", host, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(text, sizeof text, "%s", tools_settings()->ports);
    for (;;) {
        if (!ask_text("PORT CHECK", host, "PORTS (80,443 OR 20-25)", text, SETTINGS_PORTS_MAX, 1, false, false))
            return;
        count = portlist_parse(text, ports);
        if (count > 0)
            break;
        notice(PROMPT_ROW_NOTE, "NOT A LIST OF PORTS (64 AT MOST)", COL_YELLOW);
    }
    snprintf(tools_settings()->ping, sizeof tools_settings()->ping, "%s", host);
    snprintf(tools_settings()->ports, sizeof tools_settings()->ports, "%s", text);
    tools_settings_save();

    tools_screen("PORT CHECK", host);
    if ((how = tools_resolve(host, addr, sizeof addr)) != 0) {
        tools_resolve_failed(how);
        return;
    }
    snprintf(line, sizeof line, "TRYING %d PORT%s ON %s", count, count == 1 ? "" : "S", addr);
    tools_out_line(line, COL_WHITE);
    drain_keys();
    for (i = 0; i < (unsigned)count && !cancelled; i++) {
        uint32_t started = plat_now_ms();
        unsigned ms = 0;
        int state = NET_BUSY;
        const char *word;
        uint16_t colour;

        if (net_probe_start(addr, ports[i]) != 0) {
            state = NET_FAILED;
        } else {
            while (state == NET_BUSY && plat_now_ms() - started < PORTS_TIMEOUT_MS) {
                state = net_probe_poll(&ms);
                if (poll_key() == KEY_ESC) {
                    cancelled = true;
                    break;
                }
                plat_sleep_ms(POLL_MS);
            }
        }
        net_probe_stop();
        if (cancelled)
            break;
        result_port[i] = ports[i];
        result_ms[i] = (uint16_t)ms;
        result_state[i] = state == NET_DONE ? PORTS_OPEN : (state == NET_FAILED ? PORTS_CLOSED : PORTS_SILENT);
        tested = i + 1;
        if (state == NET_DONE) {
            word = "OPEN";
            colour = COL_GREEN;
            open_count++;
        } else if (state == NET_FAILED) {
            word = "CLOSED";
            colour = COL_WHITE;
        } else {
            word = "NO ANSWER";
            colour = COL_YELLOW;
        }
        if (state == NET_DONE)
            snprintf(line, sizeof line, "%5u %-9s %-9s %u MS", (unsigned)ports[i], portlist_name(ports[i]), word, ms);
        else
            snprintf(line, sizeof line, "%5u %-9s %s", (unsigned)ports[i], portlist_name(ports[i]), word);
        tools_out_page(line, colour);
    }
    if (cancelled) {
        tools_out_page("STOPPED", COL_YELLOW);
    } else {
        snprintf(line, sizeof line, "%u OPEN OF %d", open_count, count);
        tools_out_page(line, COL_CYAN);
    }
    if (tested > 0) {
        put_row(TOOL_ROW_HELP, "ANY KEY: THE GRID", COL_WHITE);
        (void)wait_any_key();
        show_grid(tested);
    }
    tools_finish();
}
