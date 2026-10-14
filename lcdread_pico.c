/*
 * lcdread_pico.c - see lcdread.h.
 *
 * The read is hand-made because RAMRD needs the chip select held low from the
 * command through every byte read back, while the driver's lcd_write_cmd() raises
 * it again at once. Interrupts are off for the read, as in lcd_blit(): the cursor
 * timer would otherwise write to the same SPI bus in the middle of it.
 *
 * Author: Thomas Dzubin
 */
#include "hardware/spi.h"
#include "hardware/sync.h"

#include "lcd.h"

#include "lcdread.h"
#include "lcdread_pico_const.h"

static uint8_t read_row[LCDREAD_ROW_PIXELS * LCDREAD_BYTES_PER_PIXEL];

void lcd_read_pixels(uint16_t *pixels, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint32_t irq_state;
    uint8_t dummy[LCDREAD_DUMMY_BYTES], cmd = LCD_CMD_RAMRD;
    uint16_t row, col;

    if (width == 0 || height == 0 || width > LCDREAD_ROW_PIXELS)
        return;
    irq_state = save_and_disable_interrupts();

    /* the window to read, as lcd_blit() sets the window to write */
    lcd_write_cmd(LCD_CMD_CASET);
    lcd_write_data(4, UPPER8(x), LOWER8(x), UPPER8((uint16_t)(x + width - 1)), LOWER8((uint16_t)(x + width - 1)));
    lcd_write_cmd(LCD_CMD_RASET);
    lcd_write_data(4, UPPER8(y), LOWER8(y), UPPER8((uint16_t)(y + height - 1)), LOWER8((uint16_t)(y + height - 1)));

    spi_set_baudrate(LCD_SPI, LCDREAD_BAUDRATE);
    gpio_put(LCD_DCX, 0);               /* a command */
    gpio_put(LCD_CSX, 0);               /* selected until the whole read is done */
    spi_write_blocking(LCD_SPI, &cmd, 1);
    gpio_put(LCD_DCX, 1);               /* data from here */
    spi_read_blocking(LCD_SPI, 0x00, dummy, LCDREAD_DUMMY_BYTES);

    for (row = 0; row < height; row++) {
        const uint8_t *in = read_row;
        uint16_t *out = pixels + (size_t)row * width;

        spi_read_blocking(LCD_SPI, 0x00, read_row, (size_t)width * LCDREAD_BYTES_PER_PIXEL);
        for (col = 0; col < width; col++, in += LCDREAD_BYTES_PER_PIXEL)
            out[col] = (uint16_t)((in[0] << 8) | in[1]);
    }

    gpio_put(LCD_CSX, 1);               /* only now may a new command go out */
    spi_set_baudrate(LCD_SPI, LCD_BAUDRATE);
    restore_interrupts(irq_state);
}
