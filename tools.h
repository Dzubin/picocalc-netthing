/*
 * tools.h - the network tools of the status screen: ping, name lookup, a
 * talk with a test server, and the clock from an NTP server. Each one is a
 * whole screen that asks for what it needs (the host last used is offered
 * again), runs, shows the result and returns when a key is pressed.
 *
 * Author: Thomas Dzubin
 */
#ifndef TOOLS_H
#define TOOLS_H

#include <stdbool.h>

#include "modes.h"
#include <stddef.h>

void tool_ping(void);
void tool_ping_gateway(void);
void tool_ping_live(void);          /* pinglive.c */
void tool_trace(void);              /* trace.c */
void tool_ports(void);              /* ports.c */
void tool_lan(void);                /* lan.c */
void tool_survey(void);             /* survey.c */
void tool_menu(void);               /* menu.c: the Wi-Fi tools */
int  tool_lan_menu(void);            /* menu.c: the LAN tools mode; returns the mode to go to next */
void tool_health(void);             /* health.c */
void tool_meter(void);              /* meter.c */
int  tool_bt_menu(void);             /* btmenu.c: the Bluetooth mode; returns the mode to go to next */
void tool_dns_check(void);          /* dnscheck.c */
void tool_speed(void);              /* fetch.c */
void tool_server_menu(void);        /* srvtools.c */
void tool_server_download(void);
void tool_server_upload(void);
void tool_server_latency(void);
void tool_lookup(void);
void tool_server(void);
void tool_time(void);
void tool_get(void);                /* fetch.c */
void tool_certs(void);              /* certs.c */

/* The date and time of the clock the time tool set ("2026-10-12 14:23:05
 * UTC-6"). Returns false until it has been set. */
bool tools_clock_text(char *out, size_t size);

#endif /* TOOLS_H */
