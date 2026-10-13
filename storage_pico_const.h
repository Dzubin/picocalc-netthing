/*
 * storage_pico_const.h - constants of storage_pico.c.
 *
 * Author: Thomas Dzubin
 */
#ifndef STORAGE_PICO_CONST_H
#define STORAGE_PICO_CONST_H

#define STORAGE_TRIES        3      /* write and read-back attempts */
#define STORAGE_VERIFY_MAX   6144   /* largest file that can be checked after a write */
#define STORAGE_DIR_MAX      64     /* longest folder part of a path */

#endif /* STORAGE_PICO_CONST_H */
