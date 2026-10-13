/*
 * wifi_pico_const.h - constants of wifi_pico.c.
 *
 * Author: Thomas Dzubin
 */
#ifndef WIFI_PICO_CONST_H
#define WIFI_PICO_CONST_H

#define WIFI_COUNTRY       CYW43_COUNTRY_CANADA   /* sets the legal 2.4 GHz channels */
#define WIFI_AP_MAX        64                     /* access points kept */
#define WIFI_AP_KEEP_SCANS  3                     /* a scan that continues an earlier one drops access points not heard for this many scans */

/* Authentication methods to try, in order, for a WPA2 network: WPA2 or WPA2/WPA first, then
 * WPA2/WPA3 for an access point that only accepts WPA3 */
#define WIFI_METHODS_WPA2  { CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA3_WPA2_AES_PSK }

#endif /* WIFI_PICO_CONST_H */
