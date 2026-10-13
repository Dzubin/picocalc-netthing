/*
 * portlist_const.h - constants of portlist.c: the most ports one check takes,
 * and the names of the well-known ports.
 *
 * Author: Thomas Dzubin
 */
#ifndef PORTLIST_CONST_H
#define PORTLIST_CONST_H

#define PORTLIST_MAX        64      /* ports in one list */
#define PORTLIST_TEXT_MAX   39      /* longest list as typed */

/* port number, name (up to 8 characters); the list ends with port 0 */
#define PORTLIST_NAMES { \
    {7, "ECHO"}, {20, "FTP-DATA"}, {21, "FTP"}, {22, "SSH"}, {23, "TELNET"}, {25, "SMTP"}, \
    {53, "DNS"}, {67, "DHCP"}, {80, "HTTP"}, {110, "POP3"}, {123, "NTP"}, {139, "NETBIOS"}, \
    {143, "IMAP"}, {161, "SNMP"}, {389, "LDAP"}, {443, "HTTPS"}, {445, "SMB"}, {465, "SMTPS"}, \
    {587, "SUBMIT"}, {631, "IPP"}, {993, "IMAPS"}, {995, "POP3S"}, {1433, "MSSQL"}, \
    {1883, "MQTT"}, {3306, "MYSQL"}, {3389, "RDP"}, {5900, "VNC"}, {8080, "HTTP-ALT"}, \
    {8443, "HTTPS-ALT"}, {0, ""} }

#endif /* PORTLIST_CONST_H */
