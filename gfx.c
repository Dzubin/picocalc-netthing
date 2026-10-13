/*
 * gfx.c - see gfx.h.
 *
 * Author: Thomas Dzubin
 */
#include "lcd.h"

#include "gfx.h"
#include "netthing_const.h"

void gfx_rect(uint16_t colour, int x, int y, int width, int height)
{
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > GFX_SCREEN)
        width = GFX_SCREEN - x;
    if (y + height > GFX_SCREEN)
        height = GFX_SCREEN - y;
    if (width <= 0 || height <= 0)
        return;
    lcd_solid_rectangle(colour, (uint16_t)x, (uint16_t)y, (uint16_t)width, (uint16_t)height);
}

void gfx_frame(uint16_t colour, int x, int y, int width, int height)
{
    gfx_rect(colour, x, y, width, 1);
    gfx_rect(colour, x, y + height - 1, width, 1);
    gfx_rect(colour, x, y, 1, height);
    gfx_rect(colour, x + width - 1, y, 1, height);
}

uint16_t gfx_ms_colour(unsigned ms)
{
    return ms <= LIVE_GOOD_MS ? COL_GREEN : (ms <= LIVE_FAIR_MS ? COL_YELLOW : COL_RED);
}

unsigned gfx_scale_up(unsigned top)
{
    static const unsigned scales[] = GFX_SCALES;
    unsigned i;

    for (i = 0; scales[i] != 0; i++)
        if (top <= scales[i])
            return scales[i];
    return scales[i - 1];
}

unsigned gfx_top(const uint16_t *values, unsigned count)
{
    unsigned i, top = 0;

    for (i = 0; i < count; i++)
        if (values[i] != GFX_LOST && values[i] > top)
            top = values[i];
    return top;
}

void gfx_bars(const uint16_t *values, unsigned count, int x, int y, int bar_width, int step, int height,
              unsigned scale, bool by_ms, uint16_t colour)
{
    unsigned i;

    if (scale == 0)
        scale = 1;
    gfx_rect(COL_WHITE, x - 2, y + height, (int)count * step + 3, 1);
    for (i = 0; i < count; i++) {
        int h, bx = x + (int)i * step;
        uint16_t c;

        if (values[i] == GFX_LOST) {
            h = height;
            c = COL_WHITE;
        } else {
            h = (int)((uint32_t)values[i] * (uint32_t)height / scale);
            if (h < 2)
                h = 2;
            if (h > height)
                h = height;
            c = by_ms ? gfx_ms_colour(values[i]) : colour;
        }
        gfx_rect(c, bx, y + height - h, bar_width, h);
    }
}

void gfx_hbar(int x, int y, int height, int max_width, unsigned value, unsigned scale, uint16_t colour)
{
    int w;

    if (scale == 0)
        scale = 1;
    w = (int)((uint32_t)value * (uint32_t)max_width / scale);
    if (value > 0 && w < 2)
        w = 2;
    if (w > max_width)
        w = max_width;
    gfx_rect(colour, x, y, w, height);
}

unsigned gfx_signal_height(int rssi, unsigned height)
{
    if (rssi >= SIGNAL_TOP_DBM)
        return height;
    if (rssi <= SIGNAL_BOTTOM_DBM)
        return 0;
    return (unsigned)(rssi - SIGNAL_BOTTOM_DBM) * height / (unsigned)(SIGNAL_TOP_DBM - SIGNAL_BOTTOM_DBM);
}
