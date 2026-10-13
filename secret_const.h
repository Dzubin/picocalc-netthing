/*
 * secret_const.h - constants of secret.c, the scrambled list of saved networks.
 *
 * The key below is in the program, so this only hides the passwords from a
 * casual look at the SD card (a file that reads as noise, different at every
 * save). Anyone who has the program can undo it. It is not encryption.
 *
 * Author: Thomas Dzubin
 */
#ifndef SECRET_CONST_H
#define SECRET_CONST_H

#define SECRET_SSID_MAX     32      /* longest network name */
#define SECRET_PASS_MAX     63      /* longest WPA passphrase */
#define SECRET_NETS_MAX     16      /* networks the file can hold */

/* File layout: four clear bytes SECRET_MAGIC, a four byte salt (clear, little
 * endian), then the scrambled body: one byte count, per network one byte name
 * length, the name, one byte password length, the password, and last the CRC-32
 * of the body so far (little endian). */
#define SECRET_MAGIC        "NTW1"
#define SECRET_MAGIC_LEN    4
#define SECRET_SALT_LEN     4
#define SECRET_CRC_LEN      4
#define SECRET_HEADER_LEN   (SECRET_MAGIC_LEN + SECRET_SALT_LEN)
#define SECRET_FILE_MAX     (SECRET_HEADER_LEN + 1 + SECRET_NETS_MAX * (2 + SECRET_SSID_MAX + SECRET_PASS_MAX) + SECRET_CRC_LEN)

/* The scrambling key: a pair of seeds for the generator that makes the
 * stream of bytes XORed into the body. The salt of the file is mixed in. */
#define SECRET_SEED_A       0x4E657454u
#define SECRET_SEED_B       0x68696E67u
#define SECRET_MIX          0x9E3779B9u

#endif /* SECRET_CONST_H */
