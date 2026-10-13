/*
 * menu.c - the menus of the tools: one letter starts a tool. The Wi-Fi tools menu
 * (T on the status screen) and the LAN tools menu (L) are here; the Bluetooth and
 * Windows test server menus are with their tools. A menu comes back to the screen
 * that opened it when ESC is pressed.
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

/* Draw a menu and wait for a letter of it. Returns that letter in upper case, or 0 for ESC. */
static char menu_pick(const char *title, const char *keys, const char *const *labels, int row_first, int row_step)
{
    for (;;) {
        char line[TCOLS + 1], up;
        unsigned i;
        uint8_t key;
        const char *at;

        lcd_clear_screen();
        put_row(2, title, COL_CYAN);
        for (i = 0; labels[i] != NULL; i++) {
            snprintf(line, sizeof line, "%c  %s", keys[i], labels[i]);
            put_at(MENU_COL, row_first + (int)i * row_step, line, COL_WHITE);
        }
        put_row(TOOL_ROW_HELP, MENU_HELP, COL_WHITE);
        drain_keys();
        key = wait_any_key();
        if (key == KEY_ESC)
            return 0;
        up = (key >= 'a' && key <= 'z') ? (char)(key - 'a' + 'A') : (char)key;
        at = up ? strchr(keys, up) : NULL;
        if (at != NULL)
            return *at;
    }
}

void tool_menu(void)
{
    static const char *const labels[] = MENU_LABELS;
    static const char keys[] = MENU_KEYS;
    char choice;

    while ((choice = menu_pick(MENU_TITLE, keys, labels, MENU_ROW_FIRST_WIFI, MENU_ROW_STEP_WIFI)) != 0) {
        switch (choice) {
        case 'H': tool_health(); break;
        case 'M': tool_meter(); break;
        case 'S': tool_survey(); break;
        default: break;
        }
    }
}

void tool_lan_menu(void)
{
    static const char *const labels[] = LANMENU_LABELS;
    static const char keys[] = LANMENU_KEYS;
    char choice;

    while ((choice = menu_pick(LANMENU_TITLE, keys, labels, LANMENU_ROW_FIRST, LANMENU_ROW_STEP)) != 0) {
        switch (choice) {
        case 'H': tool_health(); break;
        case 'P': tool_ping(); break;
        case 'R': tool_ping_gateway(); break;
        case 'C': tool_ping_live(); break;
        case 'T': tool_trace(); break;
        case 'D': tool_dns_check(); break;
        case 'N': tool_lookup(); break;
        case 'B': tool_speed(); break;
        case 'G': tool_get(); break;
        case 'W': tool_time(); break;
        case 'U': tool_certs(); break;
        case 'S': tool_lan(); break;
        case 'O': tool_ports(); break;
        case 'V': tool_server_menu(); break;
        default: break;
        }
    }
}
