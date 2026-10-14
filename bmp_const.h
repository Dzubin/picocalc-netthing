/*
 * bmp_const.h - constants of bmp.c: the BMP file layout (a BITMAPFILEHEADER and a
 * BITMAPINFOHEADER, uncompressed 24-bit BI_RGB).
 *
 * Author: Thomas Dzubin
 */
#ifndef BMP_CONST_H
#define BMP_CONST_H

#define BMP_FILE_HEADER_SIZE    14
#define BMP_INFO_HEADER_SIZE    40
#define BMP_HEADER_SIZE         (BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE)
#define BMP_BITS_PER_PIXEL      24
#define BMP_BYTES_PER_PIXEL     3
#define BMP_ROW_ALIGN           4       /* every row of the file is a multiple of this many bytes */
#define BMP_COMPRESSION_BI_RGB  0

#endif /* BMP_CONST_H */
