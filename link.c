/*
 * link.c - see link.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"

#include "wifi.h"
#include "usbnet.h"
#include "btpan.h"
#include "bt.h"
#include "ui.h"
#include "netthing_const.h"
#include "link.h"

int link_kind(void)
{
    if (wifi_connected())
        return LINK_WIFI;
    if (usbnet_state() == USBNET_UP)
        return LINK_USB;
    return btpan_state() == BTPAN_UP ? LINK_BLUETOOTH : LINK_NONE;
}

uint16_t link_text(char *out, unsigned size)
{
    wifi_info_t info;

    if (link_kind() == LINK_WIFI && wifi_info_get(&info)) {
        snprintf(out, size, "WI-FI %.*s  %d DBM", LINK_NAME_WIDTH, info.ssid, (int)info.rssi);
        return COL_GREEN;
    }
    if (link_kind() == LINK_USB && usbnet_info(&info)) {
        snprintf(out, size, "USB  %s", info.ip);
        return COL_GREEN;
    }
    if (link_kind() == LINK_BLUETOOTH && btpan_info(&info)) {
        snprintf(out, size, "BT %.14s  %s", info.ssid, info.ip);
        return COL_GREEN;
    }
    snprintf(out, size, "%s", LINK_TEXT_NONE);
    return COL_WHITE;
}

bool link_info_get(wifi_info_t *info)
{
    switch (link_kind()) {
    case LINK_WIFI:
        return wifi_info_get(info);
    case LINK_USB:
        return usbnet_info(info);
    case LINK_BLUETOOTH:
        return btpan_info(info);
    default:
        memset(info, 0, sizeof *info);
        return false;
    }
}

void link_drop(void)
{
    switch (link_kind()) {
    case LINK_WIFI:
        wifi_disconnect();
        break;
    case LINK_USB:
        usbnet_stop();
        break;
    case LINK_BLUETOOTH:
        btpan_stop();
        bt_power_off();         /* the Bluetooth screen is not open when another link is made */
        break;
    default:
        break;
    }
}

void link_draw(void)
{
    char text[LINK_TEXT_MAX + 1];
    uint16_t colour = link_text(text, sizeof text);

    clear_row(LINK_ROW);
    put_at(0, LINK_ROW, text, colour);
}
