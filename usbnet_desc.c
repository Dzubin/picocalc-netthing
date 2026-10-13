/*
 * usbnet_desc.c - the USB descriptors of the network adapter the PicoCalc
 * presents: two configurations, RNDIS (which Windows takes, with its own driver)
 * and CDC ECM (for Linux and macOS).
 *
 * Based on the net_lwip_webserver example of TinyUSB, by Ha Thach (tinyusb.org),
 * which is under the MIT licence:
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Author of the changes: Thomas Dzubin
 */
#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"

#include "usbnet_const.h"

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_INTERFACE,
    STRID_MAC
};

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_NET_NOTIF   0x81
#define EPNUM_NET_OUT     0x02
#define EPNUM_NET_IN      0x82

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USBNET_VID,
    .idProduct          = USBNET_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 2
};

const uint8_t *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&desc_device;
}

#define MAIN_CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_RNDIS_DESC_LEN)
#define ALT_CONFIG_TOTAL_LEN   (TUD_CONFIG_DESC_LEN + TUD_CDC_ECM_DESC_LEN)

static const uint8_t rndis_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, MAIN_CONFIG_TOTAL_LEN, 0, 100),
    TUD_RNDIS_DESCRIPTOR(ITF_NUM_CDC, STRID_INTERFACE, EPNUM_NET_NOTIF, 8, EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE),
};

static const uint8_t ecm_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(2, ITF_NUM_TOTAL, 0, ALT_CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_ECM_DESCRIPTOR(ITF_NUM_CDC, STRID_INTERFACE, STRID_MAC, EPNUM_NET_NOTIF, 64, EPNUM_NET_OUT, EPNUM_NET_IN,
                           CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU),
};

/* Windows takes the first configuration (RNDIS); Linux takes either; macOS needs ECM. */
const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
    return index == 0 ? rndis_configuration : (index == 1 ? ecm_configuration : NULL);
}

/* The strings: names, the serial number (from the chip's unique id) and the MAC address as text. */
static const char *const string_desc_arr[] = {
    [STRID_LANGID]       = (const char[]){ 0x09, 0x04 },
    [STRID_MANUFACTURER] = USBNET_MANUFACTURER,
    [STRID_PRODUCT]      = USBNET_PRODUCT,
    [STRID_SERIAL]       = NULL,
    [STRID_INTERFACE]    = USBNET_INTERFACE
};

extern uint8_t tud_network_mac_address[6];

static uint16_t desc_str[32 + 1];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    unsigned count = 0;
    char text[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

    (void)langid;
    switch (index) {
    case STRID_LANGID:
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        count = 1;
        break;
    case STRID_SERIAL:
        pico_get_unique_board_id_string(text, sizeof text);
        for (count = 0; text[count] != '\0' && count < 32; count++)
            desc_str[1 + count] = (uint16_t)(uint8_t)text[count];
        break;
    case STRID_MAC: {
        static const char hex[] = "0123456789ABCDEF";

        for (unsigned i = 0; i < 6; i++) {
            desc_str[1 + count++] = (uint16_t)(uint8_t)hex[(tud_network_mac_address[i] >> 4) & 0xF];
            desc_str[1 + count++] = (uint16_t)(uint8_t)hex[tud_network_mac_address[i] & 0xF];
        }
        break;
    }
    default: {
        const char *s;

        if (index >= sizeof string_desc_arr / sizeof string_desc_arr[0])
            return NULL;
        s = string_desc_arr[index];
        if (s == NULL)
            return NULL;
        for (count = 0; s[count] != '\0' && count < 32; count++)
            desc_str[1 + count] = (uint16_t)(uint8_t)s[count];
        break;
    }
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
    return desc_str;
}
