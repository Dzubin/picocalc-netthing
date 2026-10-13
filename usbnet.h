/*
 * usbnet.h - IP over USB: the PicoCalc's USB port shows up on a PC as a network
 * adapter (RNDIS for Windows, ECM for the others). The PC shares its internet connection with that adapter
 * (Internet Connection Sharing on Windows), its DHCP server gives the PicoCalc
 * an address, and the PicoCalc's network tools then run over that link. The
 * firmware implementation is usbnet.c (TinyUSB and lwIP); netthing.c and link.c
 * see only these calls.
 *
 * Author: Thomas Dzubin
 */
#ifndef USBNET_H
#define USBNET_H

#include <stdbool.h>

#include "wifi.h"
#include "usbnet_const.h"

/* Start the adapter (the PC then sees a new network device). Returns 0, or
 * non-zero if the USB part would not start. */
int  usbnet_start(void);

/* Take the adapter away again and go back to the Wi-Fi interface. */
void usbnet_stop(void);

/* USBNET_OFF, USBNET_WAITING, USBNET_LINK or USBNET_UP. */
int  usbnet_state(void);

/* A line of text on what the USB side has seen (events, bus reset, set up by the PC), for the connect screen. */
void usbnet_debug_text(char *out, unsigned size);

/* The addresses of the link, in the same record as a Wi-Fi connection (the name
 * is "USB" and the signal 0). Returns false unless the state is USBNET_UP. */
bool usbnet_info(wifi_info_t *info);

#endif /* USBNET_H */
