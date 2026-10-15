/*
 * bt_pico_const.h - constants of bt_pico.c.
 *
 * Author: Thomas Dzubin
 */
#ifndef BT_PICO_CONST_H
#define BT_PICO_CONST_H

#define BT_SCAN_INTERVAL   0x0060   /* in units of 0.625 ms: listen 45 ms in every 60 ms, which leaves */
#define BT_SCAN_WINDOW     0x0048   /* the radio some time for Wi-Fi */
#define BT_KEEP_MS         5000     /* a device heard this recently is never dropped to make room */
#define BT_SMOOTH_DIV      2       /* each advert moves the averaged signal this fraction (1/n) of the way to the new one */
#define BT_SORT_MARGIN     6        /* dB: the sort position follows the signal only after it has moved this far */
#define BT_NAME_PSRM      1        /* page scan repetition mode used to ask for a name */

/* The characteristics read from a device, in this order: device name, manufacturer, model number,
 * firmware revision, battery level */
#define INSPECT_READS      { 0x2A00, 0x2A29, 0x2A24, 0x2A26, 0x2A19 }

#endif /* BT_PICO_CONST_H */
