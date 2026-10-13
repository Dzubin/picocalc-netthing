/*
 * menu.c - the tools menu: one letter starts a tool. The status screen opens it
 * with T and comes back to itself when a tool ends.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "ui.h"
#include "tools.h"
#include "netthing_const.h"

static void draw_menu(void)
{
    static const char *const labels[] = MENU_LABELS;
    static const char keys[] = MENU_KEYS;
    char line[TCOLS + 1];
    unsigned i;

    lcd_clear_screen();
    put_row(2, "NETWORK TOOLS", COL_CYAN);
    for (i = 0; labels[i] != NULL; i++) {
        snprintf(line, sizeof line, "%c  %s", keys[i], labels[i]);
        put_at(MENU_COL, MENU_ROW_FIRST + (int)i * MENU_ROW_STEP, line, COL_WHITE);
    }
    put_row(TOOL_ROW_HELP, MENU_HELP, COL_WHITE);
}

void tool_menu(void)
{
    static const char keys[] = MENU_KEYS;

    for (;;) {
        uint8_t key;
        char up;
        const char *at;

        draw_menu();
        drain_keys();
        key = wait_any_key();
        if (key == KEY_ESC)
            return;
        up = (key >= 'a' && key <= 'z') ? (char)(key - 'a' + 'A') : (char)key;
        at = up ? strchr(keys, up) : NULL;
        if (at == NULL)
            continue;
        switch (*at) {
        case 'H': tool_health(); break;
        case 'M': tool_meter(); break;
        case 'P': tool_ping(); break;
        case 'R': tool_ping_gateway(); break;
        case 'C': tool_ping_live(); break;
        case 'T': tool_trace(); break;
        case 'O': tool_ports(); break;
        case 'L': tool_lan(); break;
        case 'N': tool_lookup(); break;
        case 'S': tool_survey(); break;
        case 'G': tool_get(); break;
        case 'V': tool_server_menu(); break;
        case 'K': tool_bt_menu(); break;
        case 'D': tool_dns_check(); break;
        case 'B': tool_speed(); break;
        case 'W': tool_time(); break;
        case 'U': tool_certs(); break;
        default: break;
        }
    }
}
