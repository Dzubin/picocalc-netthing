/*
 * link_const.h - constants of link.c, the line that says how the PicoCalc
 * reaches the LAN.
 *
 * Author: Thomas Dzubin
 */
#ifndef LINK_CONST_H
#define LINK_CONST_H

#define LINK_ROW            0       /* the top row of every screen */
#define LINK_TEXT_MAX       40

#define LINK_NONE           0
#define LINK_WIFI           1
#define LINK_BLUETOOTH      2       /* tethered to a phone (not built yet) */
#define LINK_USB            3       /* IP over USB (not built yet) */

#define LINK_TEXT_NONE      "NOT CONNECTED"
#define LINK_NEEDED         "NOT CONNECTED: CONNECT FIRST"   /* L with no link up */
#define LINK_NAME_WIDTH     22      /* longest network name shown */

#endif /* LINK_CONST_H */
