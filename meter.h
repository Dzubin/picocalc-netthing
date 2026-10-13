/*
 * meter.h - the signal meter picture: one wide bar for the signal strength, the
 * number in dBm and in words, the best and worst seen, and a strip of the recent
 * past below. The Wi-Fi meter (meter.c) and the Bluetooth device finder
 * (btfind.c) both draw with it.
 *
 * Author: Thomas Dzubin
 */
#ifndef METER_PICTURE_H
#define METER_PICTURE_H

#include <stdbool.h>
#include <stdint.h>

/* Clear the screen and draw the heading, the scale and the help line; forget the best, worst and history. */
void meter_frame(const char *title, const char *help);

/* Show a reading. `name` is written under the heading. With `add` the reading also goes on the
 * history strip (and into the best and worst). */
void meter_show(int rssi, const char *name, bool add);

/* Put a line of text in place of the number (for "NOT HEARD" and the like). */
void meter_message(const char *text, uint16_t colour);

/* A line under the best and worst (for "WARMER"). */
void meter_hint(const char *text, uint16_t colour);

/* Forget the best and the worst. */
void meter_clear_best(void);

#endif /* METER_PICTURE_H */
