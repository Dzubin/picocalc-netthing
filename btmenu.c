/*
 * btmenu.c - the Bluetooth tools menu. The radio is switched on when the menu
 * opens (the firmware of the Bluetooth part of the chip is loaded, which takes a
 * second or two) and off again when it closes, so Wi-Fi has the chip to itself
 * the rest of the time.
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

bool bt_bring_up(void)
{
    uint32_t started = plat_now_ms();

    lcd_clear_screen();
    put_row(2, "BLUETOOTH", COL_CYAN);
    put_row(BTMENU_ROW_STATUS, "STARTING THE RADIO...", COL_YELLOW);
    bt_power_on();
    drain_keys();
    while (bt_power_state() == BT_POWER_STARTING) {
        if (plat_now_ms() - started > BT_START_TIMEOUT_MS) {
            bt_power_off();
            clear_row(BTMENU_ROW_STATUS);
            put_row(BTMENU_ROW_STATUS, "THE BLUETOOTH RADIO DID NOT START", COL_YELLOW);
            pause_ms(NOTICE_MS * 2);
            return false;
        }
        if (poll_key() == KEY_ESC) {
            bt_power_off();
            return false;
        }
        plat_sleep_ms(POLL_MS);
    }
    return bt_power_state() == BT_POWER_READY;
}

void tool_bt_menu(void)
{
    static const char *const labels[] = BTMENU_LABELS;
    static const char keys[] = BTMENU_KEYS;

    if (!bt_bring_up())
        return;
    for (;;) {
        char line[TCOLS + 1];
        unsigned i;
        uint8_t key;
        char up;
        const char *at;

        lcd_clear_screen();
        put_row(2, "BLUETOOTH TOOLS", COL_CYAN);
        for (i = 0; labels[i] != NULL; i++) {
            snprintf(line, sizeof line, "%c  %s", keys[i], labels[i]);
            put_at(MENU_COL, BTMENU_ROW_FIRST + (int)i * BTMENU_ROW_STEP, line, COL_WHITE);
        }
        put_row(TOOL_ROW_HELP, MENU_HELP, COL_WHITE);
        drain_keys();
        key = wait_any_key();
        if (key == KEY_ESC)
            break;
        up = (key >= 'a' && key <= 'z') ? (char)(key - 'a' + 'A') : (char)key;
        at = up ? strchr(keys, up) : NULL;
        if (at == NULL)
            continue;
        switch (*at) {
        case 'S': tool_bt_list(BT_LIST_BROWSE); break;
        case 'F': tool_bt_list(BT_LIST_FIND); break;
        case 'I': tool_bt_list(BT_LIST_INSPECT); break;
        case 'C': tool_bt_classic(); break;
        case 'T': tool_bt_tracker(); break;
        case 'X': tool_bt_crowding(); break;
        default: break;
        }
    }
    bt_power_off();
}
