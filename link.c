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
    bool wifi = wifi_connected();
    bool usb = usbnet_state() != USBNET_OFF;
    bool bt = btpan_state() != BTPAN_IDLE;

    if (!wifi && !usb && !bt)
        return;
    /* each link says goodbye properly (address released, access point or phone told); it takes a moment */
    put_row(LINK_DROP_ROW, LINK_DROP_TEXT, COL_YELLOW);
    if (wifi)
        wifi_disconnect();
    if (usb)
        usbnet_stop();
    if (bt) {
        btpan_stop();
        bt_power_off();         /* the Bluetooth screen is not open when another link is made */
    }
    clear_row(LINK_DROP_ROW);
}

void link_draw(void)
{
    char text[LINK_TEXT_MAX + 1];
    uint16_t colour = link_text(text, sizeof text);

    clear_row(LINK_ROW);
    put_at(TCOLS - (int)strlen(LINK_OWNER_TAG), LINK_ROW, LINK_OWNER_TAG, COL_WHITE);
    put_at(0, LINK_ROW, text, colour);
}
