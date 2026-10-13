/*
 * tether.h - the Bluetooth tether slots at the bottom of the Wi-Fi scan screen:
 * up to four phones found by Classic inquiries. Bluetooth is on only between
 * tether_open() and tether_close(); the screen starts an inquiry now and then
 * (tether_search()) in between the Wi-Fi scans, so the two do not share the
 * radio's airtime.
 *
 * Author: Thomas Dzubin
 */
#ifndef TETHER_H
#define TETHER_H

#include <stdbool.h>

#include "tether_const.h"

/* Switch the radio on and begin searching (the radio takes a second or two to
 * start; tether_poll() carries on from there). */
void tether_open(void);

/* Stop searching and switch the radio off. */
void tether_close(void);

/* True once the radio is on and can search. */
bool tether_ready(void);

/* Start one inquiry (about TETHER_INQUIRY_SECONDS, then the names of the
 * devices that gave none). Returns false if the radio is not ready or the
 * inquiry would not start. */
bool tether_search(void);

/* True while an inquiry or the asking of names goes on. */
bool tether_busy(void);

/* Call it often. Watches the radio start and looks at the table of devices now
 * and then. Returns true when what tether_draw() shows has changed. */
bool tether_poll(void);

/* Draw the heading, with `what` at its right end, and the slots. */
void tether_draw(const char *what);

#endif /* TETHER_H */
