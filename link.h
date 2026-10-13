/*
 * link.h - which way the PicoCalc reaches the LAN: Wi-Fi, a Bluetooth tether to
 * a phone, or IP over USB. Only one is up at a time. The screens ask this module
 * what to show, so a new kind of link is added here and nowhere else.
 *
 * Author: Thomas Dzubin
 */
#ifndef LINK_H
#define LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "link_const.h"
#include "wifi.h"

/* LINK_NONE, LINK_WIFI, LINK_BLUETOOTH or LINK_USB. */
int link_kind(void);

/* The line for the screens, such as "WI-FI HomeNet  -52 DBM" or
 * "NOT CONNECTED"; returns the colour to draw it in. */
uint16_t link_text(char *out, unsigned size);

/* The addresses of the link that is up (Wi-Fi or USB), in the record of a Wi-Fi
 * connection: for USB the name is "USB" and the signal 0. Returns false if no
 * link is up. */
bool link_info_get(wifi_info_t *info);

/* Draw that line on LINK_ROW. */
void link_draw(void);

/* Give up whatever link is up (only one is used at a time, so a new one starts
 * with this). Does nothing if none is up. */
void link_drop(void);

#endif /* LINK_H */
