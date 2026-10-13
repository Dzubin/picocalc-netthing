/*
 * toolsui.h - what the tools share: a screen with a heading and lines of
 * output, a name lookup with its failure message, the settings file and the
 * clock. tools.c supplies it; fetch.c (the web tool) uses it.
 *
 * Author: Thomas Dzubin
 */
#ifndef TOOLSUI_H
#define TOOLSUI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "settings.h"

extern int tools_out_row;                     /* the next row of output */

void tools_screen(const char *heading, const char *name);
void tools_out_line(const char *text, uint16_t colour);
void tools_out_page(const char *text, uint16_t colour);   /* the same, starting a fresh page when full */
void tools_finish(void);                      /* "PRESS ANY KEY" and wait */
bool tools_wait_or_escape(uint32_t ms);       /* wait; true at once if ESC was pressed */

/* A ping and its wait (tools_ping_once), or only the wait after net_ping_start() (tools_ping_wait):
 * NET_DONE with the time in *ms, or NET_FAILED; *cancelled is set when ESC was pressed. */
int tools_ping_once(const char *addr, uint32_t timeout_ms, unsigned *ms, bool *cancelled);
int tools_ping_wait(uint32_t timeout_ms, unsigned *ms, bool *cancelled);

/* Wait for a stream just opened with net_stream_open(): 0 open, 1 failed (reason written), 2 ESC. */
int tools_stream_wait_open(void);

/* Look a host up: 0 with the dotted address, 1 not found, 2 cancelled (ESC). */
int  tools_resolve(const char *host, char *addr, size_t size);
void tools_resolve_failed(int how);

void        tools_settings_load(void);
void        tools_settings_save(void);
settings_t *tools_settings(void);

/* The clock as seconds since 1970 (UTC); 0 if it has not been set. */
uint32_t tools_clock_seconds(void);

/* Let the TLS layer see the clock (call once before the first https fetch). */
void tools_register_clock(void);

/* Make sure the clock is set, asking the NTP server of the settings if it is not.
 * Returns true if it is set now. Output goes on the current tool screen. */
bool tools_ensure_clock(void);

#endif /* TOOLSUI_H */
