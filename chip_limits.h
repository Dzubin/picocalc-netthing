/*
 * chip_limits.h - the sizes that depend on the chip: the Pico 2 W (RP2350, 520 KB of RAM) and the
 * older Pico W (RP2040, 264 KB). Everything that has to be smaller on the Pico W is here, so the
 * two builds are tuned in one place. The Pico W values were found by making the program fit; that
 * build has not been run on real hardware.
 *
 * Author: Thomas Dzubin
 */
#ifndef CHIP_LIMITS_H
#define CHIP_LIMITS_H

#ifdef PICO_RP2040
#define CHIP_MEM_SIZE        (80 * 1024)    /* the lwIP heap, which the TLS code uses too */
#define CHIP_PBUF_POOL_SIZE  18             /* a 16 KB TLS record needs 11 of them at once */
#define CHIP_CA_MAX          20480          /* the most of CA.PEM that is read: about 15 roots */
#define CHIP_BT_DEVICES_MAX  32
#define CHIP_BT_TETHER       0              /* no Bluetooth tethering: the RAM is needed elsewhere */
#define CHIP_SERVER_TOOLS    0              /* no Windows test server tools: the RAM is needed elsewhere */
#define CHIP_CERTS_CHUNK     512            /* the certificate tool copies files in smaller pieces (slower, 6 KB less RAM) */
#else
#define CHIP_MEM_SIZE        (128 * 1024)
#define CHIP_PBUF_POOL_SIZE  24
#define CHIP_CA_MAX          49152
#define CHIP_BT_DEVICES_MAX  64
#define CHIP_BT_TETHER       1
#define CHIP_SERVER_TOOLS    1
#define CHIP_CERTS_CHUNK     2048
#endif

#endif /* CHIP_LIMITS_H */
