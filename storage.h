/*
 * storage.h - small whole files on the SD card, as netthing.c needs them: read
 * one into memory, write one from memory (the folder is made if needed). The
 * implementation is storage_pico.c (the vendored FAT32 driver). A write is read
 * back and compared, and tried again a few times, because writes to the real
 * card fail now and then.
 *
 * Author: Thomas Dzubin
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdint.h>

/* 0 if a card is present and the file system mounted. */
int storage_ready(void);

/* Read the whole file into buf (at most max bytes). Returns 0 and sets *len,
 * 1 if there is no such file, 2 for no card or a read error, 3 if the file
 * does not fit. */
int storage_read(const char *path, uint8_t *buf, size_t max, size_t *len);

/* Replace the file with these bytes, making the folder first if it is not
 * there. Returns 0 only when the bytes read back equal; 1 for no card, 2 for a
 * write or read-back failure. */
int storage_write(const char *path, const uint8_t *data, size_t len);

/* A file written in pieces (a download): open it (folders are made, an old file
 * of the same name is replaced), write as often as needed, close. The calls
 * return 0, or non-zero (1 no card or nothing open, 2 a write failed). Only one
 * such file can be open at a time. */
int storage_out_open(const char *path);
int storage_out_write(const void *data, size_t length);
int storage_out_close(void);

/* A file read in pieces (a big one): open it, read until storage_in_read() returns 0, close.
 * storage_in_open() returns 0, 1 for no card, 2 if there is no such file. */
int    storage_in_open(const char *path);
size_t storage_in_read(void *buffer, size_t max);
void   storage_in_close(void);

#endif /* STORAGE_H */
