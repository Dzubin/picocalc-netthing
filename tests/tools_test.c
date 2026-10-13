/*
 * tools_test.c - PC test of timeutil.c (offsets and calendar times) and of
 * settings.c (the text file of last-used hosts).
 *
 *   gcc -O1 -Wall -Wextra -I. tests/tools_test.c timeutil.c settings.c -o tools_test && ./tools_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "timeutil.h"
#include "settings.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

static int civil_is(uint32_t seconds, int offset, unsigned y, unsigned mo, unsigned d, unsigned h,
                    unsigned mi, unsigned s, unsigned weekday)
{
    timeutil_t t;

    timeutil_civil(seconds, offset, &t);
    return t.year == y && t.month == mo && t.day == d && t.hour == h && t.minute == mi &&
           t.second == s && t.weekday == weekday;
}

int main(void)
{
    int m = 0;
    char text[32];
    settings_t s, t;
    char file[SETTINGS_FILE_MAX];
    size_t n;

    /* calendar times, checked against Python's datetime */
    check("the epoch is a Thursday", civil_is(0, 0, 1970, 1, 1, 0, 0, 0, 4));
    check("1700000000", civil_is(1700000000u, 0, 2023, 11, 14, 22, 13, 20, 2));
    check("a leap day: 2000-02-29", civil_is(951782400u, 0, 2000, 2, 29, 0, 0, 0, 2));
    check("1760000000", civil_is(1760000000u, 0, 2025, 10, 9, 8, 53, 20, 4));
    check("the last second of 2099", civil_is(4102444799u, 0, 2099, 12, 31, 23, 59, 59, 4));
    check("2038-01-19", civil_is(2147483647u, 0, 2038, 1, 19, 3, 14, 7, 2));
    check("UTC-6 moves back over the hour", civil_is(1700000000u, -360, 2023, 11, 14, 16, 13, 20, 2));
    check("UTC+5:30 crosses midnight", civil_is(1700000000u, 330, 2023, 11, 15, 3, 43, 20, 3));
    check("before 1970 by an offset", civil_is(3600u, -120, 1969, 12, 31, 23, 0, 0, 3));

    /* offsets */
    check("-6", timeutil_parse_offset("-6", &m) && m == -360);
    check("+5", timeutil_parse_offset("+5", &m) && m == 300);
    check("5:30", timeutil_parse_offset("5:30", &m) && m == 330);
    check("-3:30", timeutil_parse_offset("-3:30", &m) && m == -210);
    check("-3.5", timeutil_parse_offset("-3.5", &m) && m == -210);
    check("0", timeutil_parse_offset("0", &m) && m == 0);
    check("14 is the east limit", timeutil_parse_offset("14", &m) && m == 840);
    check("15 is too far east", !timeutil_parse_offset("15", &m));
    check("-13 is too far west", !timeutil_parse_offset("-13", &m));
    check("a word is refused", !timeutil_parse_offset("six", &m));
    check("empty is refused", !timeutil_parse_offset("", &m));
    check("a sign alone is refused", !timeutil_parse_offset("-", &m));
    check("1:75 is refused", !timeutil_parse_offset("1:75", &m));
    check("-6 then text is refused", !timeutil_parse_offset("-6x", &m));
    timeutil_format_offset(0, text, sizeof text);
    check("format 0", strcmp(text, "UTC") == 0);
    timeutil_format_offset(-360, text, sizeof text);
    check("format -360", strcmp(text, "UTC-6") == 0);
    timeutil_format_offset(330, text, sizeof text);
    check("format 330", strcmp(text, "UTC+5:30") == 0);

    /* settings */
    settings_defaults(&s);
    check("defaults", strcmp(s.port, "7000") == 0 && strcmp(s.ntp, "pool.ntp.org") == 0 && s.ping[0] == '\0');
    strcpy(s.ping, "192.168.1.1");
    strcpy(s.server, "192.168.1.50");
    strcpy(s.offset, "-6");
    n = settings_format(&s, file, sizeof file);
    check("format fits", n != 0 && n < sizeof file);
    settings_defaults(&t);
    settings_parse(&t, file, n);
    check("round trip", memcmp(&s, &t, sizeof s) == 0);
    settings_defaults(&t);
    settings_parse(&t, "ping=10.0.0.1\r\nbogus=1\r\nnoequals\r\nport=8080\r\n", 45);
    check("CR LF and unknown lines", strcmp(t.ping, "10.0.0.1") == 0 && strcmp(t.port, "8080") == 0 &&
          strcmp(t.ntp, "pool.ntp.org") == 0);
    settings_defaults(&t);
    memset(file, 'x', sizeof file);
    memcpy(file, "ping=", 5);
    settings_parse(&t, file, sizeof file - 1);
    check("a value that is too long is skipped", t.ping[0] == '\0');
    check("a file that is too small is refused", settings_format(&s, file, 10) == 0);

    printf("\n%d check(s) failed\n", failures);
    return failures;
}
