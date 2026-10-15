/*
 * ui.c - see ui.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* WIDTH, GLYPH_HEIGHT, RGB(), lcd_*()              */
#include "southbridge.h"  /* sb_read_keyboard()                               */
#include "keyboard.h"     /* KEY_* codes, KEY_STATE_* values                  */

#include "platform.h"
#include "ui.h"
#include "link.h"
#include "screenshot.h"
#include "netthing_const.h"

/* ===================================================================== */
/*  Text on the screen                                                    */
/* ===================================================================== */
void screen_clear(void)
{
    lcd_clear_screen();
    link_draw();
}

void clear_row(int row)
{
    lcd_solid_rectangle(COL_BG, 0, (uint16_t)(row * GLYPH_HEIGHT), WIDTH,
                        GLYPH_HEIGHT);
}

/* clear a row and print a string centred on it */
void put_row(int row, const char *str, uint16_t fg)
{
    clear_row(row);
    int len = (int)strlen(str);
    int col = (TCOLS - len) / 2;
    if (col < 0)
        col = 0;
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* print a string at a column without clearing the row first */
void put_at(int col, int row, const char *str, uint16_t fg)
{
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* text in the background colour on a block of colour fg */
void put_at_reverse(int col, int row, const char *str, uint16_t fg)
{
    lcd_set_background(fg);
    lcd_set_foreground(COL_BG);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
    lcd_set_background(COL_BG);
}

/* one setting for both scan screens, kept while the program runs */
static unsigned scan_interval = SCAN_INTERVAL_DEFAULT_MS;

unsigned scan_interval_get(void)
{
    return scan_interval;
}

void scan_interval_change(int steps)
{
    int now = (int)scan_interval + steps * SCAN_INTERVAL_STEP_MS;

    if (now < SCAN_INTERVAL_MIN_MS)
        now = SCAN_INTERVAL_MIN_MS;
    if (now > SCAN_INTERVAL_MAX_MS)
        now = SCAN_INTERVAL_MAX_MS;
    scan_interval = (unsigned)now;
}

void scan_interval_text(char *out, unsigned size)
{
    snprintf(out, size, "%u.%uS", scan_interval / 1000u, scan_interval % 1000u / 100u);
}

/* ===================================================================== */
/*  Keyboard helpers (raw south-bridge FIFO polling)                      */
/* ===================================================================== */
static inline uint16_t key_event(void)      { return sb_read_keyboard(); }
static inline uint8_t  ev_state(uint16_t e) { return e >> 8; }
static inline uint8_t  ev_code(uint16_t e)  { return e & 0xFF; }

/* true for a bare SHIFT / CTRL / ALT / SYM modifier key code */
static inline bool is_mod_key(uint8_t code)
{
    return code == KEY_MOD_SHL || code == KEY_MOD_SHR || code == KEY_MOD_CTRL ||
           code == KEY_MOD_ALT || code == KEY_MOD_SYM;
}

/* reboot into BOOTSEL (USB drive) mode; does not return */
void go_bootsel(void)
{
    link_drop();                /* leave the network properly before the reset */
    put_row(BOOTSEL_NOTICE_ROW, "REBOOTING TO BOOTSEL...", COL_YELLOW);
    plat_sleep_ms(REBOOT_MS);
    plat_bootsel();
}

/* leave for the UF2 Loader menu (ESC or Q on the splash); does not return */
void go_loader(void)
{
    link_drop();
    put_row(SPLASH_ROW_EXIT, "BACK TO THE LOADER...", COL_YELLOW);
    plat_sleep_ms(REBOOT_MS);
    plat_exit_to_loader();
}

void drain_keys(void)
{
    while (key_event() != 0)
        ;                     /* reading the FIFO empties it */
}

/* Look at the keyboard once: the code of a key that was just pressed (a tilde
 * included), or 0 for none. */
uint8_t poll_key_raw(void)
{
    uint16_t e = key_event();

    if (e && ev_state(e) == KEY_STATE_PRESSED && !is_mod_key(ev_code(e))) {
        if (ev_code(e) == KEY_F5) {
            screenshot_press();     /* F5 saves the screen, on every screen */
            return 0;
        }
        return ev_code(e);
    }
    return 0;
}

/* Like poll_key_raw(), but '~' goes to BOOTSEL here, so every wait that calls
 * this (a scan, a join, a fetch) can be left that way. */
uint8_t poll_key(void)
{
    uint8_t code = poll_key_raw();

    if (code == '~')
        go_bootsel();
    return code;
}

/* wait for a real key press and return its code ('~' still goes to BOOTSEL) */
uint8_t wait_any_key(void)
{
    uint8_t code;

    drain_keys();
    while ((code = poll_key()) == 0)
        plat_sleep_ms(POLL_MS);
    drain_keys();
    return code;
}

/* wait a while, still letting '~' reach BOOTSEL */
void pause_ms(uint32_t ms)
{
    uint32_t until = plat_now_ms() + ms;

    drain_keys();
    while ((int32_t)(until - plat_now_ms()) > 0) {
        (void)poll_key();
        plat_sleep_ms(POLL_MS);
    }
}

/* a short message on a row, left up for a moment */
void notice(int row, const char *text, uint16_t colour)
{
    put_row(row, text, colour);
    pause_ms(NOTICE_MS);
    clear_row(row);
}

/* ===================================================================== */
/*  Signal bars                                                           */
/* ===================================================================== */
/* 0 to BAR_CELLS bars for a signal strength in dBm */
unsigned bars_for(int rssi)
{
    static const int limit[BAR_CELLS] = BAR_LIMITS;
    unsigned i;

    for (i = 0; i < BAR_CELLS; i++)
        if (rssi >= limit[i])
            return BAR_CELLS - i;
    return 0;
}

/* strong signals green, middling yellow, weak red */
uint16_t bars_colour(unsigned bars)
{
    if (bars >= BARS_GOOD)
        return COL_GREEN;
    if (bars >= BARS_FAIR)
        return COL_YELLOW;
    return COL_RED;
}

/* ===================================================================== */
/*  Typing a name or a password                                           */
/* ===================================================================== */
/* Draw the entry field: "> " and the end of what was typed (as asterisks when
 * hidden), then the cursor. */
static void draw_entry(const char *buf, unsigned len, bool hide)
{
    char line[TCOLS + 1];
    unsigned shown = len > PROMPT_WIDTH ? PROMPT_WIDTH : len;
    unsigned i, start = len - shown;

    for (i = 0; i < shown; i++)
        line[i] = hide ? PROMPT_MASK : buf[start + i];
    line[shown] = PROMPT_CURSOR;
    line[shown + 1] = '\0';
    clear_row(PROMPT_ROW_INPUT);
    put_at(PROMPT_COL_LEFT, PROMPT_ROW_INPUT, ">", COL_GREEN);
    put_at(PROMPT_COL_LEFT + 2, PROMPT_ROW_INPUT, line, COL_WHITE);
}

bool edit_text(char *buf, unsigned max, bool secret)
{
    unsigned len = (unsigned)strlen(buf);
    bool hide = secret, changed = true, tilde_pending = false;
    uint32_t tilde_at = 0;

    put_row(PROMPT_ROW_HELP, secret ? PROMPT_HELP_HIDDEN : PROMPT_HELP_NAME, COL_WHITE);
    drain_keys();
    for (;;) {
        uint8_t code;

        if (changed) {
            draw_entry(buf, len, hide);
            changed = false;
        }
        code = poll_key_raw();
        if (code == 0) {
            plat_sleep_ms(POLL_MS);
            continue;
        }
        if (code == '~') {
            uint32_t now = plat_now_ms();

            if (tilde_pending && now - tilde_at <= TILDE_DOUBLE_MS)
                go_bootsel();
            tilde_pending = true;
            tilde_at = now;
        } else {
            tilde_pending = false;
        }
        if (code == KEY_ESC)
            return false;
        if (code == KEY_ENTER || code == KEY_RETURN)
            return true;
        if (code == KEY_BACKSPACE && len > 0) {
            buf[--len] = '\0';
            changed = true;
        } else if (code == KEY_TAB && secret) {
            hide = !hide;
            put_row(PROMPT_ROW_HELP, hide ? PROMPT_HELP_HIDDEN : PROMPT_HELP_SHOWN, COL_WHITE);
            changed = true;
        } else if (code >= 0x20 && code < 0x7F && len < max) {
            buf[len++] = (char)code;
            buf[len] = '\0';
            changed = true;
        }
    }
}

bool ask_text(const char *heading, const char *name, const char *label, char *buf,
              unsigned max, unsigned min, bool secret, bool allow_empty)
{
    char note[TCOLS + 1];

    for (;;) {
        screen_clear();
        put_row(PROMPT_ROW_HEADING, heading, COL_CYAN);
        if (name[0])
            put_row(PROMPT_ROW_NAME, name, COL_WHITE);
        put_at(PROMPT_COL_LEFT, PROMPT_ROW_LABEL, label, COL_CYAN);
        if (!edit_text(buf, max, secret))
            return false;
        if ((buf[0] == '\0' && allow_empty) || strlen(buf) >= min)
            return true;
        snprintf(note, sizeof note, "AT LEAST %u CHARACTERS", min);
        notice(PROMPT_ROW_NOTE, note, COL_YELLOW);
    }
}
