/*
 * lcdread.h - reading pixels back off the LCD, the mirror of lcd_blit(): the one
 * thing the display driver has no call for. The implementation is lcdread_pico.c
 * (the ST7365P's RAMRD command on the PicoCalc's SPI bus); screenshot.c sees only
 * this call.
 *
 * Author: Thomas Dzubin
 */
#ifndef LCDREAD_H
#define LCDREAD_H

#include <stdint.h>

/* Read a width x height block of RGB565 pixels, its top left corner at (x, y), into
 * `pixels` (width * height entries, row by row, as lcd_blit() takes them). Slower
 * than writing: the bus is slowed down for the read. Interrupts are off while a read
 * lasts, so keep the blocks small (a row at a time). */
void lcd_read_pixels(uint16_t *pixels, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

#endif /* LCDREAD_H */
