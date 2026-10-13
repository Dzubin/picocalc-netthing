/*
 * btfind.c - find one Bluetooth device by its signal: the same wide meter as
 * the Wi-Fi signal meter, fed by the adverts of the chosen device, with
 * "WARMER" or "COLDER" as the signal rises or falls while you walk. A reading
 * is added only when the device was heard again, since adverts come at their own pace.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "gfx.h"
#include "meter.h"
#include "ui.h"
#include "btui.h"
#include "netthing_const.h"

void bt_find_device(const bt_device_t *device)
{
    char label[BTL_NAME_W + 8];
    uint8_t address[6];
    int recent[BTF_TREND * 2];          /* the last readings, newest at the end */
    unsigned recent_count = 0;
    uint32_t seen_before = 0;
    uint32_t last = 0;

    memcpy(address, device->address, 6);
    bt_device_label(device, label, sizeof label);
    seen_before = device->sightings;
    meter_frame("FIND A DEVICE", "C CLEAR BEST AND WORST  ESC BACK");
    (void)bt_le_scan_start();
    drain_keys();
    for (;;) {
        uint8_t key = poll_key();
        uint32_t now = plat_now_ms();

        if (key == KEY_ESC)
            return;
        if (key == 'c' || key == 'C')
            meter_clear_best();
        if (now - last >= METER_MS) {
            bt_device_t d;

            last = now;
            if (!bt_device_find(address, &d)) {
                meter_message("NOT HEARD YET", COL_YELLOW);
            } else if (now - d.last_ms > BTF_LOST_MS) {
                char text[TOOL_LINE_MAX];

                snprintf(text, sizeof text, "NOT HEARD FOR %u S", (unsigned)((now - d.last_ms) / 1000u));
                meter_message(text, COL_YELLOW);
                meter_hint("", COL_WHITE);
            } else {
                bool fresh = d.sightings != seen_before;

                seen_before = d.sightings;
                meter_show(d.rssi, label, fresh);
                if (fresh) {
                    unsigned i;

                    if (recent_count == BTF_TREND * 2) {
                        for (i = 1; i < recent_count; i++)
                            recent[i - 1] = recent[i];
                        recent_count--;
                    }
                    recent[recent_count++] = d.rssi;
                }
                if (recent_count == BTF_TREND * 2) {
                    int before = 0, after = 0;
                    unsigned i;

                    for (i = 0; i < BTF_TREND; i++) {
                        before += recent[i];
                        after += recent[BTF_TREND + i];
                    }
                    if (after - before > BTF_STEP_DB * (int)BTF_TREND)
                        meter_hint("WARMER", COL_GREEN);
                    else if (before - after > BTF_STEP_DB * (int)BTF_TREND)
                        meter_hint("COLDER", COL_YELLOW);
                    else
                        meter_hint("ABOUT THE SAME", COL_WHITE);
                }
            }
        }
        plat_sleep_ms(POLL_MS);
    }
}
