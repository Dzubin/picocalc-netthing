/*
 * certs_const.h - constants of certs.c, the tool that updates the root
 * certificates the program trusts for https.
 *
 * Author: Thomas Dzubin
 */
#ifndef CERTS_CONST_H
#define CERTS_CONST_H

/* Where the Mozilla list of root certificates comes from, and its SHA-256 */
#define CERTS_LIST_URL      "https://curl.se/ca/cacert.pem"
#define CERTS_SUM_URL       "https://curl.se/ca/cacert.pem.sha256"

/* Files on the SD card */
#define CERTS_LIST_FILE     "/netthing/DOWNLOAD/CACERT.PEM"     /* the list as downloaded */
#define CERTS_SUM_FILE      "/netthing/DOWNLOAD/CACERT.SHA"     /* its checksum, as downloaded */
#define CERTS_WANT_FILE     "/netthing/CA-WANT.TXT"             /* the names of the roots to keep, one per line */
#define CERTS_BACKUP_FILE   "/netthing/CA.BAK"                  /* the CA.PEM that was replaced */

#define CERTS_CHUNK         2048        /* bytes read from a file at a time */
#define CERTS_WANT_MAX      4096        /* the most of the names file that is read */
#define CERTS_SUM_HEX       64          /* hex digits of a SHA-256 */

/* The roots kept when there is no names file: widely used ones, the same as
 * tools/make_ca_pem.py chooses. The names are exactly as in the Mozilla list. */
#define CERTS_DEFAULT_NAMES { \
    "ISRG Root X1", "ISRG Root X2", \
    "DigiCert Global Root G2", "DigiCert Global Root G3", "DigiCert TLS ECC P384 Root G5", \
    "DigiCert TLS RSA4096 Root G5", "DigiCert Trusted Root G4", \
    "GlobalSign Root CA - R3", "GlobalSign Root R46", "GlobalSign Root E46", "GlobalSign ECC Root CA - R5", \
    "Go Daddy Root Certificate Authority - G2", "Starfield Root Certificate Authority - G2", \
    "USERTrust RSA Certification Authority", "USERTrust ECC Certification Authority", \
    "COMODO RSA Certification Authority", \
    "Amazon Root CA 1", "Amazon Root CA 2", "Amazon Root CA 3", "Amazon Root CA 4", \
    "GTS Root R1", "GTS Root R3", "GTS Root R4", \
    "Microsoft RSA Root Certificate Authority 2017", "Microsoft ECC Root Certificate Authority 2017", \
    "Sectigo Public Server Authentication Root R46", "Sectigo Public Server Authentication Root E46", \
    "IdenTrust Commercial Root CA 1", \
    "SSL.com TLS ECC Root CA 2022", "SSL.com TLS RSA Root CA 2022", \
    "SSL.com Root Certification Authority ECC", "SSL.com Root Certification Authority RSA", NULL }

#define CERTS_WANT_HEADER   "# Names of the root certificates to keep in CA.PEM, one per line, exactly as in the\r\n" \
                            "# Mozilla list at https://curl.se/ca/cacert.pem. Lines that start with # are ignored.\r\n" \
                            "# Edit this file on a PC, then use the certificate tool of the program again.\r\n"

/* The screen */
#define CERTS_ROW_FIRST       5
#define CERTS_ROW_ASK        26
#define CERTS_INTRO { \
    "THE PICOCALC DOWNLOADS THE LIST OF", "TRUSTED ROOT CERTIFICATES FROM", \
    "CURL.SE (ABOUT 190 KB), CHECKS ITS", "SHA-256, AND BUILDS /netthing/CA.PEM", \
    "FROM THE NAMES IN CA-WANT.TXT.", "", "THE OLD CA.PEM IS KEPT AS CA.BAK.", \
    "THE DOWNLOAD IS CHECKED WITH THE", "CA.PEM YOU HAVE NOW.", NULL }

#endif /* CERTS_CONST_H */
