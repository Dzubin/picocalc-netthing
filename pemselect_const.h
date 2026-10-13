/*
 * pemselect_const.h - constants of pemselect.c: the states of the reader.
 *
 * Author: Thomas Dzubin
 */
#ifndef PEMSELECT_CONST_H
#define PEMSELECT_CONST_H

#define S_SEEK     0        /* looking for a line of equals signs, the name is the line before */
#define S_WAIT     1        /* a name was found; waiting for the certificate to start */
#define S_CERT     2        /* inside a wanted certificate */
#define S_SKIP     3        /* inside a certificate that is not wanted */

#endif /* PEMSELECT_CONST_H */
