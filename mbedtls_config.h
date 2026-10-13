/*
 * mbedtls_config.h - what the Mbed TLS library is built with for PicoCalc NetThing:
 * a TLS 1.2 client that checks the server's certificate against root
 * certificates read from the SD card. Only what a client needs is enabled, to
 * keep the program small. (TLS 1.3 is off, since it needs the PSA crypto
 * layer; nearly every server still accepts TLS 1.2.)
 *
 * Author: Thomas Dzubin
 */
#ifndef NETTHING_MBEDTLS_CONFIG_H
#define NETTHING_MBEDTLS_CONFIG_H

/* the system */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT        /* the Pico SDK supplies it from its random number source */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY             /* lwIP's altcp layer supplies calloc and free */
#define MBEDTLS_PLATFORM_TIME_ALT           /* the clock comes from mbedtls_platform_set_time() */
#define MBEDTLS_PLATFORM_MS_TIME_ALT        /* a millisecond counter, supplied by net_stream_pico.c */

/* random numbers */
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

/* ciphers and hashes */
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA224_C

/* big numbers, public keys and certificates */
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_OID_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CHECK_KEY_USAGE
#define MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE

/* TLS */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_IN_CONTENT_LEN   16384  /* servers may send full-size records */
#define MBEDTLS_SSL_OUT_CONTENT_LEN   2048  /* we send only a short request */

/* readable error messages and a trace for the failure screen */
#define MBEDTLS_ERROR_C

#endif /* NETTHING_MBEDTLS_CONFIG_H */
