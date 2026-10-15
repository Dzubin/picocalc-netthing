/*
 * ui.h - the screen and keyboard helpers shared by every screen of PicoCalc
 * NetThing: rows of text, polling the keyboard (with the ~ key always leading
 * to BOOTSEL), short notices, signal bars, and typing a line of text.
 *
 * Author: Thomas Dzubin
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

/* ---- text on the screen ------------------------------------------------ */
void screen_clear(void);        /* clears the screen and draws the link line (link.h) */
void clear_row(int row);
void put_row(int row, const char *str, uint16_t fg);            /* centred   */
void put_at(int col, int row, const char *str, uint16_t fg);    /* no clear  */
void put_at_reverse(int col, int row, const char *str, uint16_t fg);   /* text in the background colour on fg */

/* ---- the keyboard ------------------------------------------------------ */
void    drain_keys(void);
uint8_t poll_key_raw(void);     /* a key just pressed (a tilde too), or 0    */
uint8_t poll_key(void);         /* the same, but ~ goes to BOOTSEL here      */
uint8_t wait_any_key(void);     /* waits; ~ goes to BOOTSEL                  */
void    pause_ms(uint32_t ms);  /* waits; ~ goes to BOOTSEL                  */
void    notice(int row, const char *text, uint16_t colour);   /* up for a moment */

void go_bootsel(void);          /* reboot into BOOTSEL (USB drive) mode      */
void go_loader(void);           /* back to the UF2 Loader menu               */

/* ---- how often the scan screens update their lists --------------------- */
unsigned scan_interval_get(void);               /* in milliseconds */
void     scan_interval_change(int steps);       /* by SCAN_INTERVAL_STEP_MS each, within the limits */
void     scan_interval_text(char *out, unsigned size);   /* "1.0S" */

/* ---- signal bars -------------------------------------------------------- */
unsigned bars_for(int rssi);            /* 0 to BAR_CELLS for a level in dBm */
uint16_t bars_colour(unsigned bars);    /* green, yellow or red              */

/* ---- typing a line of text --------------------------------------------- */
/* Edit buf (at most max characters) until Enter (true) or ESC (false). TAB
 * shows or hides the text when `secret`. One tilde is typed; two within
 * TILDE_DOUBLE_MS go to BOOTSEL. */
bool edit_text(char *buf, unsigned max, bool secret);

/* A whole entry screen: heading, a name line (may be empty), a label and the
 * field. The text must be at least `min` characters, or empty if allow_empty.
 * Returns false if the operator gave up. */
bool ask_text(const char *heading, const char *name, const char *label, char *buf,
              unsigned max, unsigned min, bool secret, bool allow_empty);

#endif /* UI_H */
