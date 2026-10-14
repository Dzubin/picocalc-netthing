/*
 * bmp.c - see bmp.h. The header is built a byte at a time with little-endian
 * helpers, not as a packed struct laid over the bytes, so nothing depends on how a
 * compiler lays a struct out.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "bmp.h"

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

size_t bmp_row_size(unsigned width)
{
    size_t bytes = (size_t)width * BMP_BYTES_PER_PIXEL;

    return (bytes + BMP_ROW_ALIGN - 1) / BMP_ROW_ALIGN * BMP_ROW_ALIGN;
}

void bmp_header(uint8_t out[BMP_HEADER_SIZE], unsigned width, unsigned height)
{
    uint32_t pixel_bytes = (uint32_t)(bmp_row_size(width) * height);

    memset(out, 0, BMP_HEADER_SIZE);
    out[0] = 'B';
    out[1] = 'M';
    put_le32(out + 2, BMP_HEADER_SIZE + pixel_bytes);   /* the size of the file */
    put_le32(out + 10, BMP_HEADER_SIZE);                /* where the pixels start */
    put_le32(out + 14, BMP_INFO_HEADER_SIZE);
    put_le32(out + 18, width);
    put_le32(out + 22, height);                         /* positive: the rows run bottom up */
    put_le16(out + 26, 1);                              /* planes */
    put_le16(out + 28, BMP_BITS_PER_PIXEL);
    put_le32(out + 30, BMP_COMPRESSION_BI_RGB);
    put_le32(out + 34, pixel_bytes);
}

size_t bmp_row_from_rgb565(const uint16_t *pixels, unsigned width, uint8_t *out)
{
    size_t size = bmp_row_size(width);
    unsigned i;

    for (i = 0; i < width; i++) {
        uint16_t px = pixels[i];
        uint8_t r5 = (uint8_t)((px >> 11) & 0x1F);
        uint8_t g6 = (uint8_t)((px >> 5) & 0x3F);
        uint8_t b5 = (uint8_t)(px & 0x1F);
        uint8_t *d = out + (size_t)i * BMP_BYTES_PER_PIXEL;

        /* widen each part to 8 bits by repeating its top bits in the new low bits */
        d[0] = (uint8_t)((b5 << 3) | (b5 >> 2));
        d[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
        d[2] = (uint8_t)((r5 << 3) | (r5 >> 2));
    }
    memset(out + (size_t)width * BMP_BYTES_PER_PIXEL, 0, size - (size_t)width * BMP_BYTES_PER_PIXEL);
    return size;
}
