/*
 * settings.c - see settings.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "settings.h"

void settings_defaults(settings_t *settings)
{
    memset(settings, 0, sizeof *settings);
    snprintf(settings->ping, sizeof settings->ping, "%s", SETTINGS_DEFAULT_PING);
    snprintf(settings->lookup, sizeof settings->lookup, "%s", SETTINGS_DEFAULT_LOOKUP);
    snprintf(settings->server, sizeof settings->server, "%s", SETTINGS_DEFAULT_SERVER);
    snprintf(settings->port, sizeof settings->port, "%s", SETTINGS_DEFAULT_PORT);
    snprintf(settings->ntp, sizeof settings->ntp, "%s", SETTINGS_DEFAULT_NTP);
    snprintf(settings->offset, sizeof settings->offset, "%s", SETTINGS_DEFAULT_OFFSET);
    snprintf(settings->url, sizeof settings->url, "%s", SETTINGS_DEFAULT_URL);
    snprintf(settings->ports, sizeof settings->ports, "%s", SETTINGS_DEFAULT_PORTS);
    snprintf(settings->speed_url, sizeof settings->speed_url, "%s", SETTINGS_DEFAULT_SPEED_URL);
}

size_t settings_format(const settings_t *settings, char *out, size_t max)
{
    int n = snprintf(out, max,
                     SETTINGS_KEY_PING "=%s\n" SETTINGS_KEY_LOOKUP "=%s\n" SETTINGS_KEY_SERVER "=%s\n"
                     SETTINGS_KEY_PORT "=%s\n" SETTINGS_KEY_NTP "=%s\n" SETTINGS_KEY_OFFSET "=%s\n"
                     SETTINGS_KEY_URL "=%s\n" SETTINGS_KEY_PORTS "=%s\n" SETTINGS_KEY_SPEED_URL "=%s\n" SETTINGS_KEY_LAN_FIRST "=%s\n" SETTINGS_KEY_LAN_LAST "=%s\n",
                     settings->ping, settings->lookup, settings->server, settings->port,
                     settings->ntp, settings->offset, settings->url, settings->ports, settings->speed_url, settings->lan_first,
                     settings->lan_last);

    return n > 0 && (size_t)n < max ? (size_t)n : 0;
}

/* Copy `value` (length n) into field of `size` bytes if it fits. */
static void set_field(char *field, size_t size, const char *value, size_t n)
{
    if (n < size) {
        memcpy(field, value, n);
        field[n] = '\0';
    }
}

void settings_parse(settings_t *settings, const char *text, size_t length)
{
    size_t pos = 0;

    while (pos < length) {
        size_t end = pos, eq, key_len, n;
        const char *line = text + pos;

        while (end < length && text[end] != '\n')
            end++;
        n = end - pos;
        if (n > 0 && line[n - 1] == '\r')
            n--;
        for (eq = 0; eq < n && line[eq] != '='; eq++)
            ;
        if (eq < n) {
            const char *value = line + eq + 1;
            size_t value_len = n - eq - 1;

            key_len = eq;
#define IS_KEY(k) (key_len == sizeof(k) - 1 && memcmp(line, k, key_len) == 0)
            if (IS_KEY(SETTINGS_KEY_PING))
                set_field(settings->ping, sizeof settings->ping, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_LOOKUP))
                set_field(settings->lookup, sizeof settings->lookup, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_SERVER))
                set_field(settings->server, sizeof settings->server, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_PORT))
                set_field(settings->port, sizeof settings->port, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_NTP))
                set_field(settings->ntp, sizeof settings->ntp, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_OFFSET))
                set_field(settings->offset, sizeof settings->offset, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_URL))
                set_field(settings->url, sizeof settings->url, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_PORTS))
                set_field(settings->ports, sizeof settings->ports, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_SPEED_URL))
                set_field(settings->speed_url, sizeof settings->speed_url, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_LAN_FIRST))
                set_field(settings->lan_first, sizeof settings->lan_first, value, value_len);
            else if (IS_KEY(SETTINGS_KEY_LAN_LAST))
                set_field(settings->lan_last, sizeof settings->lan_last, value, value_len);
#undef IS_KEY
        }
        pos = end + 1;
    }
}
