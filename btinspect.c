/*
 * btinspect.c - connect to one Low Energy device and read what it says about
 * itself: its name, maker, model and firmware (the Device Information
 * service), its battery level, and the list of its services. The connection
 * is dropped as soon as the reading is done. Many devices accept only one
 * connection, or only their owner's phone, and then this says why it failed.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "gfx.h"
#include "ui.h"
#include "btui.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static void show_field(const char *label, const char *value)
{
    char line[TOOL_LINE_MAX];

    snprintf(line, sizeof line, "%-9s%s", label, value[0] ? value : "NOT GIVEN");
    tools_out_line(line, value[0] ? COL_WHITE : COL_CYAN);
}

void bt_inspect_device(const bt_device_t *device)
{
    char label[BTL_NAME_W + 8], line[TOOL_LINE_MAX];
    bt_inspect_t r;
    uint32_t started = plat_now_ms();
    int state;
    unsigned i;

    bt_device_label(device, label, sizeof label);
    tools_screen("READ A DEVICE", label);
    tools_out_line("CONNECTING...", COL_YELLOW);
    if (bt_inspect_start(device->address, device->address_type) != 0) {
        tools_out_line("COULD NOT START", COL_YELLOW);
        tools_finish();
        return;
    }
    drain_keys();
    for (;;) {
        state = bt_inspect_poll(&r);
        if (state == BT_INSPECT_DONE || state == BT_INSPECT_FAILED)
            break;
        if (plat_now_ms() - started > BTI_TIMEOUT_MS) {
            bt_inspect_stop();
            tools_out_line("IT DID NOT ANSWER IN TIME", COL_YELLOW);
            tools_out_line("(OUT OF RANGE, OR BUSY WITH SOMEONE ELSE)", COL_WHITE);
            tools_finish();
            return;
        }
        if (poll_key() == KEY_ESC) {
            bt_inspect_stop();
            return;
        }
        snprintf(line, sizeof line, "%s %u S", state == BT_INSPECT_CONNECTING ? "CONNECTING..." : "READING...",
                 (unsigned)((plat_now_ms() - started) / 1000u));
        clear_row(TOOL_ROW_LAST);
        put_at(TOOL_COL, TOOL_ROW_LAST, line, COL_WHITE);
        plat_sleep_ms(POLL_MS);
    }
    bt_inspect_stop();
    tools_screen("READ A DEVICE", label);
    if (state == BT_INSPECT_FAILED) {
        tools_out_line(r.error ? r.error : "IT FAILED", COL_YELLOW);
        tools_out_row++;
        tools_out_line("MANY DEVICES TAKE ONE CONNECTION AT", COL_WHITE);
        tools_out_line("A TIME, OR ONLY THEIR OWN PHONE.", COL_WHITE);
        tools_out_line("TURN OFF THE PHONE'S BLUETOOTH AND", COL_WHITE);
        tools_out_line("TRY AGAIN.", COL_WHITE);
        tools_finish();
        return;
    }
    show_field("NAME", r.name);
    show_field("MAKER", r.manufacturer);
    show_field("MODEL", r.model);
    show_field("FIRMWARE", r.firmware);
    if (r.battery >= 0) {
        uint16_t colour = r.battery > BTI_BATTERY_GOOD ? COL_GREEN : (r.battery > BTI_BATTERY_LOW ? COL_YELLOW : COL_RED);

        snprintf(line, sizeof line, "%-9s%d%%", "BATTERY", r.battery);
        tools_out_line(line, colour);
        gfx_hbar(TOOL_COL * 8, tools_out_row * 10, 8, BTI_BATTERY_W, (unsigned)r.battery, 100, colour);
        gfx_frame(COL_WHITE, TOOL_COL * 8 - 1, tools_out_row * 10 - 1, BTI_BATTERY_W + 2, 10);
        tools_out_row++;
    } else {
        show_field("BATTERY", "");
    }
    tools_out_row++;
    snprintf(line, sizeof line, "SERVICES (%u)", r.service_count);
    tools_out_line(line, COL_CYAN);
    for (i = 0; i < r.service_count; i++) {
        const char *name = btd_service_name(r.services[i]);

        if (name[0])
            snprintf(line, sizeof line, "  %04X  %s", (unsigned)r.services[i], name);
        else
            snprintf(line, sizeof line, "  %04X", (unsigned)r.services[i]);
        tools_out_line(line, COL_WHITE);
    }
    tools_finish();
}
