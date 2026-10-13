/*
 * usbnet_const.h - constants of usbnet.c and usbnet_desc.c, the PicoCalc as a USB
 * network adapter.
 *
 * Author: Thomas Dzubin
 */
#ifndef USBNET_CONST_H
#define USBNET_CONST_H

/* The USB ids are TinyUSB's example ones (0xCafe), good for a hobby device. */
#define USBNET_VID           0xCafe
#define USBNET_PID           0x4020
#define USBNET_MANUFACTURER  "Thomas Dzubin"
#define USBNET_PRODUCT       "PicoCalc NetThing"
#define USBNET_INTERFACE     "PicoCalc NetThing network"

/* The states usbnet_state() gives. */
#define USBNET_OFF           0      /* not started */
#define USBNET_WAITING       1      /* started; the PC has not set the adapter up */
#define USBNET_LINK          2      /* the PC has it; waiting for an address from its DHCP server */
#define USBNET_UP            3      /* has an IPv4 address: usable */

#define USBNET_TICK_MS       500    /* how often the state is looked at */
#define USBNET_DHCP_RETRY_MS 8000   /* a DHCP search that has found nobody is started again after this */
#define USBNET_MTU           1500

#endif /* USBNET_CONST_H */
