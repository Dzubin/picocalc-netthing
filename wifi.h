/*
 * wifi.h - the Wi-Fi radio as netthing.c sees it: start it, scan for access
 * points, read the results. The firmware implementation is wifi_pico.c (the
 * CYW43 chip of the Pico 2 W through the Pico SDK); netthing.c includes no
 * radio or SDK header, so every call it makes is listed here.
 *
 * Author: Thomas Dzubin
 */
#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>
#include <stdint.h>

#define WIFI_SSID_MAX   32      /* longest network name, in characters */
#define WIFI_IP6_MAX    40      /* an IPv6 address in text (39 characters) and the end */

/* Bits of wifi_ap_t.security. The radio driver reports only these: whether the
 * access point advertises WPA, WPA2 (an RSN element, which WPA3-only and
 * mixed WPA2/WPA3 access points also carry) and the privacy flag (WEP when
 * neither of the others is set). Nothing set means an open network. */
#define WIFI_SEC_PRIVACY  0x01
#define WIFI_SEC_WPA      0x02
#define WIFI_SEC_WPA2     0x04

typedef struct {
    char     ssid[WIFI_SSID_MAX + 1];   /* empty for a hidden network */
    uint8_t  bssid[6];                  /* the access point's MAC address */
    int16_t  rssi;                      /* signal strength in dBm (negative) */
    uint8_t  channel;                   /* 2.4 GHz channel number, 1 to 14 */
    uint8_t  security;                  /* WIFI_SEC_ bits */
} wifi_ap_t;

/* Start the radio (this loads its firmware and takes a second or two).
 * Returns 0, or a non-zero code if the chip does not answer. */
int  wifi_start(void);

/* Begin a scan; the list of access points is emptied first. Returns 0, or
 * non-zero if a scan cannot start. The scan runs by itself (a few seconds). */
int  wifi_scan_start(void);

/* Begin a scan that continues the earlier ones: the list is kept, what is heard
 * again is updated, new access points are added, and those not heard for the
 * last few scans are dropped. For scanning over and over while walking about. */
int  wifi_scan_more(void);

/* How many access points the running scan has heard so far (the list that
 * wifi_ap_count() gives only changes between scans). */
unsigned wifi_scan_found(void);

/* True while the scan is still running (a few seconds; it ends by itself). */
bool wifi_scan_busy(void);

/* The access points found, strongest signal first. The list changes only when a
 * scan has ended (the entries do not shift while a scan runs). One entry per
 * access point (a network on several access points shows several times).
 * wifi_ap_get() returns false for an index out of range. */
unsigned wifi_ap_count(void);
bool     wifi_ap_get(unsigned index, wifi_ap_t *out);

/* Order the list by `rank` (the higher first) before the signal. Without it the list is by signal alone. */
void wifi_ap_set_rank(int (*rank)(const wifi_ap_t *ap));

/* The ranks have changed (a network was saved or forgotten): sort the list again when no scan is running. */
void wifi_ap_resort(void);

/* ---- joining a network ---------------------------------------------- */

/* Where a connection stands; wifi_link_state() polls it. */
#define WIFI_LINK_IDLE         0    /* nothing started, or disconnected */
#define WIFI_LINK_JOINING      1    /* working on it (joining, or waiting for an address) */
#define WIFI_LINK_UP           2    /* joined and has an IP address */
#define WIFI_LINK_NO_NETWORK   3    /* the network was not found */
#define WIFI_LINK_BAD_PASSWORD  4   /* the access point refused the password */
#define WIFI_LINK_FAILED       5    /* any other failure */

/* How many ways of authenticating are worth trying for an access point with
 * these WIFI_SEC_ bits (0 is also what a hand-typed network gets, with a
 * password). A WEP network cannot be joined, so it gets 0 tries. */
unsigned wifi_attempts(uint8_t security, bool has_password);

/* Start joining with attempt number `attempt` (0 up to wifi_attempts() - 1).
 * Returns at once; poll wifi_link_state(). Returns 0, or non-zero if it could
 * not be started. An empty password means an open network. */
int  wifi_connect_start(const char *ssid, const char *password, uint8_t security,
                        bool has_password, unsigned attempt);
int  wifi_link_state(void);
void wifi_disconnect(void);

/* True when the radio is up and joined to a network with an address. */
bool wifi_connected(void);

typedef struct {
    char    ssid[WIFI_SSID_MAX + 1];    /* the network joined */
    int16_t rssi;                       /* its signal now, in dBm */
    char    ip[16];                     /* "192.168.1.23" */
    char    netmask[16];
    char    gateway[16];
    char    dns[WIFI_IP6_MAX];          /* the first DNS server (IPv4 or IPv6) */
    char    ip6[WIFI_IP6_MAX];          /* the global IPv6 address, "" until the router has given one */
    char    ip6_local[WIFI_IP6_MAX];    /* the link-local IPv6 address (fe80::...) */
    uint8_t mac[6];                     /* this PicoCalc's Wi-Fi address */
    uint8_t bssid[6];                   /* the access point joined (all zero if unknown) */
} wifi_info_t;

/* Fill in the details of the current connection. Returns false if not joined. */
bool wifi_info_get(wifi_info_t *info);

#endif /* WIFI_H */
