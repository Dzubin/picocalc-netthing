/*
 * secret_test.c - PC test of secret.c, the scrambled list of saved networks.
 *
 *   gcc -O1 -Wall -I. tests/secret_test.c secret.c -o secret_test && ./secret_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "secret.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

static int contains(const uint8_t *data, size_t len, const char *text)
{
    size_t n = strlen(text), i;

    for (i = 0; i + n <= len; i++)
        if (memcmp(data + i, text, n) == 0)
            return 1;
    return 0;
}

int main(void)
{
    static secret_net_t nets[SECRET_NETS_MAX], back[SECRET_NETS_MAX];
    uint8_t file1[SECRET_FILE_MAX], file2[SECRET_FILE_MAX];
    size_t size1, size2;
    unsigned count = 99, i;

    strcpy(nets[0].ssid, "HomeNet-5G");
    strcpy(nets[0].pass, "correct horse battery staple");
    strcpy(nets[1].ssid, "Coffee_Guest");
    nets[1].pass[0] = '\0';                         /* an open network */
    strcpy(nets[2].ssid, "Tilde~Net");
    strcpy(nets[2].pass, "p~ss~word~with~tildes");

    size1 = secret_pack(nets, 3, 0x12345678u, file1, sizeof file1);
    check("pack three networks", size1 != 0);
    check("starts with the magic", memcmp(file1, SECRET_MAGIC, SECRET_MAGIC_LEN) == 0);
    check("no password in the file", !contains(file1, size1, "horse") && !contains(file1, size1, "p~ss"));
    check("no network name in the file", !contains(file1, size1, "HomeNet") && !contains(file1, size1, "Coffee"));

    memset(back, 0, sizeof back);
    check("unpack returns 0", secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 0);
    check("three networks back", count == 3);
    check("names and passwords equal",
          strcmp(back[0].ssid, nets[0].ssid) == 0 && strcmp(back[0].pass, nets[0].pass) == 0 &&
          strcmp(back[1].ssid, nets[1].ssid) == 0 && back[1].pass[0] == '\0' &&
          strcmp(back[2].ssid, nets[2].ssid) == 0 && strcmp(back[2].pass, nets[2].pass) == 0);

    size2 = secret_pack(nets, 3, 0x9ABCDEF0u, file2, sizeof file2);
    check("another salt gives another file of the same size",
          size2 == size1 && memcmp(file1 + SECRET_HEADER_LEN, file2 + SECRET_HEADER_LEN, size1 - SECRET_HEADER_LEN) != 0);

    file1[SECRET_HEADER_LEN + 5] ^= 0x01;           /* damage one bit */
    check("a damaged file fails the checksum", secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 2);
    file1[SECRET_HEADER_LEN + 5] ^= 0x01;
    check("the repaired file reads again", secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 0);

    file1[0] = 'X';
    check("wrong magic is refused", secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 1);
    check("a cut-short file is refused", secret_unpack(file2, 10, back, SECRET_NETS_MAX, &count) == 1);

    check("empty list packs", (size1 = secret_pack(nets, 0, 1, file1, sizeof file1)) != 0);
    check("empty list unpacks", secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 0 && count == 0);

    for (i = 0; i < SECRET_NETS_MAX; i++) {
        memset(nets[i].ssid, 'A' + (int)i, SECRET_SSID_MAX);
        nets[i].ssid[SECRET_SSID_MAX] = '\0';
        memset(nets[i].pass, 'a' + (int)i, SECRET_PASS_MAX);
        nets[i].pass[SECRET_PASS_MAX] = '\0';
    }
    size1 = secret_pack(nets, SECRET_NETS_MAX, 7, file1, sizeof file1);
    check("sixteen longest networks pack within the file maximum", size1 != 0 && size1 <= SECRET_FILE_MAX);
    check("and unpack",
          secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 0 && count == SECRET_NETS_MAX &&
          strcmp(back[15].pass, nets[15].pass) == 0);
    check("seventeen are refused", secret_pack(nets, SECRET_NETS_MAX + 1, 7, file1, sizeof file1) == 0);
    nets[0].ssid[0] = '\0';
    check("an empty network name is refused", secret_pack(nets, 1, 7, file1, sizeof file1) == 0);

    printf("\n%d check(s) failed\n", failures);
    return failures;
}
