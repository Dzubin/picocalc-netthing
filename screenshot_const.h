/*
 * screenshot_const.h - constants of screenshot.c, the F5 key that saves the screen
 * as a BMP file on the SD card.
 *
 * Author: Thomas Dzubin
 */
#ifndef SCREENSHOT_CONST_H
#define SCREENSHOT_CONST_H

#include "chip_limits.h"

#define SHOT_WIDTH          320
#define SHOT_HEIGHT         320
#define SHOT_BATCH_ROWS     CHIP_SHOT_ROWS      /* rows of the file written to the card in one piece (chip_limits.h) */
#define SHOT_ROW_BYTES      960                 /* one row in the file: 320 pixels of 3 bytes, already a multiple of 4 */
#define SHOT_DIR            "/netthing/SCREENS"
#define SHOT_NAME_TRIES     20                  /* names SCRNxxxx.BMP tried before giving up */

/* The message after a save is drawn at the left end of the last text row and the pixels under
 * it are put back afterwards, so the screen is as it was. It must fit the batch buffer:
 * SHOT_MSG_PX_W * SHOT_MSG_PX_H * 2 bytes. */
#define SHOT_MSG_ROW        31
#define SHOT_MSG_COLS       22
#define SHOT_MSG_PX_W       (SHOT_MSG_COLS * 8)
#define SHOT_MSG_PX_H       10
#define SHOT_MSG_Y          (SHOT_MSG_ROW * SHOT_MSG_PX_H)
#define SHOT_MSG_MS         1500

#define SHOT_OK             0
#define SHOT_NO_CARD        1
#define SHOT_FAILED         2

#endif /* SCREENSHOT_CONST_H */
