/*
 * btpanui.c - the screen that connects to a phone's Bluetooth tethering and waits
 * for it: what is happening, the pairing question, and the result.
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
#include "btpan.h"
#include "tools.h"
#include "netthing_const.h"

bool bt_pan_connect_screen(const bt_device_t *device)
{
    char label[BT_TEXT_MAX + 1], line[TCOLS + 1];
    const char *shown = NULL;
    bool asked = false;

    bt_device_label(device, label, sizeof label);
    if (!leave_current_link())
        return false;
    screen_clear();
    put_row(BTPANUI_ROW_HEADING, "BLUETOOTH TETHERING", COL_CYAN);
    put_row(BTPANUI_ROW_NAME, label, COL_WHITE);
    put_row(BTPANUI_ROW_NOTE, BTPANUI_NOTE, COL_WHITE);
    put_row(TOOL_ROW_HELP, "ESC=CANCEL", COL_WHITE);
    if (btpan_start(device->address, label) != 0)
        btpan_stop();
    drain_keys();
    for (;;) {
        uint32_t number;
        int state;
        uint8_t key = poll_key();

        if (key == KEY_ESC) {
            btpan_stop();
            return false;
        }
        if (btpan_pairing_request(&number)) {
            if (!asked) {
                asked = true;
                clear_row(BTPANUI_ROW_PAIR);
                clear_row(BTPANUI_ROW_PAIR + 1);
                if (number != 0)
                    snprintf(line, sizeof line, "PAIRING CODE  %06lu", (unsigned long)number);
                else
                    snprintf(line, sizeof line, "PAIRING WITH THE PHONE");
                put_row(BTPANUI_ROW_PAIR, line, COL_YELLOW);
                put_row(BTPANUI_ROW_PAIR + 1, "SAME ON THE PHONE?  Y=YES  N=NO", COL_YELLOW);
            }
            if (key == 'y' || key == 'Y' || key == 'n' || key == 'N') {
                btpan_pairing_answer(key == 'y' || key == 'Y');
                asked = false;
                clear_row(BTPANUI_ROW_PAIR);
                clear_row(BTPANUI_ROW_PAIR + 1);
            }
        }
        state = btpan_state();
        if (btpan_message() != shown) {
            shown = btpan_message();
            clear_row(BTPANUI_ROW_STATUS);
            put_row(BTPANUI_ROW_STATUS, shown, state == BTPAN_FAILED ? COL_YELLOW : COL_CYAN);
        }
        if (state == BTPAN_UP) {
            screen_clear();
            return true;
        }
        if (state == BTPAN_FAILED) {
            put_row(TOOL_ROW_HELP, "PRESS ANY KEY", COL_WHITE);
            (void)wait_any_key();
            btpan_stop();
            return false;
        }
        plat_sleep_ms(POLL_MS);
    }
}
