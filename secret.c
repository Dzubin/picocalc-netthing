/*
 * secret.c - see secret.h.
 *
 * The scrambling is a XOR with a stream of bytes from a small generator
 * (xorshift32) seeded from the fixed key and the file's salt, and a CRC-32 at
 * the end of the body catches damage or a wrong key.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "secret.h"

typedef struct {
    uint32_t state;
} stream_t;

static void stream_start(stream_t *s, uint32_t salt)
{
    s->state = (SECRET_SEED_A ^ (salt * SECRET_MIX)) + SECRET_SEED_B;
    if (s->state == 0)
        s->state = SECRET_SEED_B;
}

static uint8_t stream_next(stream_t *s)
{
    uint32_t x = s->state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->state = x;
    return (uint8_t)(x >> 11);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;
    unsigned bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

size_t secret_pack(const secret_net_t *nets, unsigned count, uint32_t salt,
                   uint8_t *out, size_t max)
{
    uint8_t *body = out + SECRET_HEADER_LEN;
    size_t n = 0, i, size;
    stream_t stream;
    unsigned k;

    if (count > SECRET_NETS_MAX)
        return 0;
    if (max < SECRET_HEADER_LEN + 1 + SECRET_CRC_LEN)
        return 0;
    body[n++] = (uint8_t)count;
    for (k = 0; k < count; k++) {
        size_t sl = strlen(nets[k].ssid);
        size_t pl = strlen(nets[k].pass);

        if (sl == 0 || sl > SECRET_SSID_MAX || pl > SECRET_PASS_MAX)
            return 0;
        if (SECRET_HEADER_LEN + n + 2 + sl + pl + SECRET_CRC_LEN > max)
            return 0;
        body[n++] = (uint8_t)sl;
        memcpy(body + n, nets[k].ssid, sl);
        n += sl;
        body[n++] = (uint8_t)pl;
        memcpy(body + n, nets[k].pass, pl);
        n += pl;
    }
    put32(body + n, crc32_update(0xFFFFFFFFu, body, n) ^ 0xFFFFFFFFu);
    n += SECRET_CRC_LEN;

    memcpy(out, SECRET_MAGIC, SECRET_MAGIC_LEN);
    put32(out + SECRET_MAGIC_LEN, salt);
    stream_start(&stream, salt);
    for (i = 0; i < n; i++)
        body[i] ^= stream_next(&stream);
    size = SECRET_HEADER_LEN + n;
    return size;
}

int secret_unpack(const uint8_t *in, size_t len, secret_net_t *nets, unsigned max,
                  unsigned *count)
{
    uint8_t body[SECRET_FILE_MAX];
    size_t n, pos = 0, i;
    stream_t stream;
    unsigned found, k;

    *count = 0;
    if (len < SECRET_HEADER_LEN + 1 + SECRET_CRC_LEN || len > SECRET_FILE_MAX)
        return 1;
    if (memcmp(in, SECRET_MAGIC, SECRET_MAGIC_LEN) != 0)
        return 1;
    n = len - SECRET_HEADER_LEN;
    stream_start(&stream, get32(in + SECRET_MAGIC_LEN));
    for (i = 0; i < n; i++)
        body[i] = in[SECRET_HEADER_LEN + i] ^ stream_next(&stream);
    if (get32(body + n - SECRET_CRC_LEN) !=
        (crc32_update(0xFFFFFFFFu, body, n - SECRET_CRC_LEN) ^ 0xFFFFFFFFu))
        return 2;
    n -= SECRET_CRC_LEN;

    found = body[pos++];
    if (found > max || found > SECRET_NETS_MAX)
        return 1;
    for (k = 0; k < found; k++) {
        size_t sl, pl;

        if (pos >= n)
            return 1;
        sl = body[pos++];
        if (sl == 0 || sl > SECRET_SSID_MAX || pos + sl >= n)
            return 1;
        memcpy(nets[k].ssid, body + pos, sl);
        nets[k].ssid[sl] = '\0';
        pos += sl;
        pl = body[pos++];
        if (pl > SECRET_PASS_MAX || pos + pl > n)
            return 1;
        memcpy(nets[k].pass, body + pos, pl);
        nets[k].pass[pl] = '\0';
        pos += pl;
    }
    if (pos != n)
        return 1;
    *count = found;
    return 0;
}
