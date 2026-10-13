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

/* What Enter does in the list of devices */
#define BT_LIST_BROWSE   0      /* show the details of the device */
#define BT_LIST_FIND     1      /* the signal meter of the device */
#define BT_LIST_INSPECT  2      /* connect to the device and read it */

void tool_bt_menu(void);
void tool_bt_list(int mode);    /* btscan.c: the Low Energy devices around, strongest first */
void tool_bt_classic(void);     /* btscan.c: Classic devices found by inquiry */
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
