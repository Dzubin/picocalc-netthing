/*
 * portlist.h - a list of TCP ports typed as text ("80,443,20-25") turned into
 * numbers, and the names of the well-known ports. Plain portable C.
 *
 * Author: Thomas Dzubin
 */
#ifndef PORTLIST_H
#define PORTLIST_H

#include <stdint.h>

#include "portlist_const.h"

/* Read a list: numbers 1 to 65535 and ranges (a-b) separated by commas or spaces. Returns how
 * many ports were stored in out[] (at most PORTLIST_MAX), or -1 if the text is empty, a number
 * or range is bad, or there are too many ports. */
int portlist_parse(const char *text, uint16_t *out);

/* The name of a well-known port, or "" for any other. */
const char *portlist_name(uint16_t port);

#endif /* PORTLIST_H */
