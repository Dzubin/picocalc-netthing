/*
 * lcdread_pico_const.h - constants of lcdread_pico.c. They were found on real
 * hardware by filling a rectangle with one colour and looking at the raw bytes
 * the LCD gives back: with the pixel format set to 16 bits (which lcd_init()
 * always does) RAMRD gives the pixels back as they were written, two bytes each,
 * most significant first, after one dummy byte.
 *
 * Author: Thomas Dzubin
 */
#ifndef LCDREAD_PICO_CONST_H
#define LCDREAD_PICO_CONST_H

#define LCDREAD_BAUDRATE       1000000u    /* the bus speed while reading; known to work, not known how much faster could */
#define LCDREAD_DUMMY_BYTES    1           /* thrown away before the first pixel */
#define LCDREAD_BYTES_PER_PIXEL 2          /* big-endian RGB565 */
#define LCDREAD_ROW_PIXELS     320         /* the widest block read in one go */

#endif /* LCDREAD_PICO_CONST_H */
