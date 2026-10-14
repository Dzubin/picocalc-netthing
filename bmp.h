/*
 * bmp.h - the pieces of an uncompressed 24-bit BMP file that a screenshot needs: the
 * file header, and one row of RGB565 pixels turned into the file's bottom-up BGR
 * rows with their padding. Plain portable C; reading the screen and writing the
 * file are screenshot.c's job.
 *
 * Author: Thomas Dzubin
 */
#ifndef BMP_H
#define BMP_H

#include <stddef.h>
#include <stdint.h>

#include "bmp_const.h"

/* The size in bytes of one pixel row of this width in the file (3 bytes a pixel,
 * padded to a multiple of 4). */
size_t bmp_row_size(unsigned width);

/* Fill in the BMP_HEADER_SIZE bytes of the file header for an image of this size
 * (rows to follow in the file bottom row first, each bmp_row_size() bytes). */
void bmp_header(uint8_t out[BMP_HEADER_SIZE], unsigned width, unsigned height);

/* Turn `width` RGB565 pixels into one file row at `out` (bmp_row_size(width)
 * bytes, padding included). Returns the number of bytes written. */
size_t bmp_row_from_rgb565(const uint16_t *pixels, unsigned width, uint8_t *out);

#endif /* BMP_H */
