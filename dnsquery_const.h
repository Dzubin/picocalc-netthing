/*
 * dnsquery_const.h - constants of dnsquery.c: the layout of a DNS packet.
 *
 * Author: Thomas Dzubin
 */
#ifndef DNSQUERY_CONST_H
#define DNSQUERY_CONST_H

#define DNSQ_HEADER_LEN     12
#define DNSQ_FLAG_RESPONSE  0x8000u
#define DNSQ_FLAG_RD        0x0100u
#define DNSQ_CLASS_IN       1

#endif /* DNSQUERY_CONST_H */
