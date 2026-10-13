/*
 * tether_const.h - constants of tether.c, the Bluetooth tether slots at the
 * bottom of the Wi-Fi scan screen.
 *
 * Author: Thomas Dzubin
 */
#ifndef TETHER_CONST_H
#define TETHER_CONST_H

#define TETHER_SLOTS           4        /* devices shown at the bottom of the scan screen */
#define TETHER_ROW_HEADING     23       /* "BLUETOOTH TETHER" and what the search is doing */
#define TETHER_ROW_FIRST       24       /* the four slots, rows 24 to 27 */
#define TETHER_COL_MARKER      0        /* the slot number, 1 to 4 */
#define TETHER_COL_NAME        2
#define TETHER_NAME_W          17
#define TETHER_COL_BARS        20
#define TETHER_COL_DBM         26

#define TETHER_REFRESH_MS      500      /* how often the table of devices is looked at */
#define TETHER_START_TIMEOUT_MS 15000   /* the radio's firmware did not load */
#define TETHER_INQUIRY_SECONDS 8        /* one Classic inquiry; the next follows at once */
#define TETHER_HIDE_MS         60000    /* a device not heard for this long leaves the slots */

/* The major device class sits in bits 8 to 12 of the class of device. Only
 * phones are offered: they are what shares a connection. */
#define TETHER_CLASS_MAJOR(cod)  (((cod) >> 8) & 0x1Fu)
#define TETHER_MAJOR_PHONE       2

#define TETHER_HEADING         "BLUETOOTH TETHER  1-4"
#define TETHER_SEARCHING       "SEARCHING"
#define TETHER_STARTING        "STARTING"
#define TETHER_PAUSED          "PAUSED"
#define TETHER_EVERY           5        /* every fifth turn is the Bluetooth search; the other four are Wi-Fi scans */
#define TETHER_NONE_FOUND      "NO PHONES FOUND YET"
#define TETHER_UNAVAILABLE     "BLUETOOTH IS NOT AVAILABLE"
#define TETHER_NOT_READY       "TETHERING IS NOT READY YET"

#endif /* TETHER_CONST_H */
