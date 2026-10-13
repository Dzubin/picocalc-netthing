/*
 * btdecode_const.h - constants and name tables of btdecode.c. The maker and
 * service numbers are the ones assigned by the Bluetooth SIG; only the common
 * ones are listed, and anything else is shown as a number.
 *
 * Author: Thomas Dzubin
 */
#ifndef BTDECODE_CONST_H
#define BTDECODE_CONST_H

#define BT_NAME_MAX         20
#define BT_SERVICES_MAX      6
#define BT_ADDR_TEXT_MAX    18

/* advertising data types */
#define BT_AD_FLAGS            0x01
#define BT_AD_UUID16_PARTIAL   0x02
#define BT_AD_UUID16_COMPLETE  0x03
#define BT_AD_NAME_SHORT       0x08
#define BT_AD_NAME_COMPLETE    0x09
#define BT_AD_TX_POWER         0x0A
#define BT_AD_SERVICE_DATA16   0x16
#define BT_AD_APPEARANCE       0x19
#define BT_AD_MANUFACTURER     0xFF

#define BT_COMPANY_APPLE       0x004C
#define BT_APPLE_IBEACON       0x02
#define BT_APPLE_FIND_MY       0x12
#define BT_UUID_EDDYSTONE      0xFEAA
#define BT_UUID_TILE_1         0xFEED
#define BT_UUID_TILE_2         0xFEEC
#define BT_UUID_SMARTTAG       0xFD5A

/* maker number, name (up to 12 characters); ends with number 0xFFFF */
#define BT_COMPANIES { \
    {0x0000, "ERICSSON"}, {0x0001, "NOKIA"}, {0x0002, "INTEL"}, {0x0006, "MICROSOFT"}, {0x000D, "TEXAS INSTR"}, \
    {0x000F, "BROADCOM"}, {0x001D, "QUALCOMM"}, {0x0046, "MEDIATEK"}, {0x004C, "APPLE"}, {0x0059, "NORDIC"}, \
    {0x0075, "SAMSUNG"}, {0x0087, "GARMIN"}, {0x009E, "BOSE"}, {0x00E0, "GOOGLE"}, {0x012D, "SONY"}, \
    {0x0171, "AMAZON"}, {0x02E5, "ESPRESSIF"}, {0x038F, "XIAOMI"}, {0x0499, "RUUVI"}, {0xFFFF, ""} }

/* service number, name (up to 14 characters); ends with number 0 */
#define BT_SERVICES { \
    {0x1800, "GENERIC ACCESS"}, {0x1801, "GENERIC ATTR"}, {0x1802, "IMMEDIATE ALERT"}, {0x1803, "LINK LOSS"}, \
    {0x1804, "TX POWER"}, {0x1805, "CURRENT TIME"}, {0x180A, "DEVICE INFO"}, {0x180D, "HEART RATE"}, \
    {0x180F, "BATTERY"}, {0x1809, "THERMOMETER"}, {0x1812, "HID (KEYS)"}, {0x1816, "CYCLING SPEED"}, \
    {0x1818, "CYCLING POWER"}, {0x181A, "ENVIRONMENT"}, {0x181D, "WEIGHT SCALE"}, {0x1826, "FITNESS MACH."}, \
    {0x183B, "BINARY SENSOR"}, {0xFD5A, "SMARTTAG"}, {0xFD6F, "EXPOSURE NOTE"}, {0xFE2C, "FAST PAIR"}, \
    {0xFEAA, "EDDYSTONE"}, {0xFEED, "TILE"}, {0xFEEC, "TILE"}, {0xFE95, "XIAOMI"}, {0xFE9F, "GOOGLE"}, {0, ""} }

#define BT_KIND_NAMES { "UNKNOWN", "PHONE", "COMPUTER", "WATCH", "AUDIO", "INPUT", "TRACKER", "BEACON", \
                        "HEALTH", "SENSOR", "OTHER" }
#define BT_TRACKER_NAMES { "", "FIND MY TAG", "TILE", "SMARTTAG" }

#endif /* BTDECODE_CONST_H */
