/*
 * bmp_test.c - PC test of bmp.c, the BMP file pieces of a screenshot.
 *
 *   gcc -O1 -Wall -Wextra -I. tests/bmp_test.c bmp.c -o bmp_test && ./bmp_test
 *
 * The exit status is the number of failed checks.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "bmp.h"

static int failures;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

static unsigned le32(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

int main(void)
{
    uint8_t hdr[BMP_HEADER_SIZE], row[16];
    uint16_t px[3] = { 0xF800, 0x07E0, 0x001F };    /* red, green, blue */
    size_t n;

    check("a row of 320 pixels is 960 bytes", bmp_row_size(320) == 960);
    check("a row of 1 pixel is padded to 4", bmp_row_size(1) == 4);
    check("a row of 3 pixels is padded to 12", bmp_row_size(3) == 12);

    bmp_header(hdr, 320, 320);
    check("the header starts with BM", hdr[0] == 'B' && hdr[1] == 'M');
    check("the file size is the header and 320 rows", le32(hdr + 2) == BMP_HEADER_SIZE + 960u * 320u);
    check("the pixels start after the header", le32(hdr + 10) == BMP_HEADER_SIZE);
    check("the info header is 40 bytes", le32(hdr + 14) == 40);
    check("the width and height are stored", le32(hdr + 18) == 320 && le32(hdr + 22) == 320);
    check("24 bits a pixel, no compression", hdr[28] == 24 && hdr[29] == 0 && le32(hdr + 30) == 0);
    check("the image size is stored", le32(hdr + 34) == 960u * 320u);

    memset(row, 0xEE, sizeof row);
    n = bmp_row_from_rgb565(px, 3, row);
    check("a row of 3 pixels is 12 bytes", n == 12);
    check("red is B=0 G=0 R=255", row[0] == 0 && row[1] == 0 && row[2] == 255);
    check("green is B=0 G=255 R=0", row[3] == 0 && row[4] == 255 && row[5] == 0);
    check("blue is B=255 G=0 R=0", row[6] == 255 && row[7] == 0 && row[8] == 0);
    check("the padding is zero", row[9] == 0 && row[10] == 0 && row[11] == 0);

    px[0] = 0x0000;
    px[1] = 0xFFFF;
    bmp_row_from_rgb565(px, 2, row);
    check("black stays 0 and white becomes 255", row[0] == 0 && row[2] == 0 && row[3] == 255 && row[4] == 255 && row[5] == 255);

    printf("\n%d check(s) failed\n", failures);
    return failures;
}
