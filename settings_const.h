/*
 * settings_const.h - constants of settings.c, the last-used hosts and options
 * of the network tools, kept as plain text on the SD card.
 *
 * Author: Thomas Dzubin
 */
#ifndef SETTINGS_CONST_H
#define SETTINGS_CONST_H

#define SETTINGS_HOST_MAX    63         /* longest host name or address */
#define SETTINGS_PORT_MAX    5
#define SETTINGS_URL_MAX     199        /* a web address typed in the get tool */
#define SETTINGS_OFFSET_MAX  8
#define SETTINGS_PORTS_MAX   39         /* the port list of the port check */
#define SETTINGS_ADDR_MAX    15         /* an IPv4 address in text */
#define SETTINGS_FILE_MAX    2048        /* the whole file, in bytes */

/* the keys in the file, one per line as key=value */
#define SETTINGS_KEY_PING     "ping"
#define SETTINGS_KEY_LOOKUP   "lookup"
#define SETTINGS_KEY_SERVER   "server"
#define SETTINGS_KEY_PORT     "port"
#define SETTINGS_KEY_NTP      "ntp"
#define SETTINGS_KEY_OFFSET   "offset"
#define SETTINGS_KEY_URL      "url"
#define SETTINGS_KEY_PORTS    "ports"
#define SETTINGS_KEY_SPEED_URL "speedurl"
#define SETTINGS_KEY_LAN_FIRST "lanfirst"
#define SETTINGS_KEY_LAN_LAST  "lanlast"

/* the values before anything has been saved */
#define SETTINGS_DEFAULT_PING    ""     /* empty: the tool offers the gateway address */
#define SETTINGS_DEFAULT_LOOKUP  "example.com"
#define SETTINGS_DEFAULT_SERVER  ""
#define SETTINGS_DEFAULT_PORT    "7000"
#define SETTINGS_DEFAULT_NTP     "pool.ntp.org"
#define SETTINGS_DEFAULT_OFFSET  "0"
#define SETTINGS_DEFAULT_URL     "http://example.com/"
#define SETTINGS_DEFAULT_PORTS   "80,443"
#define SETTINGS_DEFAULT_SPEED_URL "http://speedtest.tele2.net/10MB.zip"

#endif /* SETTINGS_CONST_H */
