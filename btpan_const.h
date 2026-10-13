/*
 * btpan_const.h - constants of btpan.c, Bluetooth tethering.
 *
 * Author: Thomas Dzubin
 */
#ifndef BTPAN_CONST_H
#define BTPAN_CONST_H

/* The states btpan_state() gives. */
#define BTPAN_IDLE         0        /* nothing going on */
#define BTPAN_QUERY        1        /* asking the phone for its network service */
#define BTPAN_CONNECT      2        /* opening the BNEP link (and pairing, if asked) */
#define BTPAN_DHCP         3        /* linked; waiting for an address */
#define BTPAN_UP           4        /* has an IPv4 address */
#define BTPAN_FAILED       5        /* ended; btpan_message() says why */

#define BTPAN_BNEP_MTU     1691     /* the smallest frame size BNEP allows */
#define BTPAN_QUERY_MS     15000    /* the phone did not answer the service query */
#define BTPAN_CONNECT_MS   60000    /* the link did not open (time to answer a pairing question) */
#define BTPAN_DHCP_MS      25000    /* no address came */
#define BTPAN_NAME_MAX     24
#define BTPAN_RELEASE_MS   200      /* time for the DHCP release to reach the phone before the link closes */
#define BTPAN_CLOSE_MS     1500     /* the longest wait for the link to close */
#define BTPAN_ATTR_MAX     256      /* the longest SDP attribute that is read */

#define BTPAN_MSG_QUERY    "ASKING THE PHONE FOR ITS NETWORK"
#define BTPAN_MSG_CONNECT  "OPENING THE LINK (PAIR ON THE PHONE)"
#define BTPAN_MSG_DHCP     "WAITING FOR AN ADDRESS (DHCP)"
#define BTPAN_MSG_UP       "CONNECTED"
#define BTPAN_MSG_NO_NAP   "NO NETWORK OFFERED: IS TETHERING ON?"
#define BTPAN_MSG_NO_ANSWER "THE PHONE DID NOT ANSWER"
#define BTPAN_MSG_NO_LINK  "THE LINK WOULD NOT OPEN"
#define BTPAN_MSG_NO_DHCP  "NO ADDRESS WAS GIVEN"
#define BTPAN_MSG_CLOSED   "THE PHONE CLOSED THE LINK"
#define BTPAN_MSG_NO_START "BLUETOOTH WOULD NOT START THE QUERY"

#endif /* BTPAN_CONST_H */
