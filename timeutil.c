/*
 * timeutil.c - see timeutil.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>

#include "timeutil.h"

bool timeutil_parse_offset(const char *text, int *minutes)
{
    int sign = 1;
    int hours = 0, mins = 0;
    bool digits = false;

    if (*text == '+' || *text == '-') {
        sign = *text == '-' ? -1 : 1;
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        hours = hours * 10 + (*text++ - '0');
        digits = true;
        if (hours > 99)
            return false;
    }
    if (!digits)
        return false;
    if (*text == ':') {
        text++;
        if (!(text[0] >= '0' && text[0] <= '9' && text[1] >= '0' && text[1] <= '9'))
            return false;
        mins = (text[0] - '0') * 10 + (text[1] - '0');
        text += 2;
        if (mins > 59)
            return false;
    } else if (*text == '.') {
        text++;
        if (*text != '5')
            return false;
        mins = 30;
        text++;
    }
    if (*text != '\0')
        return false;
    *minutes = sign * (hours * 60 + mins);
    return *minutes >= TIMEUTIL_OFFSET_MIN && *minutes <= TIMEUTIL_OFFSET_MAX;
}

void timeutil_format_offset(int minutes, char *out, size_t size)
{
    unsigned absolute = (unsigned)(minutes < 0 ? -minutes : minutes);

    if (minutes == 0)
        snprintf(out, size, "UTC");
    else if (absolute % 60 == 0)
        snprintf(out, size, "UTC%c%u", minutes < 0 ? '-' : '+', absolute / 60);
    else
        snprintf(out, size, "UTC%c%u:%02u", minutes < 0 ? '-' : '+', absolute / 60, absolute % 60);
}

/* Days since 1 January 1970 to a calendar date: the well-known civil-from-days
 * algorithm (proleptic Gregorian calendar), on signed 32-bit values. */
static void civil_from_days(int32_t days, unsigned *year, unsigned *month, unsigned *day)
{
    int32_t z = days + 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t y = yoe + era * 400;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    int32_t d = doy - (153 * mp + 2) / 5 + 1;
    int32_t m = mp < 10 ? mp + 3 : mp - 9;

    *year = (unsigned)(m <= 2 ? y + 1 : y);
    *month = (unsigned)m;
    *day = (unsigned)d;
}

void timeutil_civil(uint32_t unix_seconds, int offset_minutes, timeutil_t *out)
{
    int64_t local = (int64_t)unix_seconds + (int64_t)offset_minutes * 60;
    int64_t days = local >= 0 ? local / 86400 : -((-local + 86399) / 86400);
    int32_t rest = (int32_t)(local - days * 86400);

    civil_from_days((int32_t)days, &out->year, &out->month, &out->day);
    out->hour = (unsigned)(rest / 3600);
    out->minute = (unsigned)(rest % 3600 / 60);
    out->second = (unsigned)(rest % 60);
    out->weekday = (unsigned)(((days % 7) + 11) % 7);     /* 1 January 1970 was a Thursday */
}
