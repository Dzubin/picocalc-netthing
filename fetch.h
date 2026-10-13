/*
 * fetch.h - what the other tools use of the web check: download an address
 * to the SD card, and share the big certificate buffer.
 *
 * Author: Thomas Dzubin
 */
#ifndef FETCH_H
#define FETCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "url.h"

#define FETCH_OK      0
#define FETCH_CANCEL  1
#define FETCH_ERROR   2

/* Fetch an address into a named file on the SD card, following redirects (the address is
 * moved to where it was found), with no screen of its own. Returns FETCH_OK, FETCH_CANCEL
 * (ESC), or FETCH_ERROR with the reason written on the current tool screen (tools_screen()). */
int fetch_file(url_t *url, const char *path, uint32_t *bytes);

/* The speed test: tool_speed() asks for an address; fetch_speed_server() runs it against the test
 * server's /stream page. Each shows its own screen. */
void tool_speed(void);
void fetch_speed_server(const char *host, unsigned port);

/* There is no CA.PEM on the card: offer to write the root certificates built into the program
 * there (a Y/N question on the current tool screen). Returns true if the file was written. */
bool fetch_offer_builtin_ca(void);

#endif /* FETCH_H */
