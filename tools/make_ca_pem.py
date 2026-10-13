#!/usr/bin/env python3
"""make_ca_pem.py - build the CA.PEM file for PicoCalc NetThing's https from the
Mozilla root certificate list that curl publishes (https://curl.se/ca/cacert.pem).

    python make_ca_pem.py cacert.pem CA.PEM                 the 32 roots listed below
    python make_ca_pem.py cacert.pem CA.PEM names.txt       the roots named in names.txt
    python make_ca_pem.py cacert.pem --list                 every root name in the list

The program reads at most 48 KB of CA.PEM and keeps every certificate in memory while
it talks to a server, so a short list is better than all of the roughly 120. Copy the
result to the SD card as /netthing/CA.PEM.

Author: Thomas Dzubin
"""
import re
import sys

DEFAULT_ROOTS = [
    "ISRG Root X1", "ISRG Root X2",
    "DigiCert Global Root G2", "DigiCert Global Root G3", "DigiCert TLS ECC P384 Root G5",
    "DigiCert TLS RSA4096 Root G5", "DigiCert Trusted Root G4",
    "GlobalSign Root CA - R3", "GlobalSign Root R46", "GlobalSign Root E46", "GlobalSign ECC Root CA - R5",
    "Go Daddy Root Certificate Authority - G2", "Starfield Root Certificate Authority - G2",
    "USERTrust RSA Certification Authority", "USERTrust ECC Certification Authority",
    "COMODO RSA Certification Authority",
    "Amazon Root CA 1", "Amazon Root CA 2", "Amazon Root CA 3", "Amazon Root CA 4",
    "GTS Root R1", "GTS Root R3", "GTS Root R4",
    "Microsoft RSA Root Certificate Authority 2017", "Microsoft ECC Root Certificate Authority 2017",
    "Sectigo Public Server Authentication Root R46", "Sectigo Public Server Authentication Root E46",
    "IdenTrust Commercial Root CA 1",
    "SSL.com TLS ECC Root CA 2022", "SSL.com TLS RSA Root CA 2022",
    "SSL.com Root Certification Authority ECC", "SSL.com Root Certification Authority RSA",
]

LIMIT = 48 * 1024           # what the program reads of CA.PEM


def read_roots(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    blocks = re.findall(r"^([^\n]+)\n={5,}\n(-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----)",
                        text, re.S | re.M)
    return {name: pem for name, pem in blocks}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    roots = read_roots(argv[1])
    if argv[2] == "--list":
        for name in roots:
            print(name.encode("ascii", "replace").decode())
        return 0
    wanted = DEFAULT_ROOTS
    if len(argv) > 3:
        with open(argv[3], encoding="utf-8") as f:
            wanted = [line.strip() for line in f if line.strip() and not line.startswith("#")]
    missing = [name for name in wanted if name not in roots]
    if missing:
        print("not in the list:", ", ".join(missing))
        return 1
    chosen = [name for name in roots if name in wanted]     # in the order of the list, as the program does
    out = "\n".join(roots[name] for name in chosen) + "\n"
    if len(out) > LIMIT:
        print("too big: %d bytes, the program reads %d" % (len(out), LIMIT))
        return 1
    with open(argv[2], "w", encoding="utf-8", newline="\n") as f:
        f.write(out)
    print("%d roots, %d bytes written to %s" % (len(chosen), len(out), argv[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
