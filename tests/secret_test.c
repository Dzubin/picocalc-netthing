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

/* The file of the first kind (NTW1, no addresses) holding one network "AB"
 * with the password "xy", scrambled the way secret.c does it. */
static size_t make_v1_file(uint8_t *out)
{
    static const uint8_t plain[] = { 1, 2, 'A', 'B', 2, 'x', 'y' };
    uint8_t body[sizeof plain + 4];
    uint32_t crc = 0xFFFFFFFFu, salt = 0x01020304u;
    uint32_t state = (SECRET_SEED_A ^ (salt * SECRET_MIX)) + SECRET_SEED_B;
    size_t i;
    unsigned bit;

    memcpy(body, plain, sizeof plain);
    for (i = 0; i < sizeof plain; i++) {
        crc ^= plain[i];
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    crc ^= 0xFFFFFFFFu;
    for (i = 0; i < 4; i++)
        body[sizeof plain + i] = (uint8_t)(crc >> (8 * i));
    memcpy(out, SECRET_MAGIC_V1, 4);
    for (i = 0; i < 4; i++)
        out[4 + i] = (uint8_t)(salt >> (8 * i));
    for (i = 0; i < sizeof body; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        out[SECRET_HEADER_LEN + i] = body[i] ^ (uint8_t)(state >> 11);
    }
    return SECRET_HEADER_LEN + sizeof body;
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

    {
        static const uint8_t ap_a[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
        static const uint8_t ap_b[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x66 };
        static const uint8_t ap_c[6] = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
        uint8_t extra[6];
        secret_net_t list[2];

        memset(list, 0, sizeof list);
        strcpy(list[0].ssid, "HomeNet");
        strcpy(list[0].pass, "secret words");
        strcpy(list[1].ssid, "Office");
        check("address added", secret_add_bssid(&list[0], ap_a) && list[0].bssid_count == 1);
        check("same address not added twice", !secret_add_bssid(&list[0], ap_a) && list[0].bssid_count == 1);
        check("second address added", secret_add_bssid(&list[0], ap_b) && list[0].bssid_count == 2);

        check("name and address: both", secret_match(list, 2, "HomeNet", ap_a) == SECRET_MATCH_BOTH);
        check("other address of the name: one", secret_match(list, 2, "HomeNet", ap_c) == SECRET_MATCH_ONE);
        check("name with no address saved: one", secret_match(list, 2, "Office", ap_a) == SECRET_MATCH_ONE);
        check("address under another name: one", secret_match(list, 2, "Renamed", ap_b) == SECRET_MATCH_ONE);
        check("hidden name, saved address: one", secret_match(list, 2, "", ap_a) == SECRET_MATCH_ONE);
        check("unknown name and address: none", secret_match(list, 2, "Cafe", ap_c) == SECRET_MATCH_NONE);
        check("empty list: none", secret_match(list, 0, "HomeNet", ap_a) == SECRET_MATCH_NONE);

        memset(extra, 0x30, sizeof extra);
        for (i = 0; i < SECRET_BSSID_MAX + 2; i++) {
            extra[5] = (uint8_t)(0x30 + i);
            secret_add_bssid(&list[1], extra);
        }
        check("full list keeps the newest addresses",
              list[1].bssid_count == SECRET_BSSID_MAX && list[1].bssid[0][5] == 0x32 &&
              list[1].bssid[SECRET_BSSID_MAX - 1][5] == 0x35);

        size1 = secret_pack(list, 2, 5, file1, sizeof file1);
        check("addresses pack", size1 != 0 && memcmp(file1, "NTW2", 4) == 0);
        memset(back, 0, sizeof back);
        check("addresses unpack",
              secret_unpack(file1, size1, back, SECRET_NETS_MAX, &count) == 0 && count == 2 &&
              back[0].bssid_count == 2 && memcmp(back[0].bssid[1], ap_b, 6) == 0 &&
              back[1].bssid_count == SECRET_BSSID_MAX && strcmp(back[1].ssid, "Office") == 0);
        list[0].bssid_count = SECRET_BSSID_MAX + 1;
        check("too many addresses refused", secret_pack(list, 2, 5, file1, sizeof file1) == 0);

        {
            uint8_t v1[SECRET_FILE_MAX];
            size_t sz = make_v1_file(v1);

            memset(back, 0xEE, sizeof back);
            check("a file of the first kind is read, without addresses",
                  secret_unpack(v1, sz, back, SECRET_NETS_MAX, &count) == 0 && count == 1 &&
                  strcmp(back[0].ssid, "AB") == 0 && strcmp(back[0].pass, "xy") == 0 &&
                  back[0].bssid_count == 0);
        }
    }

    printf("\n%d check(s) failed\n", failures);
    return failures;
}
