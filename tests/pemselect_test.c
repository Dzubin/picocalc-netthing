/*
 * pemselect_test.c - PC test of pemselect.c, the picker of root certificates.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/pemselect_test.c pemselect.c -o pemselect_test
 *   ./pemselect_test                         a small made-up list
 *   ./pemselect_test cacert.pem CA.PEM       also: pick the 32 default roots out of the real list
 *                                            and compare with CA.PEM made by tools/make_ca_pem.py
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pemselect.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

static char *out;
static size_t out_len, out_max;

static void sink(void *context, const char *text, size_t length)
{
    (void)context;
    if (out_len + length <= out_max) {
        memcpy(out + out_len, text, length);
        out_len += length;
    }
}

static char *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    char *data;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc(*size + 1);
    if (fread(data, 1, *size, f) != *size)
        *size = 0;
    fclose(f);
    return data;
}

static const char *const defaults[] = {
    "ISRG Root X1", "ISRG Root X2", "DigiCert Global Root G2", "DigiCert Global Root G3",
    "DigiCert TLS ECC P384 Root G5", "DigiCert TLS RSA4096 Root G5", "DigiCert Trusted Root G4",
    "GlobalSign Root CA - R3", "GlobalSign Root R46", "GlobalSign Root E46", "GlobalSign ECC Root CA - R5",
    "Go Daddy Root Certificate Authority - G2", "Starfield Root Certificate Authority - G2",
    "USERTrust RSA Certification Authority", "USERTrust ECC Certification Authority",
    "COMODO RSA Certification Authority", "Amazon Root CA 1", "Amazon Root CA 2", "Amazon Root CA 3",
    "Amazon Root CA 4", "GTS Root R1", "GTS Root R3", "GTS Root R4",
    "Microsoft RSA Root Certificate Authority 2017", "Microsoft ECC Root Certificate Authority 2017",
    "Sectigo Public Server Authentication Root R46", "Sectigo Public Server Authentication Root E46",
    "IdenTrust Commercial Root CA 1", "SSL.com TLS ECC Root CA 2022", "SSL.com TLS RSA Root CA 2022",
    "SSL.com Root Certification Authority ECC", "SSL.com Root Certification Authority RSA",
};

int main(int argc, char **argv)
{
    static const char sample[] =
        "##\n## Bundle\n##\n\nFirst Root\n==========\n-----BEGIN CERTIFICATE-----\nAAAA\nBBBB\n-----END CERTIFICATE-----\n\n"
        "Second Root\n===========\nSHA256 Fingerprint: 12:34\n-----BEGIN CERTIFICATE-----\nCCCC\n-----END CERTIFICATE-----\n\n"
        "Third Root\n==========\n-----BEGIN CERTIFICATE-----\nDDDD\n-----END CERTIFICATE-----\n";
    static const char *const wanted[] = { "Third Root", "First Root", "Missing Root" };
    pemselect_t p;
    size_t pos, step;
    int i;

    out = malloc(4096);
    out_max = 4096;

    for (step = 0; step < 2; step++) {
        out_len = 0;
        pemselect_init(&p, wanted, 3, 4096, sink, NULL);
        for (pos = 0; pos < sizeof sample - 1; pos += step ? 1 : sizeof sample)
            pemselect_feed(&p, (const uint8_t *)sample + pos, step ? 1 : sizeof sample - 1);
        pemselect_finish(&p);
        check(step ? "a made-up list, a byte at a time" : "a made-up list",
              p.roots == 2 && out_len == strlen("-----BEGIN CERTIFICATE-----\nAAAA\nBBBB\n-----END CERTIFICATE-----\n"
                                                "-----BEGIN CERTIFICATE-----\nDDDD\n-----END CERTIFICATE-----\n") &&
              memcmp(out, "-----BEGIN CERTIFICATE-----\nAAAA", 32) == 0 && p.found[0] && p.found[1] && !p.found[2]);
    }
    out_len = 0;
    pemselect_init(&p, wanted, 3, 60, sink, NULL);
    pemselect_feed(&p, (const uint8_t *)sample, sizeof sample - 1);
    pemselect_finish(&p);
    check("a limit on the size leaves a certificate out", p.roots == 1 && p.too_big);
    out_len = 0;
    pemselect_init(&p, wanted, 3, 4096, sink, NULL);
    {
        static const char crlf[] = "First Root\r\n==========\r\n-----BEGIN CERTIFICATE-----\r\nAAAA\r\n-----END CERTIFICATE-----\r\n";

        pemselect_feed(&p, (const uint8_t *)crlf, sizeof crlf - 1);
        pemselect_finish(&p);
        check("CR LF line ends", p.roots == 1 && strstr(out, "\r") == NULL || (out[out_len] = 0, strchr(out, '\r') == NULL));
    }

    if (argc > 2) {
        size_t list_size, want_size;
        char *list = read_all(argv[1], &list_size), *want = read_all(argv[2], &want_size);

        if (!list || !want) {
            printf("cannot read the files\n");
            return 99;
        }
        free(out);
        out = malloc(100000);
        out_max = 100000;
        out_len = 0;
        pemselect_init(&p, defaults, sizeof defaults / sizeof defaults[0], 49152, sink, NULL);
        for (pos = 0; pos < list_size; pos += 1000)
            pemselect_feed(&p, (const uint8_t *)list + pos, list_size - pos < 1000 ? list_size - pos : 1000);
        pemselect_finish(&p);
        check("32 roots picked from the real list", p.roots == 32);
        check("every wanted name was found", ({ int all = 1; for (i = 0; i < 32; i++) all = all && p.found[i]; all; }));
        check("the result is the same file as make_ca_pem.py makes", out_len == want_size && memcmp(out, want, want_size) == 0);
        printf("      %u roots, %u bytes\n", p.roots, (unsigned)p.bytes);
    }

    printf("\n%d check(s) failed\n", failures);
    return failures;
}
