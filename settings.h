/*
 * settings.h - the hosts, port and time offset the network tools used last, as
 * text lines (key=value) that settings.c turns to and from a struct. Plain
 * portable C; reading and writing the file is storage.h's job.
 *
 * Author: Thomas Dzubin
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stddef.h>

#include "settings_const.h"

typedef struct {
    char ping[SETTINGS_HOST_MAX + 1];
    char lookup[SETTINGS_HOST_MAX + 1];
    char server[SETTINGS_HOST_MAX + 1];
    char port[SETTINGS_PORT_MAX + 1];
    char ntp[SETTINGS_HOST_MAX + 1];
    char offset[SETTINGS_OFFSET_MAX + 1];
    char url[SETTINGS_URL_MAX + 1];
    char ports[SETTINGS_PORTS_MAX + 1];       /* the port check's list */
    char speed_url[SETTINGS_URL_MAX + 1];     /* the speed test's file */
    char lan_first[SETTINGS_ADDR_MAX + 1];    /* the LAN scan's range, "" until one has been typed */
    char lan_last[SETTINGS_ADDR_MAX + 1];
} settings_t;

void settings_defaults(settings_t *settings);

/* Write the settings as text. Returns the length, or 0 if `max` is too small. */
size_t settings_format(const settings_t *settings, char *out, size_t max);

/* Read lines of key=value into the settings (unknown keys, bad lines and
 * values that are too long are skipped; what is not mentioned stays as it is).
 * Both LF and CR LF line ends are accepted. */
void settings_parse(settings_t *settings, const char *text, size_t length);

#endif /* SETTINGS_H */
