/*
 * dnscheck.c - DNS checker: ask the router's DNS server and three public ones
 * (Cloudflare, Google, Quad9) the same question, three times for the address
 * (A) and once for the IPv6 address (AAAA), and show what each answered and how
 * fast, then a plain verdict. The queries go straight to each server as UDP
 * packets (dnsquery.c builds and reads them), not through the resolver of the
 * network stack, which only asks the servers the access point gave.
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
#include "ui.h"
#include "gfx.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"

typedef struct {
    const char *label;
    char        addr[WIFI_IP6_MAX];
    bool        used;               /* there is such a server */
    bool        answered;           /* at least one of the A queries got a proper reply */
    int         rcode;              /* of the first reply */
    unsigned    best, total, worst, replies;
    unsigned    a_count;
    uint8_t     a_list[DNSQ_ADDRS_MAX][16];
    unsigned    aaaa_count;
} server_t;

static server_t servers[DNS_SERVERS_MAX];
static uint16_t query_id = 0x2A00;

/* One question to one server. Returns 0 with the reply in *answer and the time in *ms, 1 if there
 * was no proper reply in time, 2 if ESC was pressed. */
static int ask(const char *server, const char *name, uint16_t type, dnsq_answer_t *answer, unsigned *ms)
{
    uint8_t packet[DNSQ_PACKET_MAX];
    size_t length, got = 0;
    uint32_t started = plat_now_ms();
    int state = NET_BUSY, result = 1;
    uint16_t id = ++query_id;

    length = dnsq_build(name, type, id, packet, sizeof packet);
    if (length == 0 || net_udp_start(server, DNS_PORT, packet, length) != 0)
        return 1;
    while (state == NET_BUSY && plat_now_ms() - started < DNS_TIMEOUT_MS) {
        state = net_udp_poll(packet, sizeof packet, &got, ms);
        if (poll_key() == KEY_ESC) {
            net_udp_stop();
            return 2;
        }
        plat_sleep_ms(POLL_MS);
    }
    if (state == NET_DONE && dnsq_parse(packet, got, id, type, answer))
        result = 0;
    net_udp_stop();
    return result;
}

static const char *rcode_words(int rcode)
{
    switch (rcode) {
    case DNSQ_RCODE_NXDOMAIN: return "NAME NOT FOUND (NXDOMAIN)";
    case DNSQ_RCODE_SERVFAIL: return "SERVER FAILURE (SERVFAIL)";
    case DNSQ_RCODE_REFUSED:  return "REFUSED";
    default:                  return "AN ERROR ANSWER";
    }
}

/* Ask one server; false if ESC stopped it. */
static bool check_server(server_t *s, const char *name)
{
    unsigned i;
    dnsq_answer_t answer;
    unsigned ms = 0;
    int r;

    for (i = 0; i < DNS_TRIES; i++) {
        r = ask(s->addr, name, DNSQ_TYPE_A, &answer, &ms);
        if (r == 2)
            return false;
        if (r != 0) {
            if (i == 0)
                return true;            /* silent: no use asking twice more */
            continue;
        }
        if (s->replies == 0) {
            s->rcode = answer.rcode;
            s->a_count = answer.count;
            memcpy(s->a_list, answer.addr, sizeof s->a_list);
            s->best = s->worst = ms;
        }
        s->total += ms;
        if (ms < s->best)
            s->best = ms;
        if (ms > s->worst)
            s->worst = ms;
        s->replies++;
        s->answered = true;
    }
    r = ask(s->addr, name, DNSQ_TYPE_AAAA, &answer, &ms);
    if (r == 2)
        return false;
    if (r == 0)
        s->aaaa_count = answer.count;
    return true;
}

static void show_server(const server_t *s)
{
    char line[TOOL_LINE_MAX], text[DNSQ_TEXT_MAX];

    snprintf(line, sizeof line, "%s %s", s->label, s->addr);
    tools_out_page(line, COL_CYAN);
    if (!s->answered) {
        tools_out_page("  NO ANSWER (TIMEOUT)", COL_YELLOW);
        return;
    }
    if (s->rcode != DNSQ_RCODE_OK) {
        snprintf(line, sizeof line, "  %s", rcode_words(s->rcode));
        tools_out_page(line, COL_YELLOW);
    } else if (s->a_count == 0) {
        tools_out_page("  A: NO ADDRESS IN THE ANSWER", COL_YELLOW);
    } else {
        dnsq_format(DNSQ_TYPE_A, s->a_list[0], text, sizeof text);
        if (s->a_count > 1)
            snprintf(line, sizeof line, "  A %s +%u MORE", text, s->a_count - 1);
        else
            snprintf(line, sizeof line, "  A %s", text);
        tools_out_page(line, COL_GREEN);
    }
    snprintf(line, sizeof line, "  MIN %u  AVG %u  MAX %u MS", s->best, s->total / s->replies, s->worst);
    tools_out_page(line, COL_WHITE);
    if (s->aaaa_count > 0)
        snprintf(line, sizeof line, "  AAAA %u ADDRESS%s", s->aaaa_count, s->aaaa_count == 1 ? "" : "ES");
    else
        snprintf(line, sizeof line, "  AAAA NONE");
    tools_out_page(line, COL_WHITE);
}

/* Do two servers' answers have an address in common? */
static bool share_address(const server_t *a, const server_t *b)
{
    unsigned i, j;

    for (i = 0; i < a->a_count && i < DNSQ_ADDRS_MAX; i++)
        for (j = 0; j < b->a_count && j < DNSQ_ADDRS_MAX; j++)
            if (memcmp(a->a_list[i], b->a_list[j], 4) == 0)
                return true;
    return false;
}

static void verdict(void)
{
    const server_t *router = servers[0].used ? &servers[0] : NULL;
    const server_t *best = NULL;
    unsigned i, public_answered = 0, shared = 0;
    char line[TOOL_LINE_MAX];

    for (i = 1; i < DNS_SERVERS_MAX; i++) {
        const server_t *s = &servers[i];

        if (s->answered) {
            public_answered++;
            if (best == NULL || s->total / s->replies < best->total / best->replies)
                best = s;
        }
    }
    tools_out_page("", COL_WHITE);
    if (router == NULL || !router->answered) {
        if (public_answered == 0) {
            tools_out_page("NO DNS SERVER ANSWERS.", COL_YELLOW);
            tools_out_page("THE INTERNET OR THE ROUTER IS DOWN.", COL_WHITE);
        } else {
            tools_out_page(router ? "YOUR ROUTER'S DNS DOES NOT ANSWER," : "THE ACCESS POINT GAVE NO DNS SERVER,", COL_YELLOW);
            tools_out_page("BUT THE PUBLIC ONES DO.", COL_WHITE);
        }
        return;
    }
    if (public_answered == 0) {
        tools_out_page("ONLY YOUR ROUTER'S DNS ANSWERS.", COL_YELLOW);
        tools_out_page("THE PUBLIC ONES MAY BE BLOCKED.", COL_WHITE);
        return;
    }
    if (router->rcode != DNSQ_RCODE_OK && best->rcode == DNSQ_RCODE_OK) {
        tools_out_page("YOUR ROUTER SAYS THE NAME IS NOT", COL_YELLOW);
        tools_out_page("FOUND, THE PUBLIC ONES FIND IT.", COL_WHITE);
        return;
    }
    if (router->total / router->replies > DNS_SLOW_MS &&
        router->total / router->replies > DNS_SLOW_FACTOR * (best->total / best->replies)) {
        snprintf(line, sizeof line, "YOUR ROUTER'S DNS IS SLOW: %u MS", router->total / router->replies);
        tools_out_page(line, COL_YELLOW);
        snprintf(line, sizeof line, "%s TOOK %u MS", best->label, best->total / best->replies);
        tools_out_page(line, COL_WHITE);
        return;
    }
    for (i = 1; i < DNS_SERVERS_MAX; i++)
        if (servers[i].answered && share_address(router, &servers[i]))
            shared++;
    if (router->rcode == DNSQ_RCODE_OK && router->a_count > 0 && shared == 0) {
        tools_out_page("THE ANSWERS DIFFER FROM THE PUBLIC", COL_YELLOW);
        tools_out_page("ONES (OFTEN HARMLESS: CDN NAMES).", COL_WHITE);
        return;
    }
    tools_out_page("DNS LOOKS HEALTHY.", COL_GREEN);
}

/* The answer times as bars, one for each server: the bar is the average, the white line under it
 * runs from the fastest to the slowest answer. */
static void draw_chart(void)
{
    unsigned i, row = 0, top = 0;
    char line[TOOL_LINE_MAX];

    for (i = 0; i < DNS_SERVERS_MAX; i++)
        if (servers[i].used && servers[i].answered && servers[i].worst > top)
            top = servers[i].worst;
    top = gfx_scale_up(top);
    screen_clear();
    put_row(2, "DNS ANSWER TIME", COL_CYAN);
    for (i = 0; i < DNS_SERVERS_MAX; i++) {
        const server_t *s = &servers[i];
        int y = DNSBARS_Y + (int)row * DNSBARS_STEP;

        if (!s->used)
            continue;
        row++;
        put_at(1, y / 10, s->label, COL_WHITE);
        if (!s->answered) {
            put_at(DNSBARS_X / 8, y / 10, "NO ANSWER", COL_YELLOW);
            continue;
        }
        gfx_hbar(DNSBARS_X, y, DNSBARS_H, DNSBARS_W, s->total / s->replies, top, gfx_ms_colour(s->total / s->replies));
        gfx_rect(COL_WHITE, DNSBARS_X + (int)((uint32_t)s->best * DNSBARS_W / top), y + DNSBARS_H + 2,
                 (int)((uint32_t)(s->worst - s->best) * DNSBARS_W / top) + 2, 2);
        snprintf(line, sizeof line, "%u MS", s->total / s->replies);
        put_at((DNSBARS_X + DNSBARS_W) / 8 + 1, y / 10, line, COL_WHITE);
    }
    snprintf(line, sizeof line, "FULL LENGTH = %u MS", top);
    put_at(1, DNSBARS_ROW_NOTE, line, COL_WHITE);
    put_at(1, DNSBARS_ROW_NOTE + 1, "BAR = AVERAGE  LINE = MIN TO MAX", COL_WHITE);
}

void tool_dns_check(void)
{
    static const char *const public_labels[DNS_PUBLIC_COUNT] = DNS_PUBLIC_LABELS;
    static const char *const public_addrs[DNS_PUBLIC_COUNT] = DNS_PUBLIC_ADDRS;
    char name[TOOL_HOST_MAX + 1], line[TOOL_LINE_MAX];
    wifi_info_t info;
    unsigned i;
    bool cancelled = false;

    tools_settings_load();
    snprintf(name, sizeof name, "%s", tools_settings()->lookup);
    if (!ask_text("DNS CHECKER", "", "NAME TO LOOK UP", name, TOOL_HOST_MAX, 1, false, false))
        return;
    snprintf(tools_settings()->lookup, sizeof tools_settings()->lookup, "%s", name);
    tools_settings_save();

    memset(servers, 0, sizeof servers);
    servers[0].label = "ROUTER";
    if (link_info_get(&info) && info.dns[0] != '\0' && strcmp(info.dns, "0.0.0.0") != 0) {
        snprintf(servers[0].addr, sizeof servers[0].addr, "%s", info.dns);
        servers[0].used = true;
    }
    for (i = 0; i < DNS_PUBLIC_COUNT; i++) {
        servers[1 + i].label = public_labels[i];
        snprintf(servers[1 + i].addr, sizeof servers[1 + i].addr, "%s", public_addrs[i]);
        servers[1 + i].used = true;
    }
    tools_screen("DNS CHECKER", name);
    drain_keys();
    for (i = 0; i < DNS_SERVERS_MAX && !cancelled; i++) {
        if (!servers[i].used)
            continue;
        snprintf(line, sizeof line, "ASKING %s...", servers[i].label);
        clear_row(TOOL_ROW_LAST + 1);
        put_at(TOOL_COL, TOOL_ROW_LAST + 1, line, COL_YELLOW);
        if (!check_server(&servers[i], name)) {
            cancelled = true;
            break;
        }
        show_server(&servers[i]);
    }
    clear_row(TOOL_ROW_LAST + 1);
    if (cancelled)
        tools_out_page("STOPPED", COL_YELLOW);
    else {
        verdict();
        put_row(TOOL_ROW_HELP, "ANY KEY: THE CHART", COL_WHITE);
        (void)wait_any_key();
        draw_chart();
    }
    tools_finish();
}
