/*
 * btmenu.c - the Bluetooth screen. The radio is switched on when it opens (the
 * firmware of the Bluetooth part of the chip is loaded, which takes a second or
 * two) and off again when it closes, so Wi-Fi has the chip to itself the rest
 * of the time. It opens straight on the nearby devices list (btscan.c); the other
 * Bluetooth tools run from keys on that list.
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
#include "netthing_const.h"

bool bt_bring_up(void)
{
    uint32_t started = plat_now_ms();

    screen_clear();
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

int tool_bt_menu(void)
{
    int next;

    if (!bt_bring_up())
        return MODE_WIFI;
    for (;;) {
        int result = tool_bt_nearby();

        if (result == 'T') {
            tool_bt_tracker();
        } else if (result == 'X') {
            tool_bt_crowding();
        } else if (result == 'P') {
            if (bt_pan_connect_screen(tool_bt_pan_target())) {
                next = MODE_LAN;
                break;
            }
        } else if (result == 'L') {
            next = MODE_LAN;
            break;
        } else {
            next = result == BT_EXIT_WIFI ? MODE_WIFI : MODE_TITLE;
            break;
        }
    }
    bt_power_off();
    return next;
}
