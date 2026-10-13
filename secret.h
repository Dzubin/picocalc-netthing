/*
 * secret.h - the saved Wi-Fi networks as a scrambled block of bytes: names and
 * passwords go in, a file image that shows no plain text comes out, and back.
 * Plain portable C; the SD card is storage.h's job.
 *
 * Author: Thomas Dzubin
 */
#ifndef SECRET_H
#define SECRET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "secret_const.h"

typedef struct {
    char ssid[SECRET_SSID_MAX + 1];
    char pass[SECRET_PASS_MAX + 1];     /* empty for an open network */
    uint8_t bssid_count;                /* access points joined, 0 to SECRET_BSSID_MAX */
    uint8_t bssid[SECRET_BSSID_MAX][SECRET_BSSID_LEN];  /* oldest first */
} secret_net_t;

/* How a scanned access point (its name, empty if hidden, and its address)
 * compares with the saved networks: SECRET_MATCH_BOTH if one network has that
 * name and lists that address; SECRET_MATCH_ONE if only the name matches (some
 * network has it, without that address) or only the address matches (listed
 * under another name); otherwise SECRET_MATCH_NONE. */
int secret_match(const secret_net_t *nets, unsigned count, const char *ssid,
                 const uint8_t bssid[SECRET_BSSID_LEN]);

/* Remember an access point address for a network. Returns true if the list
 * changed (false if the address was already there). When the list is full the
 * oldest address is dropped. */
bool secret_add_bssid(secret_net_t *net, const uint8_t bssid[SECRET_BSSID_LEN]);

/* Build the file image for `count` networks with the given salt (any value; a
 * new one at each save makes the bytes differ). Returns its size, or 0 if
 * `count` or a length is out of range or `max` is too small. */
size_t secret_pack(const secret_net_t *nets, unsigned count, uint32_t salt,
                   uint8_t *out, size_t max);

/* Read a file image back. Returns 0 and fills `nets` and `count`; 1 if it is
 * not one of our files or is cut short; 2 if the checksum does not match
 * (damaged, or written with another key). `max` is the room in `nets`. */
int secret_unpack(const uint8_t *in, size_t len, secret_net_t *nets, unsigned max,
                  unsigned *count);

#endif /* SECRET_H */
