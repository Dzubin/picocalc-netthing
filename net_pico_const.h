/*
 * net_pico_const.h - constants of net_pico.c.
 *
 * Author: Thomas Dzubin
 */
#ifndef NET_PICO_CONST_H
#define NET_PICO_CONST_H

#define PING_ID            0x4E54       /* identifies our echo requests ("NT") */
#define PING_DATA_SIZE     32           /* bytes after the ICMP header, like Windows */
#define PING_PATTERN       'a'          /* first byte of the data, then counting up */

#define NTP_PORT           123
#define NTP_PACKET_SIZE    48
#define NTP_FIRST_BYTE     0x23         /* no leap warning, version 4, client mode */
#define NTP_TRANSMIT_AT    40           /* where the server's time (seconds since 1900) is */
#define NTP_UNIX_OFFSET    2208988800u  /* seconds from 1900 to 1970 */

#endif /* NET_PICO_CONST_H */
