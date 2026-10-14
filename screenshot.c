/*
 * screenshot.c - see screenshot.h.
 *
 * The screen is read one row at a time (a whole 320 x 320 picture would be 200 KB
 * of RAM), turned into the file's rows and written to the card a few rows at a
 * time. A BMP file keeps its rows bottom up, so the bottom of the screen is read
 * first.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"

#include "platform.h"
#include "storage.h"
#include "ui.h"
#include "lcdread.h"
#include "bmp.h"
#include "netthing_const.h"
#include "screenshot.h"

static uint16_t row_pixels[SHOT_WIDTH];
static uint8_t  batch[SHOT_BATCH_ROWS * SHOT_ROW_BYTES] __attribute__((aligned(4)));
static bool     busy;

/* Find a name that is not used yet. */
static bool free_name(char *name, size_t size, char *path, size_t path_size)
{
    unsigned tries;

    for (tries = 0; tries < SHOT_NAME_TRIES; tries++) {
        uint8_t probe;
        size_t len;

        snprintf(name, size, "SCRN%04X.BMP", (unsigned)(plat_random32() & 0xFFFFu));
        snprintf(path, path_size, "%s/%s", SHOT_DIR, name);
        if (storage_read(path, &probe, 0, &len) == 1)       /* 1: there is no such file */
            return true;
    }
    return false;
}

int screenshot_take(char *name, size_t size)
{
    char path[48];
    uint8_t header[BMP_HEADER_SIZE];
    int row;
    bool ok;

    if (storage_ready() != 0)
        return SHOT_NO_CARD;
    if (!free_name(name, size, path, sizeof path))
        return SHOT_FAILED;
    bmp_header(header, SHOT_WIDTH, SHOT_HEIGHT);
    if (storage_out_open(path) != 0)
        return SHOT_FAILED;
    ok = storage_out_write(header, sizeof header) == 0;
    row = SHOT_HEIGHT - 1;
    while (ok && row >= 0) {
        size_t offset = 0;
        unsigned rows = 0;

        while (row >= 0 && rows < SHOT_BATCH_ROWS) {
            lcd_read_pixels(row_pixels, 0, (uint16_t)row, SHOT_WIDTH, 1);
            offset += bmp_row_from_rgb565(row_pixels, SHOT_WIDTH, batch + offset);
            row--;
            rows++;
        }
        ok = storage_out_write(batch, offset) == 0;
    }
    if (storage_out_close() != 0)
        ok = false;
    return ok ? SHOT_OK : SHOT_FAILED;
}

void screenshot_press(void)
{
    char name[16], text[SHOT_MSG_COLS + 1];
    uint16_t colour = COL_GREEN;
    uint16_t *saved = (uint16_t *)(void *)batch;      /* free again once the file is written */
    int result;

    if (busy)
        return;                 /* F5 pressed again while a save or its message is going on */
    busy = true;
    result = screenshot_take(name, sizeof name);
    if (result == SHOT_OK) {
        snprintf(text, sizeof text, "SAVED %-*s", SHOT_MSG_COLS - 6, name);
    } else {
        colour = COL_YELLOW;
        snprintf(text, sizeof text, "%-*s", SHOT_MSG_COLS, result == SHOT_NO_CARD ? "NO SD CARD" : "SAVE FAILED");
    }
    lcd_read_pixels(saved, 0, SHOT_MSG_Y, SHOT_MSG_PX_W, SHOT_MSG_PX_H);
    put_at(0, SHOT_MSG_ROW, text, colour);
    pause_ms(SHOT_MSG_MS);
    lcd_blit(saved, 0, SHOT_MSG_Y, SHOT_MSG_PX_W, SHOT_MSG_PX_H);
    busy = false;
}
