/*
 * btui.h - the Bluetooth tool screens and what they share. The menu (btmenu.c)
 * switches the radio on when it opens and off when it closes; every other tool
 * assumes the radio is on.
 *
 * Author: Thomas Dzubin
 */
#ifndef BTUI_H
#define BTUI_H

#include <stdbool.h>

#include "bt.h"
#include "modes.h"

/* The Bluetooth mode: switches the radio on, shows the nearby devices and runs the other tools from its
 * keys. Returns the mode to go to next (MODE_TITLE for ESC). */
int  tool_bt_menu(void);
#define BT_EXIT_BACK     0      /* tool_bt_nearby() results besides the keys 'T', 'X' and 'L' */
#define BT_EXIT_WIFI     1
int  tool_bt_nearby(void);      /* btscan.c: the nearby devices with the Classic area; returns why it ended */
void tool_bt_tracker(void);     /* bttrack.c */
void tool_bt_crowding(void);    /* btcrowd.c */

void bt_find_device(const bt_device_t *device);      /* btfind.c: a live signal meter for one device */
void bt_inspect_device(const bt_device_t *device);   /* btinspect.c: connect and read */

/* Show "STARTING BLUETOOTH" and wait for the radio. Returns true when it is ready, false if it did
 * not start or ESC was pressed (the reason is shown). */
bool bt_bring_up(void);

/* A device as a short line for a list: its name, or the maker and the end of its address, or the
 * address. */
void bt_device_label(const bt_device_t *device, char *out, unsigned size);

#endif /* BTUI_H */
