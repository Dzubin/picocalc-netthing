/*
 * certs.c - the certificate tool: download the Mozilla list of root certificates
 * from curl.se (over https, checked with the CA.PEM there is now), check its
 * SHA-256, keep the roots named in CA-WANT.TXT, and write the new CA.PEM. The
 * old one is kept as CA.BAK. The picking itself is pemselect.c.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "keyboard.h"

#include "platform.h"
#include "net.h"
#include "storage.h"
#include "url.h"
#include "fetch.h"
#include "pemselect.h"
#include "ui.h"
#include "tools.h"
#include "toolsui.h"
#include "netthing_const.h"
#include "certs_const.h"

static char        want_text[CERTS_WANT_MAX + 1];
static const char *want_names[PEMSELECT_NAMES_MAX];
static unsigned    want_count;

/* The sink of the picker: collect text and write it to the new CA.PEM in bigger pieces. */
static char        out_buf[CERTS_CHUNK + 512];
static unsigned    out_len;
static bool        out_failed;

static void out_flush(void)
{
    if (out_len > 0 && storage_out_write(out_buf, out_len) != 0)
        out_failed = true;
    out_len = 0;
}

static void out_sink(void *context, const char *text, size_t length)
{
    (void)context;
    while (length > 0) {
        size_t room = sizeof out_buf - out_len;
        size_t n = length < room ? length : room;

        memcpy(out_buf + out_len, text, n);
        out_len += (unsigned)n;
        text += n;
        length -= n;
        if (out_len == sizeof out_buf)
            out_flush();
    }
}

/* The names the new CA.PEM should hold: the lines of CA-WANT.TXT, or the built-in list
 * (which is then written to CA-WANT.TXT, so that it can be edited on a PC). */
static void load_wanted(void)
{
    static const char *const defaults[] = CERTS_DEFAULT_NAMES;
    size_t len = 0;
    char *p, *line;
    unsigned i;

    want_count = 0;
    if (storage_read(CERTS_WANT_FILE, (uint8_t *)want_text, CERTS_WANT_MAX, &len) != 0 || len == 0) {
        size_t n = 0;

        n += (size_t)snprintf(want_text + n, sizeof want_text - n, "%s", CERTS_WANT_HEADER);
        for (i = 0; defaults[i] != NULL; i++)
            n += (size_t)snprintf(want_text + n, sizeof want_text - n, "%s\r\n", defaults[i]);
        (void)storage_write(CERTS_WANT_FILE, (const uint8_t *)want_text, n);
        len = n;
    }
    want_text[len] = '\0';
    for (line = want_text; *line && want_count < PEMSELECT_NAMES_MAX; line = p) {
        char *end;

        p = strchr(line, '\n');
        end = p ? p : line + strlen(line);
        p = p ? p + 1 : end;
        while (end > line && (end[-1] == '\r' || end[-1] == ' '))
            end--;
        *end = '\0';
        if (line[0] != '\0' && line[0] != '#')
            want_names[want_count++] = line;
    }
}

/* Copy a file to another, piece by piece (the file can be bigger than any buffer there is room for).
 * Returns true if all of it was copied. */
static bool copy_file(const char *from, const char *to)
{
    static uint8_t piece[CERTS_CHUNK];
    size_t n;
    bool ok = true;

    if (storage_in_open(from) != 0)
        return false;
    if (storage_out_open(to) != 0) {
        storage_in_close();
        return false;
    }
    while ((n = storage_in_read(piece, sizeof piece)) > 0)
        if (storage_out_write(piece, n) != 0) {
            ok = false;
            break;
        }
    storage_in_close();
    if (storage_out_close() != 0)
        ok = false;
    return ok;
}

/* Put the old file back (used when the new one could not be made). */
static void restore_backup(void)
{
    (void)copy_file(CERTS_BACKUP_FILE, CA_FILE);
}

/* Copy the CA.PEM there is now to CA.BAK. */
static bool make_backup(void)
{
    return copy_file(CA_FILE, CERTS_BACKUP_FILE);
}

/* The SHA-256 of the downloaded list against the checksum file. */
static bool checksum_matches(void)
{
    static uint8_t chunk[CERTS_CHUNK];
    char hex[65], sum_text[128];
    size_t got = 0, n;
    unsigned i;

    net_sha256_start();
    if (storage_in_open(CERTS_LIST_FILE) != 0) {
        net_sha256_finish(hex);
        return false;
    }
    while ((n = storage_in_read(chunk, sizeof chunk)) > 0)
        net_sha256_update(chunk, n);
    storage_in_close();
    net_sha256_finish(hex);
    if (storage_read(CERTS_SUM_FILE, (uint8_t *)sum_text, sizeof sum_text - 1, &got) != 0 || got < CERTS_SUM_HEX)
        return false;
    for (i = 0; i < CERTS_SUM_HEX; i++) {
        char c = sum_text[i];

        if (c >= 'A' && c <= 'F')
            c = (char)(c - 'A' + 'a');
        if (c != hex[i])
            return false;
    }
    return true;
}

void tool_certs(void)
{
    static const char *const intro[] = CERTS_INTRO;
    static uint8_t chunk[CERTS_CHUNK];
    static pemselect_t picker;
    char line[TOOL_LINE_MAX];
    uint8_t key;
    url_t u;
    uint32_t bytes = 0, done = 0, total;
    size_t n, ignore;
    unsigned i, row;
    int rc;

    tools_screen("UPDATE THE CERTIFICATES", "");
    row = CERTS_ROW_FIRST;
    for (i = 0; intro[i] != NULL; i++)
        put_at(TOOL_COL, (int)row++, intro[i], COL_WHITE);
    put_row(CERTS_ROW_ASK, "CONTINUE? Y/N", COL_YELLOW);
    key = wait_any_key();
    if (key != 'y' && key != 'Y')
        return;

    tools_screen("UPDATE THE CERTIFICATES", "");
    if (storage_read(CA_FILE, chunk, 1, &ignore) == 1) {
        if (!fetch_offer_builtin_ca()) {
            tools_finish();
            return;
        }
        tools_out_row++;
    }
    (void)url_parse(CERTS_SUM_URL, &u);
    tools_out_line("1  THE CHECKSUM", COL_CYAN);
    if (fetch_file(&u, CERTS_SUM_FILE, &bytes) != FETCH_OK) {
        tools_finish();
        return;
    }
    (void)url_parse(CERTS_LIST_URL, &u);
    tools_out_line("2  THE LIST", COL_CYAN);
    if (fetch_file(&u, CERTS_LIST_FILE, &bytes) != FETCH_OK) {
        tools_finish();
        return;
    }
    clear_row(TOOL_ROW_LAST);
    tools_out_line("3  CHECKING THE SHA-256", COL_CYAN);
    if (!checksum_matches()) {
        tools_out_line("THE CHECKSUM DID NOT MATCH.", COL_YELLOW);
        tools_out_line("THE CA.PEM WAS NOT CHANGED.", COL_WHITE);
        tools_finish();
        return;
    }
    tools_out_line("   MATCHES", COL_GREEN);

    tools_out_line("4  BUILDING THE NEW CA.PEM", COL_CYAN);
    load_wanted();
    if (!make_backup()) {
        tools_out_line("COULD NOT KEEP THE OLD FILE AS", COL_YELLOW);
        tools_out_line("CA.BAK. NOTHING WAS CHANGED.", COL_WHITE);
        tools_finish();
        return;
    }
    if (storage_out_open(CA_FILE) != 0) {
        tools_out_line("COULD NOT WRITE CA.PEM.", COL_YELLOW);
        restore_backup();
        tools_finish();
        return;
    }
    out_len = 0;
    out_failed = false;
    pemselect_init(&picker, want_names, want_count, CA_MAX, out_sink, NULL);
    total = bytes;
    rc = storage_in_open(CERTS_LIST_FILE);
    while (rc == 0 && (n = storage_in_read(chunk, sizeof chunk)) > 0) {
        pemselect_feed(&picker, chunk, n);
        done += (uint32_t)n;
        if (poll_key() == KEY_ESC)
            break;
        snprintf(line, sizeof line, "   READING THE LIST %u%%", total ? (unsigned)((uint64_t)done * 100u / total) : 0u);
        clear_row(tools_out_row);
        put_at(TOOL_COL, tools_out_row, line, COL_WHITE);
    }
    storage_in_close();
    pemselect_finish(&picker);
    out_flush();
    if (storage_out_close() != 0)
        out_failed = true;
    tools_out_row++;
    if (out_failed || picker.roots == 0) {
        tools_out_line(picker.roots == 0 ? "NO ROOT CERTIFICATE WAS FOUND." : "COULD NOT WRITE THE NEW FILE.", COL_YELLOW);
        tools_out_line("THE OLD CA.PEM IS BACK.", COL_WHITE);
        restore_backup();
        tools_finish();
        return;
    }
    snprintf(line, sizeof line, "%u ROOTS, %u BYTES", picker.roots, (unsigned)picker.bytes);
    tools_out_line(line, COL_GREEN);
    tools_out_line("THE OLD ONE IS CA.BAK.", COL_WHITE);
    if (picker.too_big)
        tools_out_line("SOME DID NOT FIT IN 48 KB.", COL_YELLOW);
    {
        unsigned missing = 0;

        for (i = 0; i < want_count; i++)
            if (!picker.found[i]) {
                if (missing == 0)
                    tools_out_line("NOT IN THE LIST:", COL_YELLOW);
                if (missing < 4)
                    tools_out_line(want_names[i], COL_WHITE);
                missing++;
            }
        if (missing > 4) {
            snprintf(line, sizeof line, "AND %u MORE", missing - 4);
            tools_out_line(line, COL_WHITE);
        }
    }
    tools_finish();
}
