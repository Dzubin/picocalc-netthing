/*
 * storage_pico.c - storage.h on the vendored FAT32 driver (picocalc-text-starter
 * by Blair Leduc) and its SD card driver.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "sdcard.h"
#include "fat32.h"

#include "storage.h"
#include "storage_pico_const.h"

static bool driver_started;

static void start_driver(void)
{
    if (!driver_started) {
        fat32_init();               /* sets up the SD card pins; no card access yet */
        driver_started = true;
    }
}

int storage_ready(void)
{
    start_driver();
    return fat32_is_ready() ? 0 : 1;
}

int storage_read(const char *path, uint8_t *buf, size_t max, size_t *len)
{
    fat32_file_t file;
    size_t got = 0;
    fat32_error_t e;

    *len = 0;
    if (storage_ready() != 0)
        return 2;
    e = fat32_open(&file, path);
    if (e != FAT32_OK)
        return (e == FAT32_ERROR_FILE_NOT_FOUND || e == FAT32_ERROR_DIR_NOT_FOUND) ? 1 : 2;
    if (fat32_size(&file) > max) {
        fat32_close(&file);
        return 3;
    }
    e = fat32_read(&file, buf, fat32_size(&file), &got);
    fat32_close(&file);
    if (e != FAT32_OK)
        return 2;
    *len = got;
    return 0;
}

/* Make every folder of a path ("/netthing/DOWNLOAD/PAGE.HTM" makes "/netthing"
 * and "/netthing/DOWNLOAD" if they are not there). */
static void make_folders(const char *path)
{
    char dir[STORAGE_DIR_MAX];
    const char *slash;
    fat32_file_t folder;

    for (slash = strchr(path + 1, '/'); slash != NULL; slash = strchr(slash + 1, '/')) {
        size_t n = (size_t)(slash - path);

        if (n >= sizeof dir)
            return;
        memcpy(dir, path, n);
        dir[n] = '\0';
        if (fat32_open(&folder, dir) == FAT32_OK)
            fat32_close(&folder);               /* already there */
        else if (fat32_dir_create(&folder, dir) == FAT32_OK)
            fat32_close(&folder);
    }
}

static int write_once(const char *path, const uint8_t *data, size_t len)
{
    fat32_file_t file;
    size_t put = 0;
    fat32_error_t e;

    (void)fat32_delete(path);       /* replace; fine if it is not there */
    if (fat32_create(&file, path) != FAT32_OK)
        return 1;
    e = fat32_write(&file, data, len, &put);
    if (fat32_close(&file) != FAT32_OK || e != FAT32_OK || put != len)
        return 1;
    return 0;
}

int storage_write(const char *path, const uint8_t *data, size_t len)
{
    static uint8_t check[STORAGE_VERIFY_MAX];
    unsigned attempt;

    if (len > sizeof check)
        return 2;
    if (storage_ready() != 0)
        return 1;
    make_folders(path);
    for (attempt = 0; attempt < STORAGE_TRIES; attempt++) {
        size_t got = 0;

        if (write_once(path, data, len) != 0)
            continue;
        if (storage_read(path, check, sizeof check, &got) == 0 && got == len &&
            memcmp(check, data, len) == 0)
            return 0;
    }
    return 2;
}

/* ---- one file written in pieces ------------------------------------------- */
static fat32_file_t out_file;
static bool out_is_open;

int storage_out_open(const char *path)
{
    if (out_is_open) {
        (void)fat32_close(&out_file);
        out_is_open = false;
    }
    if (storage_ready() != 0)
        return 1;
    make_folders(path);
    (void)fat32_delete(path);
    if (fat32_create(&out_file, path) != FAT32_OK)
        return 2;
    out_is_open = true;
    return 0;
}

int storage_out_write(const void *data, size_t length)
{
    size_t put = 0;

    if (!out_is_open)
        return 1;
    if (fat32_write(&out_file, data, length, &put) != FAT32_OK || put != length)
        return 2;
    return 0;
}

int storage_out_close(void)
{
    fat32_error_t e;

    if (!out_is_open)
        return 1;
    out_is_open = false;
    e = fat32_close(&out_file);
    return e == FAT32_OK ? 0 : 2;
}

/* ---- one file read in pieces ---------------------------------------------- */
static fat32_file_t in_file;
static bool in_is_open;

int storage_in_open(const char *path)
{
    storage_in_close();
    if (storage_ready() != 0)
        return 1;
    if (fat32_open(&in_file, path) != FAT32_OK)
        return 2;
    in_is_open = true;
    return 0;
}

size_t storage_in_read(void *buffer, size_t max)
{
    size_t got = 0;

    if (!in_is_open || fat32_read(&in_file, buffer, max, &got) != FAT32_OK)
        return 0;
    return got;
}

void storage_in_close(void)
{
    if (in_is_open) {
        (void)fat32_close(&in_file);
        in_is_open = false;
    }
}
