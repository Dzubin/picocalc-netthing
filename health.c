/*
 * health.c - "is it working?": a handful of checks one after another, each with
 * a light (green, yellow or red) and a one-line reason, and then a plain
 * sentence about what is probably wrong. The checks go from the radio outwards:
 * the Wi-Fi signal, the router, the DNS server, the internet (a ping and then a
 * TCP connection to a public address), and whether IPv6 is there.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "wifi.h"
#include "link.h"
#include "net.h"
#include "dnsquery.h"
#include "gfx.h"
#include "ui.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

static int  light[CHECKS];
static char reason[CHECKS][HEALTH_REASON_MAX + 16];

static void show_check(unsigned n)
{
    static const char *const names[CHECKS] = HEALTH_NAMES;
    int y = HEALTH_Y + (int)n * HEALTH_STEP;
    uint16_t colour;

    gfx_rect(COL_BG, HEALTH_X - 2, y - 2, HEALTH_LIGHT + 4, HEALTH_LIGHT + 4);
    switch (light[n]) {
    case LIGHT_GREEN:  colour = COL_GREEN;  break;
    case LIGHT_YELLOW: colour = COL_YELLOW; break;
    case LIGHT_RED:    colour = COL_RED;    break;
    case LIGHT_INFO:   colour = COL_CYAN;   break;
    default:           colour = COL_WHITE;  break;
    }
    if (light[n] == LIGHT_PENDING)
        gfx_frame(colour, HEALTH_X, y, HEALTH_LIGHT, HEALTH_LIGHT);
    else
        gfx_rect(colour, HEALTH_X, y, HEALTH_LIGHT, HEALTH_LIGHT);
    clear_row(y / 10);
    clear_row(y / 10 + 1);
    put_at(HEALTH_TEXT_COL, y / 10, names[n], COL_WHITE);
    put_at(HEALTH_TEXT_COL, y / 10 + 1, reason[n], light[n] == LIGHT_RED ? COL_YELLOW : COL_CYAN);
}

static void set(unsigned n, int colour, const char *text)
{
    light[n] = colour;
    snprintf(reason[n], sizeof reason[n], "%s", text);
    show_check(n);
}

/* Ping an address `count` times; the average time and the number lost. Returns false if ESC was pressed. */
static bool ping_stats(const char *addr, unsigned count, unsigned *average, unsigned *lost)
{
    unsigned i, total = 0, got = 0;

    for (i = 0; i < count; i++) {
        unsigned ms = 0;
        bool stopped;

        if (tools_ping_once(addr, HEALTH_PING_TIMEOUT_MS, &ms, &stopped) == NET_DONE) {
            total += ms;
            got++;
        }
        if (stopped || (i + 1 < count && tools_wait_or_escape(HEALTH_PING_GAP_MS)))
            return false;
    }
    *average = got ? total / got : 0;
    *lost = count - got;
    return true;
}

static void judge_ping(unsigned n, unsigned average, unsigned lost, unsigned good_ms, const char *blocked_note)
{
    char text[HEALTH_REASON_MAX + 16];

    if (lost == HEALTH_PINGS) {
        set(n, LIGHT_RED, blocked_note);
    } else if (lost > 0) {
        snprintf(text, sizeof text, "%u OF %u LOST, AVERAGE %u MS", lost, HEALTH_PINGS, average);
        set(n, LIGHT_YELLOW, text);
    } else if (average > good_ms) {
        snprintf(text, sizeof text, "SLOW: AVERAGE %u MS", average);
        set(n, LIGHT_YELLOW, text);
    } else {
        snprintf(text, sizeof text, "ANSWERS IN %u MS", average);
        set(n, LIGHT_GREEN, text);
    }
}

static void summary(void)
{
    const char *one = "", *two = "";
    uint16_t colour = COL_GREEN;
    unsigned i;
    bool any_yellow = false;

    for (i = 0; i < CHECKS; i++)
        if (light[i] == LIGHT_YELLOW)
            any_yellow = true;
    if (light[CHECK_SIGNAL] == LIGHT_RED) {
        one = "THE WI-FI SIGNAL IS WEAK. MOVE CLOSER";
        two = "TO THE ACCESS POINT.";
        colour = COL_YELLOW;
    } else if (light[CHECK_ROUTER] == LIGHT_RED) {
        one = "THE ROUTER DOES NOT ANSWER: A WI-FI OR";
        two = "ROUTER PROBLEM, NOT THE INTERNET.";
        colour = COL_YELLOW;
    } else if (light[CHECK_INTERNET] == LIGHT_RED && light[CHECK_TCP] == LIGHT_RED) {
        one = "THE ROUTER IS FINE BUT THE INTERNET IS";
        two = "NOT REACHABLE: MODEM OR PROVIDER.";
        colour = COL_YELLOW;
    } else if (light[CHECK_DNS] == LIGHT_RED) {
        one = "NAMES DO NOT RESOLVE: A DNS PROBLEM.";
        two = "TRY THE DNS CHECKER IN THE TOOLS MENU.";
        colour = COL_YELLOW;
    } else if (any_yellow) {
        one = "IT WORKS, BUT SOMETHING IS SLOW OR";
        two = "LOSING PACKETS (SEE THE YELLOW LIGHTS).";
        colour = COL_YELLOW;
    } else {
        one = "EVERYTHING LOOKS GOOD.";
    }
    put_at(1, HEALTH_ROW_SUMMARY, one, colour);
    put_at(1, HEALTH_ROW_SUMMARY + 1, two, colour);
}

void tool_health(void)
{
    static const char *const waiting = "CHECKING...";
    wifi_info_t info;
    char text[HEALTH_REASON_MAX + 16];
    unsigned n, average, lost;
    unsigned bars;

    for (;;) {
        screen_clear();
        put_row(1, "IS IT WORKING?", COL_CYAN);
        put_row(HEALTH_ROW_HELP, "R CHECK AGAIN  ESC BACK", COL_WHITE);
        for (n = 0; n < CHECKS; n++)
            set(n, LIGHT_PENDING, n == 0 ? waiting : "");
        drain_keys();

        if (!link_info_get(&info)) {
            set(CHECK_SIGNAL, LIGHT_RED, "NOT CONNECTED");
            put_at(1, HEALTH_ROW_SUMMARY, "THE PICOCALC IS NOT CONNECTED.", COL_YELLOW);
            goto wait_key;
        }
        bars = bars_for(info.rssi);
        if (link_kind() == LINK_USB) {
            set(CHECK_SIGNAL, LIGHT_GREEN, "USB LINK UP");
        } else {
            snprintf(text, sizeof text, "%d DBM, %u OF %u BARS", (int)info.rssi, bars, (unsigned)BAR_CELLS);
            set(CHECK_SIGNAL, bars >= BARS_GOOD ? LIGHT_GREEN : (bars >= BARS_FAIR ? LIGHT_YELLOW : LIGHT_RED), text);
        }

        if (info.gateway[0] == '\0' || strcmp(info.gateway, "0.0.0.0") == 0) {
            set(CHECK_ROUTER, LIGHT_YELLOW, "NO ROUTER ADDRESS WAS GIVEN");
        } else {
            set(CHECK_ROUTER, LIGHT_PENDING, waiting);
            if (!ping_stats(info.gateway, HEALTH_PINGS, &average, &lost))
                return;
            judge_ping(CHECK_ROUTER, average, lost, HEALTH_ROUTER_GOOD_MS, "NO ANSWER FROM THE ROUTER");
        }

        set(CHECK_DNS, LIGHT_PENDING, waiting);
        {
            dnsq_answer_t answer;
            unsigned ms = 0;
            int r = 1;
            uint8_t packet[DNSQ_PACKET_MAX];
            uint16_t id = 0x7B00;
            size_t length = dnsq_build(HEALTH_NAME, DNSQ_TYPE_A, id, packet, sizeof packet), got = 0;
            uint32_t started = plat_now_ms();
            int state = NET_BUSY;

            if (length > 0 && info.dns[0] != '\0' && net_udp_start(info.dns, DNS_PORT, packet, length) == 0) {
                while (state == NET_BUSY && plat_now_ms() - started < DNS_TIMEOUT_MS) {
                    state = net_udp_poll(packet, sizeof packet, &got, &ms);
                    if (poll_key() == KEY_ESC) {
                        net_udp_stop();
                        return;
                    }
                    plat_sleep_ms(POLL_MS);
                }
                if (state == NET_DONE && dnsq_parse(packet, got, id, DNSQ_TYPE_A, &answer))
                    r = 0;
                net_udp_stop();
            }
            if (r != 0) {
                set(CHECK_DNS, LIGHT_RED, "THE DNS SERVER DOES NOT ANSWER");
            } else if (answer.rcode != DNSQ_RCODE_OK || answer.count == 0) {
                set(CHECK_DNS, LIGHT_YELLOW, "IT ANSWERS BUT HAS NO ADDRESS");
            } else if (ms > HEALTH_DNS_GOOD_MS) {
                snprintf(text, sizeof text, "SLOW: %u MS", ms);
                set(CHECK_DNS, LIGHT_YELLOW, text);
            } else {
                snprintf(text, sizeof text, "ANSWERS IN %u MS", ms);
                set(CHECK_DNS, LIGHT_GREEN, text);
            }
        }

        set(CHECK_INTERNET, LIGHT_PENDING, waiting);
        if (!ping_stats(HEALTH_TARGET, HEALTH_PINGS, &average, &lost))
            return;
        judge_ping(CHECK_INTERNET, average, lost, HEALTH_NET_GOOD_MS, "NO PING ANSWER (MAY BE BLOCKED)");

        set(CHECK_TCP, LIGHT_PENDING, waiting);
        {
            uint32_t started = plat_now_ms();
            unsigned ms = 0;
            int state = NET_BUSY;

            if (net_probe_start(HEALTH_TARGET, HEALTH_TCP_PORT) == 0) {
                while (state == NET_BUSY && plat_now_ms() - started < HEALTH_TCP_TIMEOUT_MS) {
                    state = net_probe_poll(&ms);
                    if (poll_key() == KEY_ESC) {
                        net_probe_stop();
                        return;
                    }
                    plat_sleep_ms(POLL_MS);
                }
            }
            net_probe_stop();
            if (state == NET_DONE) {
                snprintf(text, sizeof text, "CONNECTED IN %u MS", ms);
                set(CHECK_TCP, ms > HEALTH_TCP_GOOD_MS ? LIGHT_YELLOW : LIGHT_GREEN, text);
            } else {
                set(CHECK_TCP, LIGHT_RED, "NO CONNECTION TO THE INTERNET");
            }
        }

        if (info.ip6[0] != '\0') {
            snprintf(text, sizeof text, "%.*s", HEALTH_REASON_MAX, info.ip6);
            set(CHECK_IPV6, LIGHT_INFO, text);
        } else {
            set(CHECK_IPV6, LIGHT_INFO, "NO IPV6 ADDRESS (FINE)");
        }
        summary();
wait_key:
        for (;;) {
            uint8_t key = wait_any_key();

            if (key == KEY_ESC)
                return;
            if (key == 'r' || key == 'R')
                break;
        }
    }
}
