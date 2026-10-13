/*
 * netthing_const.h - every constant of PicoCalc NetThing: version, colours, limits,
 * screen positions and tables. netthing.c holds only code, so a new constant or
 * a moved row goes here.
 *
 * Author: Thomas Dzubin
 */
#ifndef NETTHING_CONST_H
#define NETTHING_CONST_H

#include "chip_limits.h"

/* Shown on the splash screen; the firmware build reads it from this line, so
 * keep it in the form  #define VERSION "V0.01A"  on one line. */
#define VERSION "V1.00A"

/* ------------------------------------------------------------------ */
/*  Colours (full-strength only: dim greys are hard to read on the LCD) */
/* ------------------------------------------------------------------ */
#define COL_BG      RGB(  0,   0,   0)
#define COL_WHITE   RGB(235, 235, 235)
#define COL_RED     RGB(255,  90,  90)
#define COL_GREEN   RGB( 90, 235, 130)
#define COL_YELLOW  RGB(255, 225,  70)
#define COL_CYAN    RGB(120, 225, 255)
#define COL_MAGENTA RGB(255, 120, 255)
#define COL_ORANGE  RGB(255, 165,  60)

/* ------------------------------------------------------------------ */
/*  Screen geometry: the LCD is a 40 x 32 grid of 8 x 10 pixel cells    */
/* ------------------------------------------------------------------ */
#define TCOLS    40

/* ------------------------------------------------------------------ */
/*  Pictures (gfx.c)                                                    */
/* ------------------------------------------------------------------ */
#define GFX_SCREEN          320     /* the LCD is 320 x 320 pixels */
#define SIGNAL_TOP_DBM      (-30)   /* a signal this strong fills a signal picture */
#define SIGNAL_BOTTOM_DBM   (-95)   /* one this weak or weaker is empty */
#define GFX_SCALES { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 0 }   /* the tallest bar a chart can have */

/* ------------------------------------------------------------------ */
/*  Splash screen rows                                                  */
/* ------------------------------------------------------------------ */
#define SPLASH_ROW_TITLE     8
#define SPLASH_ROW_TAGLINE_1 10
#define SPLASH_ROW_TAGLINE_2 11
#define SPLASH_ROW_VERSION  14
#define SPLASH_ROW_AUTHOR   16
#define SPLASH_ROW_PROMPT   22
#define SPLASH_ROW_EXIT     23
#define SPLASH_TAGLINE_1    "Wifi and Bluetooth network tools"     /* 40 columns hold only 40 characters, so two lines */
#define SPLASH_TAGLINE_2    "for troubleshooting"

/* ------------------------------------------------------------------ */
/*  Scan screen                                                         */
/*                                                                      */
/*  row:  "> HomeNet-5G       ####-  -52 11 WPA2"                       */
/*         marker, name, signal bars, dBm, channel, security            */
/* ------------------------------------------------------------------ */
#define SCAN_ROW_HEADING     0      /* "SCAN  12 FOUND         R RESCAN" */
#define SCAN_ROW_FIRST       2      /* first access point row            */
#define SCAN_ROWS           26      /* access points visible at once     */
#define SCAN_ROW_MESSAGE    29      /* what the radio is doing           */
#define BOOTSEL_NOTICE_ROW  29      /* the REBOOTING TO BOOTSEL notice   */
#define SCAN_ROW_HELP       30      /* two help lines                    */
#define SCAN_ROW_HELP_2     31
#define SCAN_HELP_1         "UP DOWN  ENTER DETAILS  C CONNECT  B BT"
#define SCAN_HELP_2         "M NAME  S SAVED  I STATUS  R SCAN  ESC"

#define SCAN_COL_MARKER      0
#define SCAN_COL_NAME        2
#define SCAN_NAME_WIDTH     17      /* longer names are cut              */
#define SCAN_COL_BARS       20
#define SCAN_COL_DBM        26      /* four characters wide              */
#define SCAN_COL_CHANNEL    31      /* two characters wide               */
#define SCAN_COL_SECURITY   34      /* up to five characters             */
#define SCAN_HIDDEN_NAME    "<hidden>"

/* The message when there is no wireless chip (a plain Pico or Pico 2, or a chip that does not answer) */
#define NOCHIP_ROW          10
#define NOCHIP_LINE_1       "NO WIRELESS CHIP FOUND"
#define NOCHIP_LINE_2       "THIS PROGRAM NEEDS A RASPBERRY PI"
#define NOCHIP_LINE_3       "PICO W OR PICO 2 W. A PLAIN PICO OR"
#define NOCHIP_LINE_4       "PICO 2 HAS NO WI-FI OR BLUETOOTH."
#define NOCHIP_LINE_5       "ESC GOES BACK"

/* Signal bars: five cells of BAR_FULL or BAR_EMPTY. A signal at or above
 * the n-th limit (strongest first) earns that many bars. */
#define BAR_CELLS            5
#define BAR_FULL            '#'
#define BAR_EMPTY           '-'
#define BAR_LIMITS          { -50, -60, -70, -80, -90 }   /* dBm for 5, 4, 3, 2, 1 bars */
#define BARS_GOOD            4      /* this many bars or more: green       */
#define BARS_FAIR            3      /* this many: yellow; fewer: red       */

/* ------------------------------------------------------------------ */
/*  Detail screen                                                       */
/* ------------------------------------------------------------------ */
#define DETAIL_ROW_HEADING   2
#define DETAIL_ROW_NAME      5      /* label; the name itself is on the next row */
#define DETAIL_ROW_NAME_VALUE 6
#define DETAIL_ROW_BSSID     8
#define DETAIL_ROW_CHANNEL  10
#define DETAIL_ROW_SIGNAL   12
#define DETAIL_ROW_SECURITY 14
#define DETAIL_ROW_SAVED    16
#define DETAIL_ROW_HELP     29
#define DETAIL_COL_LABEL     2
#define DETAIL_COL_VALUE    12
#define DETAIL_HELP         "C CONNECT   ESC BACK"
#define DETAIL_SAVED_YES    "YES (PASSWORD ON THE SD CARD)"
#define DETAIL_SAVED_NO     "NO"
#define DETAIL_SAVED_NO_SD  "NO (NO SD CARD)"

/* Security names: short ones for the list row, longer ones for the details */
#define SEC_SHORT_OPEN      "OPEN"
#define SEC_SHORT_WEP       "WEP"
#define SEC_SHORT_WPA       "WPA"
#define SEC_SHORT_WPA2      "WPA2"
#define SEC_SHORT_MIXED     "WPA/2"
#define SEC_LONG_OPEN       "OPEN, NO PASSWORD"
#define SEC_LONG_WEP        "WEP"
#define SEC_LONG_WPA        "WPA"
#define SEC_LONG_WPA2       "WPA2 (MAYBE WPA3)"
#define SEC_LONG_MIXED      "WPA AND WPA2"

/* Signal descriptions by bars, from none or one bar up to five */
#define SIGNAL_WORDS        { "VERY WEAK", "WEAK", "FAIR", "GOOD", "EXCELLENT" }

/* Channel to frequency: channels 1 to 13 are 2407 + 5 * channel MHz; 14 is 2484 */
#define FREQ_BASE_MHZ     2407
#define FREQ_STEP_MHZ        5
#define FREQ_CHANNEL_14   2484

/* ------------------------------------------------------------------ */
/*  Typing a name or a password                                         */
/* ------------------------------------------------------------------ */
#define PROMPT_ROW_HEADING   2
#define PROMPT_ROW_NAME      5      /* the network the password is for   */
#define PROMPT_ROW_LABEL     9
#define PROMPT_ROW_INPUT    11
#define PROMPT_ROW_NOTE     14      /* a refusal, such as too short      */
#define PROMPT_ROW_HELP     29
#define PROMPT_COL_LEFT      2
#define PROMPT_WIDTH        (TCOLS - 2 * PROMPT_COL_LEFT - 2)   /* characters visible in the entry field */
#define PROMPT_CURSOR       '_'
#define PROMPT_MASK         '*'
#define PROMPT_HELP_HIDDEN  "ENTER OK  TAB SHOW  ESC CANCEL"
#define PROMPT_HELP_SHOWN   "ENTER OK  TAB HIDE  ESC CANCEL"
#define PROMPT_HELP_NAME    "ENTER OK  ESC CANCEL"
#define PASS_MIN             8      /* shortest WPA passphrase           */
#define TILDE_DOUBLE_MS    700      /* two tildes this close reboot to BOOTSEL */

/* ------------------------------------------------------------------ */
/*  Joining a network                                                   */
/* ------------------------------------------------------------------ */
#define JOIN_ROW_HEADING     2
#define JOIN_ROW_NAME        6
#define JOIN_ROW_MESSAGE    10
#define JOIN_ROW_TRY        12
#define JOIN_ROW_NOTE       14
#define JOIN_ROW_HELP       29
#define JOIN_SAVE_ROW       12      /* "SAVE THIS NETWORK? Y/N"          */

/* The status screen of a connection */
#define STATUS_ROW_HEADING   2
#define STATUS_ROW_NAME      5
#define STATUS_ROW_SIGNAL    7
#define STATUS_ROW_IP        9
#define STATUS_ROW_MASK     10
#define STATUS_ROW_GATEWAY  11
#define STATUS_ROW_DNS      12
#define STATUS_ROW_MAC      14
#define STATUS_ROW_TIME     16
#define STATUS_ROW_IP6      18      /* label, the global IPv6 address on the next row */
#define STATUS_ROW_IP6_LL   20      /* label, the link-local address on the next row */
#define STATUS_COL_IP6       0      /* an IPv6 address can be 39 characters */
#define STATUS_IP6_NONE     "NONE YET"
#define STATUS_ROW_HELP_2   30
#define STATUS_ROW_HELP     29
#define STATUS_COL_LABEL     2
#define STATUS_COL_VALUE    12
#define STATUS_HELP         "D DISCONNECT   ESC BACK"
#define STATUS_HELP_2       "H HEALTH  R ROUTER  T TOOLS  B BLUETOOTH"
#define STATUS_HELP_3       ""
#define STATUS_ROW_HELP_3   31

/* The list of saved networks */
#define SAVED_ROW_HEADING    0
#define SAVED_ROW_FIRST      2
#define SAVED_ROW_MESSAGE   29
#define SAVED_ROW_HELP      31
#define SAVED_HELP          "UP DOWN  D FORGET  ESC BACK"
#define SAVED_FILE          "/netthing/WIFI.DAT"    /* scrambled names and passwords */

/* ------------------------------------------------------------------ */
/*  Network tools (tools.c)                                             */
/* ------------------------------------------------------------------ */
#define TOOL_ROW_HEADING     2
#define TOOL_ROW_FIRST       5      /* first line of a tool's output     */
#define TOOL_ROW_LAST       27      /* last line of a tool's output      */
#define TOOL_ROW_HELP       29
#define TOOL_COL             2
#define TOOL_WRAP           36      /* longest output line               */
#define TOOL_HOST_MAX       63
#define TOOL_LINE_MAX       96      /* room to build a line before it is cut to TOOL_WRAP */
#define SETTINGS_PATH       "/netthing/SETTINGS.TXT"    /* last hosts, port, offset */

#define RESOLVE_TIMEOUT_MS  8000    /* a name lookup that gets no answer */
#define PING_COUNT             4
#define PING_TIMEOUT_MS     2000    /* one echo request with no reply    */
#define PING_INTERVAL_MS    1000    /* between the starts of two requests */
#define TCP_TIMEOUT_MS      8000    /* connecting, or waiting for a first reply */
#define TCP_QUIET_MS        1000    /* a reply this long quiet is taken as complete */
#define NTP_TRIES              3
#define NTP_TIMEOUT_MS      3000
#define SERVER_DEFAULT_MESSAGE "HELLO FROM THE PICOCALC"
#define TOOL_AGAIN_HELP     "PRESS ANY KEY"

/* ------------------------------------------------------------------ */
/*  The web tool (fetch.c)                                              */
/* ------------------------------------------------------------------ */
#define CA_FILE             "/netthing/CA.PEM"      /* the trusted root certificates for https */
#define CA_MAX              CHIP_CA_MAX   /* the most of that file that is read */
#define FETCH_CA_PIECE       2048   /* bytes of the built-in certificates written at a time */
#define DOWNLOAD_DIR        "/netthing/DOWNLOAD/"   /* where the certificate tool puts its downloads */
#define MAX_REDIRECTS           5
#define FETCH_CHUNK          1460   /* bytes taken from the connection at a time */
#define FETCH_CONNECT_TIMEOUT_MS 30000      /* connecting and the TLS handshake */
#define FETCH_IDLE_TIMEOUT_MS    20000      /* no data at all for this long */
#define FETCH_UPDATE_MS       500
#define FETCH_USER_AGENT    "PicoCalc-NetThing"
/* ------------------------------------------------------------------ */
/*  The tools menu (menu.c)                                             */
/* ------------------------------------------------------------------ */
#define MENU_ROW_FIRST       4
#define MENU_ROW_STEP        1
#define MENU_COL             4
#define MENU_HELP           "PRESS THE LETTER   ESC BACK"
#define MENU_KEYS           "HPRCMTOLDNSBGWUVK"
#define MENU_LABELS { \
    "IS IT WORKING? (HEALTH CHECK)", "PING A HOST", "PING THE ROUTER", "CONTINUOUS PING AND SIGNAL", \
    "SIGNAL METER", "TRACE ROUTE", \
    "PORT CHECK", "LAN SCAN", "DNS CHECKER", "NAME LOOKUP", "CHANNEL SURVEY", "SPEED TEST", \
    "WEB PAGE CHECK", "SET THE CLOCK (NTP)", "UPDATE THE CERTIFICATES", \
    "WINDOWS TEST SERVER TOOLS...", "BLUETOOTH TOOLS...", NULL }

/* The menu of the tools that need the Windows test server (srvtools.c) */
#define SERVER_MENU_ROW_FIRST  7
#define SERVER_MENU_KEYS    "TDUL"
#define SERVER_MENU_LABELS { "TALK TO THE SERVER", "DOWNLOAD SPEED", "UPLOAD SPEED", "LATENCY TEST", NULL }

/* ------------------------------------------------------------------ */
/*  Continuous ping with a graph (pinglive.c)                           */
/* ------------------------------------------------------------------ */
#define LIVE_SAMPLES        100     /* bars on the graph, newest at the right */
#define LIVE_BAR_W            2     /* pixels */
#define LIVE_BAR_STEP         3
#define LIVE_GRAPH_X         10     /* left edge, pixels */
#define LIVE_GRAPH_Y         70     /* top, pixels */
#define LIVE_GRAPH_H        100     /* height, pixels */
#define LIVE_ROW_HOST         1
#define LIVE_ROW_SIGNAL       3
#define LIVE_ROW_SCALE        5
#define LIVE_ROW_STATS       18     /* four lines from here */
#define LIVE_ROW_LOG         23
#define LIVE_ROW_HELP        29
#define LIVE_LOST        0xFFFF     /* a sample for a ping with no answer */
#define LIVE_GOOD_MS         50     /* a green bar up to here, yellow up to LIVE_FAIR_MS, red above */
#define LIVE_FAIR_MS        150
#define LIVE_SCALES { 10, 20, 50, 100, 200, 500, 1000, 2000, 0 }   /* the tallest bar, in ms */
#define LIVE_LOG_FILE       "/netthing/PINGLOG.CSV"
#define LIVE_LOG_HEADER     "count,seconds,ms,dbm,unix_time\r\n"
#define LIVE_LOG_BUF        512
#define LIVE_HELP           "L LOG  C CLEAR  ESC STOP"

/* The bars under the four pings of P and R (tools.c) */
#define PINGBARS_X           60
#define PINGBARS_Y          150     /* top, pixels */
#define PINGBARS_H           70
#define PINGBARS_W           30
#define PINGBARS_STEP        50
#define PINGBARS_ROW_LABEL   23

/* The histogram of the latency test (srvtools.c): bands under 5 ms, 5 to 9, 10 to 19, 20 to 49, 50 to 99, 100 and over */
#define LATHIST_BANDS         6
#define LATHIST_LIMITS { 5, 10, 20, 50, 100 }
#define LATHIST_NAMES  { "<5", "<10", "<20", "<50", "<100", "100+" }
#define LATHIST_COLOURS { COL_GREEN, COL_GREEN, COL_YELLOW, COL_YELLOW, COL_RED, COL_RED }
#define LATHIST_X            22
#define LATHIST_Y           180     /* top, pixels */
#define LATHIST_H            70
#define LATHIST_W            34
#define LATHIST_STEP         46
#define LATHIST_ROW_TITLE    16
#define LATHIST_ROW_NAMES    26

/* ------------------------------------------------------------------ */
/*  Signal meter (meter.c) and the signal strip of the status screen    */
/* ------------------------------------------------------------------ */
#define METER_MS            300     /* between two readings */
#define METER_SAMPLES        50     /* the strip below the bar: about 15 seconds */
#define METER_STEP            6     /* pixels per sample */
#define METER_X              10
#define METER_Y              50     /* the bar, pixels */
#define METER_W             300
#define METER_H              40
#define METER_TICKS { -90, -80, -70, -60, -50, -40 }
#define METER_ROW_NAME        2
#define METER_ROW_TICKS      10
#define METER_ROW_NUMBER     13
#define METER_ROW_BEST       15
#define METER_ROW_HINT       17     /* WARMER, COLDER */
#define METER_STRIP_Y       190     /* pixels */
#define METER_STRIP_H        70
#define METER_ROW_HELP       30
#define SIGHIST_SAMPLES     140     /* the status screen's strip of the signal: one sample for each update */
#define SIGHIST_STEP          2
#define SIGHIST_X            20
#define SIGHIST_Y           236     /* top, pixels */
#define SIGHIST_H            36
#define SIGHIST_ROW_LABEL    22

/* ------------------------------------------------------------------ */
/*  Health check (health.c)                                             */
/* ------------------------------------------------------------------ */
#define HEALTH_NAMES { "WI-FI SIGNAL", "THE ROUTER", "DNS (NAMES TO ADDRESSES)", "THE INTERNET (PING)", \
                       "THE INTERNET (CONNECTION)", "IPV6" }
#define HEALTH_REASON_MAX    32
#define HEALTH_X             12     /* the light, pixels */
#define HEALTH_Y             34
#define HEALTH_STEP          40     /* pixels between two checks */
#define HEALTH_LIGHT         26
#define HEALTH_TEXT_COL       6
#define HEALTH_ROW_SUMMARY   28
#define HEALTH_ROW_HELP      31
#define HEALTH_PINGS          3
#define HEALTH_PING_TIMEOUT_MS 1500
#define HEALTH_PING_GAP_MS  300
#define HEALTH_ROUTER_GOOD_MS 30    /* an average up to this is green */
#define HEALTH_NET_GOOD_MS   100
#define HEALTH_DNS_GOOD_MS   150
#define HEALTH_TCP_GOOD_MS   400
#define HEALTH_TCP_TIMEOUT_MS 3000
#define HEALTH_TCP_PORT      443
#define HEALTH_TARGET        "1.1.1.1"
#define HEALTH_NAME          "example.com"

/* ------------------------------------------------------------------ */
/*  Bluetooth tools (btmenu.c, btscan.c, btfind.c, btinspect.c, bttrack.c, btcrowd.c) */
/* ------------------------------------------------------------------ */
#define BT_START_TIMEOUT_MS 15000   /* the radio's firmware did not load */
#define BTMENU_ROW_FIRST      5
#define BTMENU_ROW_STEP       2
#define BTMENU_ROW_STATUS    15
#define BTMENU_KEYS         "SFICTX"
#define BTMENU_LABELS { "NEARBY DEVICES (LOW ENERGY)", "FIND ONE BY ITS SIGNAL", "READ A DEVICE (CONNECT)", \
                        "CLASSIC DEVICES (INQUIRY)", "TRACKER ALERT", "2.4 GHZ CROWDING REPORT", NULL }
#define BTL_ROW_HEADING       0
#define BTL_ROW_FIRST         2
#define BTL_ROWS             26
#define BTL_ROW_HELP         30     /* two lines */
#define BTL_NAME_W           15
#define BTL_COL_BARS         18
#define BTL_COL_DBM          24
#define BTL_COL_KIND         29
#define BTL_REFRESH_MS      500
#define BTL_STALE_MS      15000     /* heard longer ago than this: shown in yellow */
#define BTL_HIDE_MS       60000     /* ... and longer than this: not shown */
#define BTC_SECONDS           8     /* one Classic inquiry */
#define BTD_ROW_FIRST         5     /* the details of a device */
#define BTD_ROW_LAST         26
#define BTD_ROW_NOTE         27
#define BTD_ROW_HELP         30
#define BTD_COL_LABEL         2
#define BTD_COL_VALUE        11
#define BTF_TREND             3     /* readings averaged for WARMER and COLDER (twice this many are kept) */
#define BTF_STEP_DB           1     /* the average must change by more than this per reading */
#define BTF_LOST_MS        3000     /* not heard for this long: NOT HEARD */
#define BTI_TIMEOUT_MS    25000     /* connecting and reading a device */
#define BTI_BATTERY_GOOD     40
#define BTI_BATTERY_LOW      15
#define BTI_BATTERY_W       150     /* pixels */
#define BTT_ROW_HEADING       0
#define BTT_ROW_FIRST         2
#define BTT_ROWS             14
#define BTT_ROW_NOTE         22
#define BTT_ROW_HELP         30
#define BTT_REFRESH_MS     1000
#define BTT_HIDE_MS      180000     /* a tag not heard for this long is dropped from the list */
#define BTT_STALE_MS      20000
#define BTT_WARN_S          300     /* in range this many seconds: yellow, WATCH */
#define BTT_ALERT_S         900     /* ... and this many: red */
#define BTT_COL_BARS         13
#define BTT_COL_DBM          19
#define BTT_COL_TIME         24
#define BTT_COL_FLAG         31
#define BTX_SECONDS          10     /* the crowding report listens this long */
#define BTX_CHANNELS         13
#define BTX_REACH             2     /* channels either side that a Wi-Fi network overlaps */
#define BTX_STRONG_DBM     (-70)    /* a Bluetooth device stronger than this counts as close */
#define BTX_BUSY_SCORE        8
#define BTX_CROWDED_SCORE    16
#define BTX_GRAPH_X          17
#define BTX_GRAPH_Y          40     /* top, pixels */
#define BTX_GRAPH_H         100
#define BTX_STEP             22
#define BTX_BAR_W            18
#define BTX_ROW_LABEL        15
#define BTX_ROW_TEXT         17
#define BTX_ROW_BAR          21     /* a bar for the score, on this text row */
#define BTX_BAR_LENGTH      280
#define BTX_ROW_VERDICT      23
#define BTX_ROW_HELP         29

/* ------------------------------------------------------------------ */
/*  Trace route (trace.c)                                               */
/* ------------------------------------------------------------------ */
#define TRACE_MAX_HOPS       30
#define TRACE_PROBES          3     /* requests per hop */
#define TRACE_TIMEOUT_MS   1500
#define TRACECHART_ROW_FIRST  1     /* the row of hop 1 on the chart */
#define TRACECHART_X         24     /* where the bars start, pixels */
#define TRACECHART_W        200     /* the longest bar */
#define TRACECHART_H          6
#define TRACE_SILENT_STOP     6     /* give up after this many hops in a row with no answer at all */

/* ------------------------------------------------------------------ */
/*  Port check (ports.c)                                                */
/* ------------------------------------------------------------------ */
#define FETCH_MODE_CHECK      0     /* read the headers only, then close */
#define FETCH_MODE_SAVE       1     /* write the body to a file */
#define FETCH_MODE_SPEED      2     /* read the body and throw it away, timing it */
#define DNS_SERVERS_MAX     (1 + DNS_PUBLIC_COUNT)     /* the router's and the public ones */
#define LIGHT_PENDING         0     /* the lights of the health check */
#define LIGHT_GREEN           1
#define LIGHT_YELLOW          2
#define LIGHT_RED             3
#define LIGHT_INFO            4
#define CHECK_SIGNAL          0     /* and its checks, in the order they run */
#define CHECK_ROUTER          1
#define CHECK_DNS             2
#define CHECK_INTERNET        3
#define CHECK_TCP             4
#define CHECK_IPV6            5
#define CHECKS                6
#define PORTS_TIMEOUT_MS   2000     /* no answer by then: the port is filtered or the host is off */
#define PORTS_OPEN            0     /* what a port gave (for the grid) */
#define PORTS_CLOSED          1
#define PORTS_SILENT          2
#define PORTGRID_COLUMNS     12
#define PORTGRID_X           20     /* the top left square, pixels */
#define PORTGRID_Y           50
#define PORTGRID_SQUARE      20
#define PORTGRID_STEP        25
#define PORTGRID_ROW_LEGEND  22
#define PORTGRID_ROW_INFO    25

/* ------------------------------------------------------------------ */
/*  LAN scan (lan.c)                                                    */
/* ------------------------------------------------------------------ */
#define ARP_COPY_MAX         24     /* the size of the ARP table in lwipopts.h */
#define LAN_BATCH             8     /* ARP requests sent, then the answers collected */
#define LAN_BATCH_WAIT_MS   150     /* for the answers to a batch */
#define LAN_LAST_WAIT_MS    600     /* after the last batch */
#define LAN_PING_TIMEOUT_MS 600
#define LAN_ROW_HEADING       0
#define LAN_ROW_COLUMNS       2
#define LAN_ROW_FIRST         3
#define LAN_ROWS             25     /* devices visible at once */
#define LAN_ROW_PROGRESS     29
#define LAN_ROW_HELP         30
#define LAN_ROW_LEGEND       31
#define LAN_COL_MARK          0
#define LAN_COL_IP            2
#define LAN_COL_MAC          18
#define LAN_COL_PING         36
#define LAN_COLUMNS         "  IP ADDRESS      MAC             PING"
#define LAN_HELP            "UP DOWN SCROLL  M MAP  R RESCAN  ESC"
#define LAN_LEGEND          "R=ROUTER  T=THIS ONE  CYAN MAC=PRIVATE"
#define LAN_COL_PRIVATE     COL_CYAN
#define LANMAP_BIG_AFTER    256     /* more addresses than this get the small squares, 32 to a row */
#define LANMAP_BIG           14     /* the size of a square, pixels */
#define LANMAP_SMALL          6
#define LANMAP_X             16
#define LANMAP_Y             24
#define LANMAP_ROW_INFO      28

/* ------------------------------------------------------------------ */
/*  DNS checker (dnscheck.c)                                            */
/* ------------------------------------------------------------------ */
#define DNS_PORT               53
#define DNS_TIMEOUT_MS       2000   /* one question with no answer */
#define DNS_TRIES               3   /* questions for the address of one server */
#define DNS_SLOW_MS            50   /* a server must average more than this, and more than DNS_SLOW_FACTOR */
#define DNS_SLOW_FACTOR         3   /* times the fastest public one, to be called slow */
#define DNS_PUBLIC_COUNT        3
#define DNS_PUBLIC_LABELS { "CLOUDFLARE", "GOOGLE", "QUAD9" }
#define DNS_PUBLIC_ADDRS  { "1.1.1.1", "8.8.8.8", "9.9.9.9" }

/* The chart of the DNS answer times (dnscheck.c) */
#define DNSBARS_X            90     /* where the bars start, pixels */
#define DNSBARS_W           160     /* the longest bar */
#define DNSBARS_H            16
#define DNSBARS_Y            60     /* the top of the first bar, pixels */
#define DNSBARS_STEP         40
#define DNSBARS_ROW_NOTE     26

/* ------------------------------------------------------------------ */
/*  Speed test and the server tools (fetch.c, srvtools.c)               */
/* ------------------------------------------------------------------ */
#define SPEED_MARK_MS         500   /* the interval of the live figure */
#define SPEED_MAX_MS        10000   /* the test ends after this long, ... */
#define SPEED_MAX_BYTES  (8u * 1024u * 1024u)   /* ... or after this much data */
#define SPEED_MIN_MS          500   /* less than this is too short to measure */
#define SPEED_SERVER_PATH   "/stream"
#define SPEED_BARS           22     /* one bar for each SPEED_MARK_MS of the test */
#define SPEED_GRAPH_X         6
#define SPEED_GRAPH_Y        60     /* top, pixels */
#define SPEED_GRAPH_H        90
#define SPEED_BAR_W          12
#define SPEED_BAR_STEP       14
#define SPEED_ROW_LABEL      16
#define SPEED_ROW_RESULT     18
#define SPEED_UPLOAD_PATH   "/upload"
#define UPLOAD_MS           10000
#define UPLOAD_MAX_BYTES (16u * 1024u * 1024u)
#define UPLOAD_CHUNK         1460
#define UPLOAD_REPLY_MS      8000   /* how long to wait for the server's count */
#define LATENCY_COUNT         100
#define LATENCY_TIMEOUT_MS   1000
#define LATENCY_GAP_MS         20
#define LATENCY_SLOW_MS       100   /* an answer slower than this is counted as slow */

/* ------------------------------------------------------------------ */
/*  Channel survey (survey.c)                                           */
/* ------------------------------------------------------------------ */
#define SURVEY_CHANNELS      13
#define SURVEY_BAR_W         18     /* pixels */
#define SURVEY_BAR_STEP      22
#define SURVEY_GRAPH_X       17
#define SURVEY_GRAPH_Y       60     /* top, pixels */
#define SURVEY_GRAPH_H      120
#define SURVEY_ROW_NOTE       3
#define SURVEY_ROW_LABEL     19     /* the channel numbers */
#define SURVEY_ROW_COUNT     20     /* how many networks on each */
#define SURVEY_ROW_RESULT    22
#define SURVEY_ROW_HELP      29
#define SURVEY_REACH          4     /* channels away that still overlap */
#define SURVEY_FLOOR        (-95)   /* signal that counts for nothing in the crowding score */
#define SURVEY_HELP         "R RESCAN  V BARS OR HILLS  ESC BACK"
#define SURVEY_ROW_LEGEND    25     /* the names of the strongest, in the overlap view */
#define SURVEY_LEGEND_ROWS    3
#define SURVEY_PALETTE { COL_CYAN, COL_GREEN, COL_MAGENTA, COL_ORANGE, COL_WHITE, COL_RED }

/* ------------------------------------------------------------------ */
/*  Timing                                                              */
/* ------------------------------------------------------------------ */
#define POLL_MS         3       /* pause between keyboard polls          */
#define REBOOT_MS     300       /* time the BOOTSEL notice stays visible */
#define SCAN_UPDATE_MS 250      /* how often the "SCANNING" line changes */
#define SCAN_TIMEOUT_MS 20000   /* give up on a scan that never ends     */
#define JOIN_TIMEOUT_MS 25000   /* give up on one way of joining         */
#define JOIN_UPDATE_MS  250     /* how often the joining screen changes  */
#define STATUS_UPDATE_MS 1500   /* how often the status screen refreshes */
#define NOTICE_MS      1800     /* how long a short notice stays up      */

#endif /* NETTHING_CONST_H */
