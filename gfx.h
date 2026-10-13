/*
 * gfx.h - the little pictures the tools draw: bar charts, single bars, outlines.
 * Everything is drawn with filled rectangles of the LCD driver; positions and
 * sizes are in pixels (the screen is 320 x 320, a text cell is 8 x 10).
 *
 * Author: Thomas Dzubin
 */
#ifndef GFX_H
#define GFX_H

#include <stdbool.h>
#include <stdint.h>

#define GFX_LOST 0xFFFF         /* a value for "no answer": drawn as a full-height white bar */

/* A filled rectangle, cut to the screen. */
void gfx_rect(uint16_t colour, int x, int y, int width, int height);

/* A one pixel outline. */
void gfx_frame(uint16_t colour, int x, int y, int width, int height);

/* Green up to LIVE_GOOD_MS, yellow up to LIVE_FAIR_MS, red above (a time in milliseconds). */
uint16_t gfx_ms_colour(unsigned ms);

/* The smallest round number (10, 20, 50, 100, 200, 500, 1000...) that is at least `top`. */
unsigned gfx_scale_up(unsigned top);

/* The largest value in the list that is not GFX_LOST (0 for none). */
unsigned gfx_top(const uint16_t *values, unsigned count);

/* A bar chart with the bottom edge at y + height: bar i is at x + i * step, bar_width wide, as tall
 * as value * height / scale. Values of GFX_LOST are full height and white. With by_ms the bars
 * take their colour from gfx_ms_colour(), otherwise `colour`. A baseline is drawn under the chart. */
void gfx_bars(const uint16_t *values, unsigned count, int x, int y, int bar_width, int step, int height,
              unsigned scale, bool by_ms, uint16_t colour);

/* One horizontal bar starting at x, `height` tall, as long as value * max_width / scale (at least 2
 * pixels for a value above 0, never more than max_width). */
void gfx_hbar(int x, int y, int height, int max_width, unsigned value, unsigned scale, uint16_t colour);

/* The height of a picture of a signal strength: 0 at SIGNAL_BOTTOM_DBM or weaker, `height` at
 * SIGNAL_TOP_DBM or stronger. */
unsigned gfx_signal_height(int rssi, unsigned height);

#endif /* GFX_H */
