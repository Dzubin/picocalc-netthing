/*
 * screenshot.h - save what is on the LCD as a 24-bit BMP file on the SD card
 * (/netthing/SCREENS/SCRNxxxx.BMP, xxxx four random hex digits). ui.c calls
 * screenshot_press() when F5 is pressed, so it works on every screen.
 *
 * Author: Thomas Dzubin
 */
#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stddef.h>

#include "screenshot_const.h"

/* Save the whole screen. On SHOT_OK the file name (without the folder) is copied to name. */
int screenshot_take(char *name, size_t size);

/* The F5 key: save the screen, then say so for a moment on the left of the last text row. */
void screenshot_press(void);

#endif /* SCREENSHOT_H */
