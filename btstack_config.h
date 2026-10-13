/*
 * btstack_config.h - what the Bluetooth stack (BTstack, from the Pico SDK) is built
 * with: scanning and connecting as a BLE central, Classic inquiry, small tables.
 * The numbers are the smallest that BTstack's sources accept; this program never
 * pairs, so the link key and device database tables stay tiny.
 *
 * Author: Thomas Dzubin
 */
#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP

#ifdef ENABLE_BLE
#define ENABLE_GATT_CLIENT_PAIRING
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
#define ENABLE_LE_SECURE_CONNECTIONS
#endif

#ifdef ENABLE_CLASSIC
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#endif

#if defined(ENABLE_CLASSIC) && defined(ENABLE_BLE)
#define ENABLE_CROSS_TRANSPORT_KEY_DERIVATION
#endif

/* buffers and table sizes */
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4
#define HCI_ACL_PAYLOAD_SIZE (255 + 4)
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define MAX_NR_AVDTP_CONNECTIONS 1
#define MAX_NR_AVDTP_STREAM_ENDPOINTS 1
#define MAX_NR_AVRCP_CONNECTIONS 1
#define MAX_NR_BNEP_CHANNELS 1
#define MAX_NR_BNEP_SERVICES 1
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 2
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 2
#define MAX_NR_HFP_CONNECTIONS 1
#define MAX_NR_HIDS_HOSTS 1
#define MAX_NR_HID_HOST_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 3
#define MAX_NR_L2CAP_SERVICES 2
#define MAX_NR_LE_DEVICE_DB_ENTRIES 4
#define MAX_NR_RFCOMM_CHANNELS 1
#define MAX_NR_RFCOMM_MULTIPLEXERS 1
#define MAX_NR_RFCOMM_SERVICES 1
#define MAX_NR_SERVICE_RECORD_ITEMS 2
#define MAX_NR_SM_LOOKUP_ENTRIES 2
#define MAX_NR_WHITELIST_ENTRIES 2

/* Link keys and device database in flash (the last sectors of the Pico's flash) */
#define NVM_NUM_DEVICE_DB_ENTRIES 4
#define NVM_NUM_LINK_KEYS 4

/* there is no malloc for BTstack, so the attribute database has a fixed size */
#define MAX_ATT_DB_SIZE 128

#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT
#define HCI_RESET_RESEND_TIMEOUT_MS 200

#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_SOFTWARE_AES128

#define HAVE_BTSTACK_STDIN

#endif /* BTSTACK_CONFIG_H */
