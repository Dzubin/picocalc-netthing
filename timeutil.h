/*
 * timeutil.h - the date and time arithmetic of the clock screens: a UTC offset
 * typed as text, and a count of seconds turned into a calendar date and time.
 * Plain portable C with no library time functions (no time zone database and
 * no daylight saving: the operator changes the offset).
 *
 * Author: Thomas Dzubin
 */
#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define TIMEUTIL_OFFSET_MIN   (-12 * 60)    /* furthest west accepted, in minutes */
#define TIMEUTIL_OFFSET_MAX   (14 * 60)     /* furthest east */

typedef struct {
    unsigned year, month, day;      /* month 1 to 12, day 1 to 31 */
    unsigned hour, minute, second;
    unsigned weekday;               /* 0 is Sunday */
} timeutil_t;

/* Read an offset from UTC: "-6", "+5", "5:30", "-3:30" or "-3.5" (a number of
 * hours, then optionally half an hour or a colon and minutes). Returns false if
 * it is not one or is out of range. */
bool timeutil_parse_offset(const char *text, int *minutes);

/* "UTC", "UTC-6" or "UTC+5:30" */
void timeutil_format_offset(int minutes, char *out, size_t size);

/* The calendar time for `unix_seconds` (seconds since 1 January 1970 UTC)
 * moved by `offset_minutes`. */
void timeutil_civil(uint32_t unix_seconds, int offset_minutes, timeutil_t *out);

#endif /* TIMEUTIL_H */
