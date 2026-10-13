/*
 *  PicoCalc NetThing
 *  ===========================================
 *
 *  Wireless networking for the ClockworkPi PicoCalc with a Pico 2 W. This step
 *  scans for access points, joins one (typing the password, or using the one
 *  saved on the SD card in scrambled form), and shows the connection. Later
 *  steps fetch web pages.
 *
 *  Hardware used (all through the vendored picocalc-text-starter drivers by
 *  Blair Leduc, in picocalc-text-starter-main/, plus the radio in wifi.h):
 *      - lcd_init(), lcd_putstr(), lcd_solid_rectangle()   (ST7365P LCD)
 *      - sb_init(), sb_read_keyboard()                     (south bridge)
 *      - wifi.h (the CYW43 radio), storage.h (the SD card), secret.h
 *
 *  The keyboard is read straight from the south bridge's FIFO: each call to
 *  sb_read_keyboard() returns one 16-bit event, (state << 8) | code, or 0
 *  when the FIFO is empty. We do not call picocalc_init(), so nothing else
 *  drains that FIFO.
 *
 *  Controls
 *      any key ........ splash: open the scan screen
 *      ESC or Q ....... splash: leave for the PicoCalc UF2 Loader menu
 *      UP, DOWN ....... scan list: choose an access point
 *      ENTER .......... scan list: details of the chosen access point
 *      C .............. scan list or details: connect to it
 *      M .............. scan list: join a network by typing its name
 *      S .............. scan list: the saved networks (D forgets one)
 *      I .............. scan list: the status of the connection
 *      R .............. scan list: scan again
 *      ESC ............ back one screen
 *      ~ (SHIFT+`) .... reboot into BOOTSEL (USB drive) mode, from anywhere,
 *                       also while the radio is scanning or joining. While a
 *                       name or password is being typed, one ~ is a character
 *                       and two in quick succession reboot.
 *
 *  Two headers keep this file to code only: netthing_const.h holds every
 *  constant and platform.h is the whole interface to the machine (clock,
 *  sleep, BOOTSEL, exit to the UF2 Loader), implemented in platform_pico.c
 *  for the PicoCalc (Pico 2 W).
 *
 *  Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* WIDTH, HEIGHT, GLYPH_HEIGHT, RGB(), lcd_*()      */
#include "southbridge.h"  /* sb_init()                                        */
#include "keyboard.h"     /* KEY_* codes, KEY_STATE_* values                  */

#include "platform.h"     /* clock, sleep, BOOTSEL, loader exit (per platform) */
#include "wifi.h"         /* the radio: start, scan, join                     */
#include "storage.h"      /* whole files on the SD card                       */
#include "secret.h"       /* the scrambled list of saved networks             */
#include "ui.h"           /* rows of text, keys, typing a line                */
#include "gfx.h"          /* the strip of the signal history                  */
#include "tools.h"        /* ping, name lookup, test server, the clock        */
#include "link.h"         /* how the LAN is reached                           */
#include "usbnet.h"       /* IP over USB                                      */
#include "netthing_const.h"   /* every constant and screen position */

/* ===================================================================== */
/*  Forward declarations (so main() can sit at the top)                   */
/* ===================================================================== */


static const char *security_short(uint8_t security);
static const char *security_long(uint8_t security);
static unsigned frequency_mhz(unsigned channel);

static void saved_load(void);
static int  saved_find(const char *ssid);
static bool saved_store(void);

static void splash_screen(void);
static int  scan_screen(void);
static int  usb_connect(void);
static bool leave_current_link(void);
static void draw_scan_screen(unsigned top, unsigned chosen);
static void draw_heading(void);
static void draw_ap_row(unsigned slot, unsigned index, bool chosen);
static void draw_list(unsigned top, unsigned chosen);
static uint8_t detail_screen(unsigned index);

static void connect_selected(unsigned index);
static void connect_to(const char *ssid_in, uint8_t security, bool ask_name);
static int  wait_for_link(uint32_t started, bool *cancelled, const char *name);
static void offer_save(const char *ssid, const char *pass, const uint8_t *bssid);
static bool bssid_known(const uint8_t *bssid);
static void remember_bssid(const char *ssid, const uint8_t *bssid);
static void status_screen(void);
static void saved_screen(void);

/* ===================================================================== */
/*  State                                                                 */
/* ===================================================================== */
static secret_net_t saved[SECRET_NETS_MAX];     /* the networks on the SD card */
static unsigned     saved_count;
static bool         saved_loaded;
static int          requested_mode = MODE_NONE;     /* a mode the status screen asked for */
static const char  *saved_problem;              /* why the list could not be read, or NULL */

/* ===================================================================== */
/*  main: set up the hardware once, then splash -> scan screen -> splash  */
/* ===================================================================== */
/* Ask before the connection is dropped. Returns true only for Y. */
static bool confirm(const char *line_1, const char *line_2)
{
    uint8_t key;

    screen_clear();
    put_row(CONFIRM_ROW, line_1, COL_YELLOW);
    if (line_2 != NULL && line_2[0] != '\0')
        put_row(CONFIRM_ROW + 1, line_2, COL_WHITE);
    put_row(CONFIRM_ROW + 3, CONFIRM_KEYS_LINE, COL_CYAN);
    drain_keys();
    key = wait_any_key();
    return key == 'y' || key == 'Y';
}

int main(void)
{
    int mode = MODE_WIFI;

    plat_init();

    sb_init();                /* keyboard / south-bridge I2C (no bg poll) */
    lcd_init();               /* ST7365P LCD */
    lcd_enable_cursor(false); /* no blinking text cursor */

    for (;;) {
        splash_screen();

        uint8_t key = wait_any_key();
        if (key == KEY_ESC || key == 'q' || key == 'Q') {
            if (link_kind() != LINK_NONE && !confirm(CONFIRM_LEAVE, CONFIRM_LEAVE_NOTE))
                continue;     /* an ESC too many: stay in the program, still connected */
            go_loader();      /* does not return */
        }
        if (key == 'u' || key == 'U') {
            int next = usb_connect();

            if (next == MODE_NONE)
                continue;             /* given up: back to the title screen */
            mode = next;
        }
        /* The program is in one of three modes. ESC in a mode comes back here, and a key
         * then goes on in the mode that was left (the Wi-Fi scan the first time). */
        for (;;) {
            int next;

            if (mode == MODE_BT)
                next = tool_bt_menu();
            else if (mode == MODE_LAN)
                next = tool_lan_menu();
            else
                next = scan_screen();
            if (next == MODE_TITLE)
                break;
            mode = next;
        }
    }
}

/* ===================================================================== */
/*  Signal and security helpers                                           */
/* ===================================================================== */
static const char *security_short(uint8_t security)
{
    if (security & WIFI_SEC_WPA2)
        return (security & WIFI_SEC_WPA) ? SEC_SHORT_MIXED : SEC_SHORT_WPA2;
    if (security & WIFI_SEC_WPA)
        return SEC_SHORT_WPA;
    return (security & WIFI_SEC_PRIVACY) ? SEC_SHORT_WEP : SEC_SHORT_OPEN;
}

static const char *security_long(uint8_t security)
{
    if (security & WIFI_SEC_WPA2)
        return (security & WIFI_SEC_WPA) ? SEC_LONG_MIXED : SEC_LONG_WPA2;
    if (security & WIFI_SEC_WPA)
        return SEC_LONG_WPA;
    return (security & WIFI_SEC_PRIVACY) ? SEC_LONG_WEP : SEC_LONG_OPEN;
}

static unsigned frequency_mhz(unsigned channel)
{
    return channel == 14 ? FREQ_CHANNEL_14 : FREQ_BASE_MHZ + FREQ_STEP_MHZ * channel;
}

/* ===================================================================== */
/*  The saved networks (the scrambled file on the SD card)                */
/* ===================================================================== */
/* Read the file once. A missing file is just an empty list; a card or file
 * problem is remembered in saved_problem for the scan screen to show. */
static void saved_load(void)
{
    static uint8_t file[SECRET_FILE_MAX];
    size_t len = 0;
    int rc;

    if (saved_loaded)
        return;
    saved_loaded = true;
    saved_count = 0;
    saved_problem = NULL;
    rc = storage_read(SAVED_FILE, file, sizeof file, &len);
    if (rc == 0) {
        if (secret_unpack(file, len, saved, SECRET_NETS_MAX, &saved_count) != 0) {
            saved_count = 0;
            saved_problem = "SAVED NETWORKS FILE IS DAMAGED";
        }
    } else if (rc != 1) {
        saved_problem = storage_ready() != 0 ? "NO SD CARD, NOTHING CAN BE SAVED"
                                             : "THE SD CARD COULD NOT BE READ";
    }
}

static int saved_find(const char *ssid)
{
    unsigned i;

    for (i = 0; i < saved_count; i++)
        if (strcmp(saved[i].ssid, ssid) == 0)
            return (int)i;
    return -1;
}

/* Write the list to the card. Returns false (and says why) if that failed. */
static bool saved_store(void)
{
    static uint8_t file[SECRET_FILE_MAX];
    size_t size = secret_pack(saved, saved_count, plat_random32(), file, sizeof file);
    int rc = size ? storage_write(SAVED_FILE, file, size) : 2;

    if (rc == 0) {
        saved_problem = NULL;
        return true;
    }
    saved_problem = rc == 1 ? "NO SD CARD, NOTHING WAS SAVED" : "THE SD CARD WOULD NOT TAKE IT";
    return false;
}

/* ===================================================================== */
/*  Screens                                                               */
/* ===================================================================== */
static void splash_screen(void)
{
    lcd_clear_screen();
    put_row(SPLASH_ROW_TITLE, "PICOCALC NETTHING", COL_CYAN);
    put_row(SPLASH_ROW_TAGLINE_1, SPLASH_TAGLINE_1, COL_WHITE);
    put_row(SPLASH_ROW_TAGLINE_2, SPLASH_TAGLINE_2, COL_WHITE);
    put_row(SPLASH_ROW_VERSION, VERSION, COL_CYAN);
    put_row(SPLASH_ROW_AUTHOR, "BY THOMAS DZUBIN", COL_WHITE);
    put_row(SPLASH_ROW_PROMPT, "PRESS ANY KEY TO START", COL_WHITE);
    put_row(SPLASH_ROW_EXIT, "or ESC / Q to exit", COL_WHITE);
    put_row(SPLASH_ROW_USB, SPLASH_USB_LINE, COL_WHITE);
}

/* ===================================================================== */
/*  The scan, over and over                                               */
/* ===================================================================== */
/* The scan screen scans all the time: each Wi-Fi scan continues the list of
 * the one before. */
#define EVENT_NONE    0
#define EVENT_STATUS  1         /* the heading changed */
#define EVENT_LIST    2         /* a Wi-Fi scan ended: the list may have changed */

static bool     scanning;       /* a Wi-Fi scan is running */
static bool     paused;         /* P: no new scan is started */
static bool     auto_paused;    /* paused because a link is up (it ends with the link) */
static bool     fresh = true;   /* the next scan empties the list first */
static uint32_t scan_started_ms;

/* Carry the scanning on one step. Called often. */
static int scan_step(void)
{
    if (paused)
        return EVENT_NONE;
    if (scanning) {
        if (wifi_scan_busy() && plat_now_ms() - scan_started_ms <= SCAN_TIMEOUT_MS)
            return EVENT_NONE;
        scanning = false;
        return EVENT_LIST;
    }
    if ((fresh ? wifi_scan_start() : wifi_scan_more()) != 0) {
        put_row(SCAN_ROW_MESSAGE, "THE SCAN WOULD NOT START", COL_YELLOW);
        paused = true;
        return EVENT_STATUS;
    }
    scanning = true;
    scan_started_ms = plat_now_ms();
    if (fresh) {
        fresh = false;
        return EVENT_LIST;              /* the list was emptied */
    }
    return EVENT_STATUS;
}

/* Before leaving the scan screen for another screen that uses the radio: let a
 * running scan end. */
static void scan_leave(void)
{
    uint32_t started = plat_now_ms();

    while (wifi_scan_busy() && plat_now_ms() - started < SCAN_TIMEOUT_MS)
        plat_sleep_ms(POLL_MS);
    scanning = false;
}

/* "WIFI SCAN  12 FOUND" on the left, whether it is scanning on the right */
static void draw_heading(void)
{
    char line[TCOLS + 1];
    const char *state = paused ? "PAUSED" : "SCANNING";

    clear_row(SCAN_ROW_HEADING);
    snprintf(line, sizeof line, "WIFI SCAN  %u FOUND", wifi_ap_count());
    put_at(0, SCAN_ROW_HEADING, line, COL_CYAN);
    put_at(TCOLS - (int)strlen(state), SCAN_ROW_HEADING, state, paused ? COL_YELLOW : COL_CYAN);
}

/* One list row, on screen row SCAN_ROW_FIRST + slot, for access point `index`. */
static void draw_ap_row(unsigned slot, unsigned index, bool chosen)
{
    wifi_ap_t ap;
    char text[TCOLS + 1];
    unsigned bars, i;
    int row = SCAN_ROW_FIRST + (int)slot;
    uint16_t name_colour;

    clear_row(row);
    if (!wifi_ap_get(index, &ap))
        return;
    bars = bars_for(ap.rssi);

    put_at(SCAN_COL_MARKER, row, chosen ? ">" : " ", COL_GREEN);
    snprintf(text, sizeof text, "%.*s", SCAN_NAME_WIDTH,
             ap.ssid[0] ? ap.ssid : SCAN_HIDDEN_NAME);
    switch (secret_match(saved, saved_count, ap.ssid, ap.bssid)) {
    case SECRET_MATCH_BOTH:
        name_colour = COL_GREEN;
        break;
    case SECRET_MATCH_ONE:
        name_colour = COL_YELLOW;
        break;
    default:
        name_colour = chosen ? COL_CYAN : COL_WHITE;
        break;
    }
    put_at(SCAN_COL_NAME, row, text, name_colour);
    for (i = 0; i < BAR_CELLS; i++)
        text[i] = i < bars ? BAR_FULL : BAR_EMPTY;
    text[BAR_CELLS] = '\0';
    put_at(SCAN_COL_BARS, row, text, bars_colour(bars));
    snprintf(text, sizeof text, "%4d", (int)ap.rssi);
    put_at(SCAN_COL_DBM, row, text, COL_WHITE);
    snprintf(text, sizeof text, "%2u", (unsigned)ap.channel);
    put_at(SCAN_COL_CHANNEL, row, text, COL_WHITE);
    put_at(SCAN_COL_SECURITY, row, security_short(ap.security),
           ap.security ? COL_WHITE : COL_YELLOW);
}

/* The whole list, starting with access point `top`; `chosen` is marked. */
static void draw_list(unsigned top, unsigned chosen)
{
    unsigned slot;

    draw_heading();
    for (slot = 0; slot < SCAN_ROWS; slot++)
        draw_ap_row(slot, top + slot, top + slot == chosen);
    if (wifi_ap_count() == 0)
        put_row(SCAN_ROW_FIRST, scanning ? "SCANNING..." : "NO ACCESS POINTS FOUND", COL_YELLOW);
}

/* The scan screen from scratch: help lines, the list, and any trouble with the
 * saved networks. */
static void draw_scan_screen(unsigned top, unsigned chosen)
{
    screen_clear();
    put_row(SCAN_ROW_PAUSE, "P=PAUSE", COL_WHITE);
    put_row(SCAN_ROW_HELP, SCAN_HELP_1, COL_WHITE);
    put_row(SCAN_ROW_HELP_2, SCAN_HELP_2, COL_WHITE);
    draw_list(top, chosen);
    if (paused && wifi_connected())
        put_row(SCAN_ROW_MESSAGE, SCAN_PAUSED_LINKED, COL_YELLOW);
    if (saved_problem != NULL)
        put_row(SCAN_ROW_MESSAGE, saved_problem, COL_YELLOW);
}

/* A second connection is only made after the first is given up (one way to the
 * LAN at a time). Returns false if the operator said no. */
static bool leave_current_link(void)
{
    if (link_kind() != LINK_NONE) {
        if (!confirm(CONFIRM_DISCONNECT, CONFIRM_SWITCH_NOTE))
            return false;
        link_drop();
    }
    return true;
}

/* While a link is up the scanning stays paused (a scan makes the radio leave
 * the channel for a moment); it starts again when the link is gone. */
static void pause_for_link(void)
{
    if (wifi_connected()) {
        paused = true;
        auto_paused = true;
    } else if (auto_paused) {
        paused = false;
        auto_paused = false;
    }
}

/* IP over USB: start the adapter and wait for the PC to give an address. Returns
 * the mode to go to (the LAN tools) when the link is up, or MODE_NONE if the
 * operator gave up. */
static int usb_connect(void)
{
    static const char *const lines[] = USBSCREEN_LINES;
    unsigned i;
    int last = -1;

    if (!leave_current_link())
        return MODE_NONE;
    screen_clear();
    put_row(USBSCREEN_ROW_HEADING, "IP OVER USB", COL_CYAN);
    put_row(USBSCREEN_ROW_STATUS, "STARTING...", COL_YELLOW);
    if (wifi_start() != 0 || usbnet_start() != 0) {      /* the chip's driver also runs the USB network */
        clear_row(USBSCREEN_ROW_STATUS);
        put_row(USBSCREEN_ROW_STATUS, USBSCREEN_FAILED, COL_YELLOW);
        pause_ms(NOTICE_MS * 2);
        return MODE_NONE;
    }
    for (i = 0; lines[i] != NULL; i++)
        put_at(MENU_COL, USBSCREEN_ROW_FIRST + (int)i, lines[i], COL_WHITE);
    put_row(TOOL_ROW_HELP, USBSCREEN_HELP, COL_WHITE);
    drain_keys();
    for (;;) {
        int now_state = usbnet_state();

        if (poll_key() == KEY_ESC) {
            usbnet_stop();
            return MODE_NONE;
        }
        if (now_state == USBNET_UP) {
            screen_clear();
            return MODE_LAN;
        }
        if (now_state != last) {
            last = now_state;
            clear_row(USBSCREEN_ROW_STATUS);
            put_row(USBSCREEN_ROW_STATUS, now_state == USBNET_LINK ? USBSCREEN_WAIT_DHCP : USBSCREEN_WAIT_PC, COL_YELLOW);
        }
        plat_sleep_ms(POLL_MS);
    }
}

/* Back from another screen: the scan goes on, unless a link is up. */
static void resume_scanning(void)
{
    paused = false;
    auto_paused = false;
    pause_for_link();
}

/* Keep the marker on the access point that was chosen (by its BSSID) after the
 * list changed; with none chosen yet it stays on the first row. */
static void reselect(unsigned *chosen, const uint8_t *bssid, bool have)
{
    unsigned i, count = wifi_ap_count();
    wifi_ap_t ap;

    if (have)
        for (i = 0; i < count; i++)
            if (wifi_ap_get(i, &ap) && memcmp(ap.bssid, bssid, sizeof ap.bssid) == 0) {
                *chosen = i;
                return;
            }
    if (*chosen >= count)
        *chosen = count ? count - 1 : 0;
}

/* Remember which access point the marker is on. */
static void note_chosen(unsigned chosen, uint8_t *bssid, bool *have)
{
    wifi_ap_t ap;

    if (wifi_ap_get(chosen, &ap)) {
        memcpy(bssid, ap.bssid, sizeof ap.bssid);
        *have = true;
    }
}

static int scan_screen(void)
{
    unsigned chosen = 0, top = 0;
    uint8_t chosen_bssid[6] = { 0 };
    bool have_chosen = false, redraw = true;

    screen_clear();
    put_row(SCAN_ROW_MESSAGE, "STARTING THE WI-FI CHIP...", COL_YELLOW);
    if (wifi_start() != 0) {
        put_row(NOCHIP_ROW, NOCHIP_LINE_1, COL_YELLOW);
        put_row(NOCHIP_ROW + 2, NOCHIP_LINE_2, COL_WHITE);
        put_row(NOCHIP_ROW + 3, NOCHIP_LINE_3, COL_WHITE);
        put_row(NOCHIP_ROW + 4, NOCHIP_LINE_4, COL_WHITE);
        put_row(NOCHIP_ROW + 6, NOCHIP_LINE_5, COL_WHITE);
        while (wait_any_key() != KEY_ESC)
            ;
        return MODE_TITLE;
    }
    saved_load();
    scanning = paused = auto_paused = false;
    fresh = true;
    pause_for_link();
    drain_keys();

    for (;;) {
        uint8_t key;
        unsigned count;

        if (redraw) {
            redraw = false;
            draw_scan_screen(top, chosen);
        }
        for (;;) {
            int event;

            key = poll_key();
            if (key != 0)
                break;
            event = scan_step();
            if (event == EVENT_LIST) {
                reselect(&chosen, chosen_bssid, have_chosen);
                if (chosen < top)
                    top = chosen;
                else if (chosen >= top + SCAN_ROWS)
                    top = chosen + 1 - SCAN_ROWS;
                draw_list(top, chosen);
            } else if (event == EVENT_STATUS) {
                draw_heading();
            }
            plat_sleep_ms(POLL_MS);
        }
        count = wifi_ap_count();
        if (key == KEY_ESC) {
            scan_leave();
            return MODE_TITLE;
        }
        if (key == 'r' || key == 'R') {
            if (!wifi_scan_busy()) {
                chosen = top = 0;
                have_chosen = false;
                scanning = false;
            }
            fresh = true;
            paused = false;
            redraw = true;
        } else if (key == 'p' || key == 'P') {
            paused = !paused;
            auto_paused = false;
            clear_row(SCAN_ROW_MESSAGE);
            if (paused && wifi_connected())
                put_row(SCAN_ROW_MESSAGE, SCAN_PAUSED_LINKED, COL_YELLOW);
            draw_heading();
        } else if (key == KEY_UP && chosen > 0) {
            chosen--;
            note_chosen(chosen, chosen_bssid, &have_chosen);
        } else if (key == KEY_DOWN && chosen + 1 < count) {
            chosen++;
            note_chosen(chosen, chosen_bssid, &have_chosen);
        } else if ((key == KEY_ENTER || key == KEY_RETURN) && count > 0) {
            if (leave_current_link()) {
                scan_leave();
                connect_selected(chosen);
            }
            resume_scanning();
            redraw = true;
        } else if ((key == 'd' || key == 'D') && count > 0) {
            if (detail_screen(chosen) == 'C' && leave_current_link()) {
                scan_leave();
                connect_selected(chosen);
            }
            resume_scanning();
            redraw = true;
        } else if (key == 'm' || key == 'M') {
            if (leave_current_link()) {
                scan_leave();
                connect_to("", 0, true);
            }
            resume_scanning();
            redraw = true;
        } else if (key == 's' || key == 'S') {
            saved_screen();
            resume_scanning();
            redraw = true;
        } else if (key == 'l' || key == 'L') {
            if (link_kind() == LINK_NONE) {
                notice(SCAN_ROW_MESSAGE, LINK_NEEDED, COL_YELLOW);
                redraw = true;
            } else {
                scan_leave();
                return MODE_LAN;
            }
        } else if (key == 'b' || key == 'B') {
            scan_leave();
            return MODE_BT;
        } else if (key == 'i' || key == 'I') {
            if (wifi_link_state() == WIFI_LINK_UP) {
                scan_leave();
                status_screen();
                resume_scanning();
            } else
                notice(SCAN_ROW_MESSAGE, "NOT CONNECTED", COL_YELLOW);
            redraw = true;
        }
        if (requested_mode != MODE_NONE) {
            int next = requested_mode;      /* the status screen asked for the LAN tools or Bluetooth */

            requested_mode = MODE_NONE;
            scan_leave();
            return next;
        }
        if (chosen < top)
            top = chosen;
        else if (chosen >= top + SCAN_ROWS)
            top = chosen + 1 - SCAN_ROWS;
        if (!redraw)
            draw_list(top, chosen);
    }
}

/* Everything the scan learned about one access point. Returns 'C' if the
 * operator asked to connect to it, otherwise 0. */
static uint8_t detail_screen(unsigned index)
{
    static const char *const words[] = SIGNAL_WORDS;
    wifi_ap_t ap;
    char text[TCOLS + 1];
    unsigned bars;
    uint8_t key;

    if (!wifi_ap_get(index, &ap))
        return 0;
    bars = bars_for(ap.rssi);

    screen_clear();
    put_row(DETAIL_ROW_HEADING, "ACCESS POINT", COL_CYAN);

    put_at(DETAIL_COL_LABEL, DETAIL_ROW_NAME, "NAME", COL_CYAN);
    snprintf(text, sizeof text, "%s", ap.ssid[0] ? ap.ssid : SCAN_HIDDEN_NAME);
    put_at(DETAIL_COL_LABEL, DETAIL_ROW_NAME_VALUE, text, COL_WHITE);

    put_at(DETAIL_COL_LABEL, DETAIL_ROW_BSSID, "BSSID", COL_CYAN);
    snprintf(text, sizeof text, "%02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0], ap.bssid[1],
             ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    put_at(DETAIL_COL_VALUE, DETAIL_ROW_BSSID, text, COL_WHITE);

    put_at(DETAIL_COL_LABEL, DETAIL_ROW_CHANNEL, "CHANNEL", COL_CYAN);
    snprintf(text, sizeof text, "%u  (%u MHZ)", (unsigned)ap.channel, frequency_mhz(ap.channel));
    put_at(DETAIL_COL_VALUE, DETAIL_ROW_CHANNEL, text, COL_WHITE);

    put_at(DETAIL_COL_LABEL, DETAIL_ROW_SIGNAL, "SIGNAL", COL_CYAN);
    snprintf(text, sizeof text, "%d DBM  %s", (int)ap.rssi, words[bars <= 1 ? 0 : bars - 1]);
    put_at(DETAIL_COL_VALUE, DETAIL_ROW_SIGNAL, text, bars_colour(bars));

    put_at(DETAIL_COL_LABEL, DETAIL_ROW_SECURITY, "SECURITY", COL_CYAN);
    put_at(DETAIL_COL_VALUE, DETAIL_ROW_SECURITY, security_long(ap.security), COL_WHITE);

    put_at(DETAIL_COL_LABEL, DETAIL_ROW_SAVED, "SAVED", COL_CYAN);
    if (ap.ssid[0] && saved_find(ap.ssid) >= 0)
        put_at(DETAIL_COL_VALUE, DETAIL_ROW_SAVED, DETAIL_SAVED_YES, COL_GREEN);
    else
        put_at(DETAIL_COL_VALUE, DETAIL_ROW_SAVED,
               storage_ready() == 0 ? DETAIL_SAVED_NO : DETAIL_SAVED_NO_SD, COL_WHITE);

    put_row(DETAIL_ROW_HELP, DETAIL_HELP, COL_WHITE);
    for (;;) {
        key = wait_any_key();
        if (key == KEY_ESC)
            return 0;
        if (key == 'c' || key == 'C')
            return 'C';
    }
}

/* ===================================================================== */
/*  Joining a network                                                     */
/* ===================================================================== */
static void connect_selected(unsigned index)
{
    wifi_ap_t ap;

    if (!wifi_ap_get(index, &ap))
        return;
    if (ap.ssid[0])
        connect_to(ap.ssid, ap.security, false);
    else
        connect_to("", ap.security, true);      /* hidden: the name has to be typed */
}

/* Wait while the radio joins, showing the seconds gone. Returns the link state
 * that ended it (WIFI_LINK_FAILED after the time limit); *cancelled is set if
 * ESC was pressed. */
static int wait_for_link(uint32_t started, bool *cancelled, const char *name)
{
    char line[TCOLS + 1];
    uint32_t last_draw = started - JOIN_UPDATE_MS;

    (void)name;
    *cancelled = false;
    drain_keys();
    for (;;) {
        uint32_t now = plat_now_ms();
        int state = wifi_link_state();

        if (state != WIFI_LINK_JOINING)
            return state;
        if (poll_key() == KEY_ESC) {
            *cancelled = true;
            return WIFI_LINK_IDLE;
        }
        if (now - started > JOIN_TIMEOUT_MS)
            return WIFI_LINK_FAILED;
        if (now - last_draw >= JOIN_UPDATE_MS) {
            snprintf(line, sizeof line, "JOINING... %u S", (unsigned)((now - started) / 1000));
            put_row(JOIN_ROW_MESSAGE, line, COL_YELLOW);
            last_draw = now;
        }
        plat_sleep_ms(POLL_MS);
    }
}

/* Join a network: find or ask for its password, try the ways of joining it,
 * offer to save a password that worked, then show the connection. With
 * ask_name the name is typed too (a hidden network, or the M key). */
static void connect_to(const char *ssid_in, uint8_t security, bool ask_name)
{
    char ssid[SECRET_SSID_MAX + 1];
    char pass[SECRET_PASS_MAX + 1];
    bool has_password = false, typed = false, cancelled = false;
    unsigned attempts, attempt;
    int result = WIFI_LINK_FAILED, known;
    const char *reason, *hint;

    snprintf(ssid, sizeof ssid, "%s", ssid_in);
    pass[0] = '\0';
    if (ask_name && !ask_text("JOIN BY NAME", "", "NETWORK NAME", ssid, SECRET_SSID_MAX, 1, false, false))
        return;
    known = saved_find(ssid);
    if (known >= 0) {
        snprintf(pass, sizeof pass, "%s", saved[known].pass);
        has_password = pass[0] != '\0';
    } else if (!ask_name && security == 0) {
        has_password = false;                   /* an open network */
    } else if (!ask_name && (security & WIFI_SEC_PRIVACY) && !(security & (WIFI_SEC_WPA | WIFI_SEC_WPA2))) {
        notice(SCAN_ROW_MESSAGE, "WEP NETWORKS CANNOT BE JOINED", COL_YELLOW);
        return;
    } else {
        if (!ask_text("PASSWORD FOR", ssid, "PASSWORD", pass, SECRET_PASS_MAX, PASS_MIN, true,
                      ask_name && security == 0))
            return;
        typed = true;
        has_password = pass[0] != '\0';
    }

    attempts = wifi_attempts(security, has_password);
    screen_clear();
    put_row(JOIN_ROW_HEADING, "CONNECTING TO", COL_CYAN);
    put_row(JOIN_ROW_NAME, ssid, COL_WHITE);
    put_row(JOIN_ROW_HELP, "ESC CANCELS", COL_WHITE);
    for (attempt = 0; attempt < attempts; attempt++) {
        char line[TCOLS + 1];

        snprintf(line, sizeof line, "TRY %u OF %u", attempt + 1, attempts);
        put_row(JOIN_ROW_TRY, line, COL_WHITE);
        if (wifi_connect_start(ssid, pass, security, has_password, attempt) != 0) {
            result = WIFI_LINK_FAILED;
            break;
        }
        result = wait_for_link(plat_now_ms(), &cancelled, ssid);
        if (cancelled) {
            wifi_disconnect();
            return;
        }
        if (result == WIFI_LINK_UP || result == WIFI_LINK_NO_NETWORK)
            break;
    }

    if (result == WIFI_LINK_UP) {
        wifi_info_t info;

        if (!wifi_info_get(&info))
            memset(info.bssid, 0, sizeof info.bssid);
        if (typed && has_password)
            offer_save(ssid, pass, info.bssid);
        else if (known >= 0)
            remember_bssid(ssid, info.bssid);
        status_screen();
        return;
    }
    wifi_disconnect();
    screen_clear();
    put_row(JOIN_ROW_HEADING, "COULD NOT CONNECT TO", COL_CYAN);
    put_row(JOIN_ROW_NAME, ssid, COL_WHITE);
    if (result == WIFI_LINK_NO_NETWORK) {
        reason = "NETWORK NOT FOUND";
        hint = "TOO FAR AWAY, OR SWITCHED OFF?";
    } else if (result == WIFI_LINK_BAD_PASSWORD) {
        reason = "THE PASSWORD WAS REFUSED";
        hint = known >= 0 ? "FORGET IT WITH S, THEN TRY AGAIN" : "CHECK IT AND TRY AGAIN";
    } else {
        reason = "COULD NOT JOIN";
        hint = "THE ACCESS POINT DID NOT ANSWER";
    }
    put_row(JOIN_ROW_MESSAGE, reason, COL_YELLOW);
    put_row(JOIN_ROW_NOTE, hint, COL_WHITE);
    put_row(JOIN_ROW_HELP, "PRESS ANY KEY", COL_WHITE);
    (void)wait_any_key();
}

/* "SAVE THIS NETWORK? Y/N" after a password that worked. */
static void offer_save(const char *ssid, const char *pass, const uint8_t *bssid)
{
    int at;
    uint8_t key;

    saved_load();
    screen_clear();
    put_row(JOIN_ROW_HEADING, "CONNECTED TO", COL_CYAN);
    put_row(JOIN_ROW_NAME, ssid, COL_GREEN);
    put_row(JOIN_SAVE_ROW, "SAVE THIS NETWORK? Y/N", COL_WHITE);
    key = wait_any_key();
    if (key != 'y' && key != 'Y')
        return;
    at = saved_find(ssid);
    if (at < 0) {
        if (saved_count >= SECRET_NETS_MAX) {
            put_row(JOIN_SAVE_ROW, "THE SAVED LIST IS FULL", COL_YELLOW);
            pause_ms(NOTICE_MS);
            return;
        }
        at = (int)saved_count++;
    }
    snprintf(saved[at].ssid, sizeof saved[at].ssid, "%s", ssid);
    snprintf(saved[at].pass, sizeof saved[at].pass, "%s", pass);
    if (bssid_known(bssid))
        (void)secret_add_bssid(&saved[at], bssid);
    put_row(JOIN_SAVE_ROW, "SAVING...", COL_YELLOW);
    if (saved_store())
        put_row(JOIN_SAVE_ROW, "SAVED ON THE SD CARD", COL_GREEN);
    else
        put_row(JOIN_SAVE_ROW, saved_problem, COL_YELLOW);
    pause_ms(NOTICE_MS);
}

/* False for an all-zero address (the radio could not say which access point). */
static bool bssid_known(const uint8_t *bssid)
{
    unsigned i;

    for (i = 0; i < 6; i++)
        if (bssid[i] != 0)
            return true;
    return false;
}

/* After joining a saved network: add the access point to its addresses, and
 * write the file if that was something new. */
static void remember_bssid(const char *ssid, const uint8_t *bssid)
{
    int at = saved_find(ssid);

    if (at >= 0 && bssid_known(bssid) && secret_add_bssid(&saved[at], bssid))
        (void)saved_store();
}

/* The labels and help lines of the status screen. */
static void draw_status_frame(void)
{
    screen_clear();
    put_row(STATUS_ROW_HEADING, "CONNECTED", COL_GREEN);
    put_row(STATUS_ROW_HELP, STATUS_HELP, COL_WHITE);
    put_row(STATUS_ROW_HELP_2, STATUS_HELP_2, COL_WHITE);
    put_row(STATUS_ROW_HELP_3, STATUS_HELP_3, COL_WHITE);
    put_at(STATUS_COL_LABEL, STATUS_ROW_NAME, "NETWORK", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_SIGNAL, "SIGNAL", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_IP, "ADDRESS", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_MASK, "NETMASK", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_GATEWAY, "GATEWAY", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_DNS, "DNS", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_MAC, "THIS MAC", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_IP6, "IPV6 ADDRESS", COL_CYAN);
    put_at(STATUS_COL_LABEL, STATUS_ROW_IP6_LL, "IPV6 LINK-LOCAL", COL_CYAN);
    put_at(STATUS_COL_LABEL, SIGHIST_ROW_LABEL, "SIGNAL, LAST FEW MINUTES", COL_CYAN);
}

/* The signal of the last few minutes, one sample for each update of the status screen. */
static int8_t   signal_history[SIGHIST_SAMPLES];
static unsigned signal_count;

static void draw_signal_history(int rssi)
{
    unsigned i;

    if (signal_count == SIGHIST_SAMPLES) {
        memmove(signal_history, signal_history + 1, SIGHIST_SAMPLES - 1);
        signal_count--;
    }
    signal_history[signal_count++] = (int8_t)(rssi < -127 ? -127 : (rssi > 0 ? 0 : rssi));
    gfx_rect(COL_BG, SIGHIST_X - 2, SIGHIST_Y - 2, SIGHIST_SAMPLES * SIGHIST_STEP + 4, SIGHIST_H + 4);
    gfx_rect(COL_WHITE, SIGHIST_X - 2, SIGHIST_Y + SIGHIST_H, SIGHIST_SAMPLES * SIGHIST_STEP + 3, 1);
    for (i = 0; i < signal_count; i++) {
        unsigned h = gfx_signal_height(signal_history[i], SIGHIST_H);

        if (h < 2)
            h = 2;
        gfx_rect(bars_colour(bars_for(signal_history[i])), SIGHIST_X + (int)(SIGHIST_SAMPLES - signal_count + i) * SIGHIST_STEP,
                 SIGHIST_Y + SIGHIST_H - (int)h, SIGHIST_STEP, (int)h);
    }
}

/* The connection: name and signal (live), address details, this device's MAC,
 * and the clock once it has been set. D disconnects, ESC leaves it connected
 * and goes back, P N T and W start the network tools. */
static void status_screen(void)
{
    wifi_info_t info;
    char text[TCOLS + 1];
    uint32_t last = plat_now_ms() - STATUS_UPDATE_MS;
    unsigned bars, i;

    draw_status_frame();
    drain_keys();
    for (;;) {
        uint8_t key = poll_key();
        uint32_t now = plat_now_ms();

        if (key == KEY_ESC)
            return;
        if (key == 'd' || key == 'D') {
            if (confirm(CONFIRM_DISCONNECT, info.ssid)) {
                wifi_disconnect();
                return;
            }
            draw_status_frame();
            last = now - STATUS_UPDATE_MS;
            drain_keys();
            continue;
        }
        if (key == 'l' || key == 'L') {
            requested_mode = MODE_LAN;      /* the scan screen carries it out */
            return;
        }
        if (key == 'b' || key == 'B') {
            requested_mode = MODE_BT;
            return;
        }
        if (key == 't' || key == 'T') {
            tool_menu();
            draw_status_frame();
            last = now - STATUS_UPDATE_MS;
            drain_keys();
            continue;
        }
        if (now - last >= STATUS_UPDATE_MS) {
            last = now;
            if (!wifi_info_get(&info)) {
                put_row(STATUS_ROW_HEADING, "CONNECTION LOST", COL_YELLOW);
                put_row(STATUS_ROW_HELP, "PRESS ANY KEY", COL_WHITE);
                (void)wait_any_key();
                return;
            }
            bars = bars_for(info.rssi);
            draw_signal_history(info.rssi);
            clear_row(STATUS_ROW_NAME);
            put_at(STATUS_COL_LABEL, STATUS_ROW_NAME, "NETWORK", COL_CYAN);
            put_at(STATUS_COL_VALUE, STATUS_ROW_NAME, info.ssid, COL_WHITE);
            for (i = 0; i < BAR_CELLS; i++)
                text[i] = i < bars ? BAR_FULL : BAR_EMPTY;
            snprintf(text + BAR_CELLS, sizeof text - BAR_CELLS, "  %d DBM", (int)info.rssi);
            clear_row(STATUS_ROW_SIGNAL);
            put_at(STATUS_COL_LABEL, STATUS_ROW_SIGNAL, "SIGNAL", COL_CYAN);
            put_at(STATUS_COL_VALUE, STATUS_ROW_SIGNAL, text, bars_colour(bars));
            put_at(STATUS_COL_VALUE, STATUS_ROW_IP, info.ip, COL_WHITE);
            put_at(STATUS_COL_VALUE, STATUS_ROW_MASK, info.netmask, COL_WHITE);
            put_at(STATUS_COL_VALUE, STATUS_ROW_GATEWAY, info.gateway, COL_WHITE);
            put_at(STATUS_COL_VALUE, STATUS_ROW_DNS, info.dns, COL_WHITE);
            snprintf(text, sizeof text, "%02X:%02X:%02X:%02X:%02X:%02X", info.mac[0], info.mac[1],
                     info.mac[2], info.mac[3], info.mac[4], info.mac[5]);
            put_at(STATUS_COL_VALUE, STATUS_ROW_MAC, text, COL_WHITE);
            clear_row(STATUS_ROW_IP6 + 1);
            put_at(STATUS_COL_IP6, STATUS_ROW_IP6 + 1, info.ip6[0] ? info.ip6 : STATUS_IP6_NONE,
                   info.ip6[0] ? COL_GREEN : COL_YELLOW);
            clear_row(STATUS_ROW_IP6_LL + 1);
            put_at(STATUS_COL_IP6, STATUS_ROW_IP6_LL + 1, info.ip6_local[0] ? info.ip6_local : STATUS_IP6_NONE,
                   info.ip6_local[0] ? COL_WHITE : COL_YELLOW);
            if (tools_clock_text(text, sizeof text)) {
                clear_row(STATUS_ROW_TIME);
                put_at(STATUS_COL_LABEL, STATUS_ROW_TIME, "TIME", COL_CYAN);
                put_at(STATUS_COL_VALUE, STATUS_ROW_TIME, text, COL_GREEN);
            }
        }
        plat_sleep_ms(POLL_MS);
    }
}

/* The saved networks: names only, never the passwords. D forgets the chosen one
 * after a Y/N question. */
static void saved_screen(void)
{
    unsigned chosen = 0, i;
    bool redraw = true;

    saved_load();
    for (;;) {
        uint8_t key;

        if (redraw) {
            char line[TCOLS + 1];

            redraw = false;
            screen_clear();
            snprintf(line, sizeof line, "SAVED NETWORKS  %u", saved_count);
            put_at(0, SAVED_ROW_HEADING, line, COL_CYAN);
            for (i = 0; i < saved_count; i++) {
                put_at(0, SAVED_ROW_FIRST + (int)i, i == chosen ? ">" : " ", COL_GREEN);
                put_at(2, SAVED_ROW_FIRST + (int)i, saved[i].ssid, i == chosen ? COL_CYAN : COL_WHITE);
            }
            if (saved_count == 0)
                put_row(SAVED_ROW_FIRST, "NO SAVED NETWORKS", COL_YELLOW);
            if (saved_problem != NULL)
                put_row(SAVED_ROW_MESSAGE, saved_problem, COL_YELLOW);
            put_row(SAVED_ROW_HELP, SAVED_HELP, COL_WHITE);
        }
        key = wait_any_key();
        if (key == KEY_ESC)
            return;
        if (key == KEY_UP && chosen > 0) {
            chosen--;
            redraw = true;
        } else if (key == KEY_DOWN && chosen + 1 < saved_count) {
            chosen++;
            redraw = true;
        } else if ((key == 'd' || key == 'D') && saved_count > 0) {
            char line[TCOLS + 1];

            snprintf(line, sizeof line, "FORGET %.26s? Y/N", saved[chosen].ssid);
            put_row(SAVED_ROW_MESSAGE, line, COL_YELLOW);
            key = wait_any_key();
            if (key == 'y' || key == 'Y') {
                for (i = chosen; i + 1 < saved_count; i++)
                    saved[i] = saved[i + 1];
                saved_count--;
                if (chosen >= saved_count && chosen > 0)
                    chosen--;
                (void)saved_store();
            }
            redraw = true;
        }
    }
}
