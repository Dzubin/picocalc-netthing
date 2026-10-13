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
#define SECRET_BSSID_LEN    6       /* an access point address, in bytes */
#define SECRET_BSSID_MAX    4       /* access points remembered per network */

/* File layout: four clear bytes SECRET_MAGIC, a four byte salt (clear, little
 * endian), then the scrambled body: one byte count, per network one byte name
 * length, the name, one byte password length, the password, one byte count of
 * access point addresses and those addresses (six bytes each), and last the
 * CRC-32 of the body so far (little endian). The older file, SECRET_MAGIC_V1,
 * has no addresses; it is still read, and the next save writes the new kind. */
#define SECRET_MAGIC        "NTW2"
#define SECRET_MAGIC_V1     "NTW1"
#define SECRET_MAGIC_LEN    4
#define SECRET_SALT_LEN     4
#define SECRET_CRC_LEN      4
#define SECRET_HEADER_LEN   (SECRET_MAGIC_LEN + SECRET_SALT_LEN)
#define SECRET_FILE_MAX     (SECRET_HEADER_LEN + 1 + SECRET_NETS_MAX * (3 + SECRET_SSID_MAX + SECRET_PASS_MAX + SECRET_BSSID_MAX * SECRET_BSSID_LEN) + SECRET_CRC_LEN)

/* What secret_match() answers for a scanned access point. */
#define SECRET_MATCH_NONE   0       /* neither the name nor the address is saved */
#define SECRET_MATCH_ONE    1       /* the name or the address is saved, not both together */
#define SECRET_MATCH_BOTH   2       /* a saved network has this name and this address */

/* The scrambling key: a pair of seeds for the generator that makes the
 * stream of bytes XORed into the body. The salt of the file is mixed in. */
#define SECRET_SEED_A       0x4E657454u
#define SECRET_SEED_B       0x68696E67u
#define SECRET_MIX          0x9E3779B9u

#endif /* SECRET_CONST_H */
