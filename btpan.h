/*
 * btpan.h - Bluetooth tethering: the PicoCalc joins a phone's shared connection as
 * a PAN user (the BNEP protocol over Bluetooth Classic), gets an address from the
 * phone's DHCP server and runs its LAN tools over that link. The phone must have
 * Bluetooth tethering switched on. The implementation is btpan.c (BTstack and
 * lwIP); the screens see only these calls.
 *
 * Author: Thomas Dzubin
 */
#ifndef BTPAN_H
#define BTPAN_H

#include <stdbool.h>
#include <stdint.h>

#include "wifi.h"
#include "btpan_const.h"

/* One-time setup of the BNEP part of the stack (called when the stack starts). */
void btpan_init(void);

/* Start connecting to the phone with this address: its network service is looked up,
 * the pairing is done if it is not paired yet, the BNEP link opened and an address
 * asked for. Returns 0, or non-zero if one is going on already. */
int  btpan_start(const uint8_t address[6], const char *name);

/* Give the link up (or cancel the attempt). */
void btpan_stop(void);

/* BTPAN_IDLE .. BTPAN_FAILED. Also looks for the address the phone gives. */
int  btpan_state(void);

/* What is going on, or why it failed, as a line of text for the screen. */
const char *btpan_message(void);

/* True while the phone and the PicoCalc are comparing a pairing code; the code is
 * in *number (0 if the pairing has no code). Answer with btpan_pairing_answer(). */
bool btpan_pairing_request(uint32_t *number);
void btpan_pairing_answer(bool yes);

/* The addresses of the link, in the record of a Wi-Fi connection (the name is the
 * phone's, the signal 0). Returns false unless the state is BTPAN_UP. */
bool btpan_info(wifi_info_t *info);

#endif /* BTPAN_H */
