/*
 * link.c - see link.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"

#include "wifi.h"
#include "ui.h"
#include "netthing_const.h"
#include "link.h"

int link_kind(void)
{
    return wifi_connected() ? LINK_WIFI : LINK_NONE;
}

uint16_t link_text(char *out, unsigned size)
{
    wifi_info_t info;

    if (link_kind() == LINK_WIFI && wifi_info_get(&info)) {
        snprintf(out, size, "WI-FI %.*s  %d DBM", LINK_NAME_WIDTH, info.ssid, (int)info.rssi);
        return COL_GREEN;
    }
    snprintf(out, size, "%s", LINK_TEXT_NONE);
    return COL_WHITE;
}

void link_drop(void)
{
    switch (link_kind()) {
    case LINK_WIFI:
        wifi_disconnect();
        break;
    default:
        break;                  /* Bluetooth and USB links are not built yet */
    }
}

void link_draw(void)
{
    char text[LINK_TEXT_MAX + 1];
    uint16_t colour = link_text(text, sizeof text);

    clear_row(LINK_ROW);
    put_at(0, LINK_ROW, text, colour);
}
