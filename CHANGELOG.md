# Changelog

All notable changes to PicoCalc NetThing.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `netthing_const.h`.

## [Unreleased]

### Fixed
- The Wi-Fi tools (signal meter, channel survey with its hill view, health check) can be opened with `T` on the Wi-Fi scan, without a
  connection. Since V1.10A they could only be reached from the status screen, which needs a Wi-Fi connection.

## [V1.10A]

### Added
- **Three ways to reach the LAN**, one at a time (connecting a second way first asks to give up the first):
  - Wi-Fi, as before;
  - **IP over USB**: `U` on the title screen makes the PicoCalc a USB network adapter (RNDIS for Windows, ECM for the others) that gets its
    address from the PC's shared connection (Internet Connection Sharing on Windows). Tried on Windows 11; Linux, the BSDs and macOS are
    untried (the README has pointers);
  - **Bluetooth tethering** (Pico 2 W only): `Enter` on a Classic phone in the Bluetooth scan pairs (a code to confirm), opens a PAN (BNEP)
    link and gets an address by DHCP. The radio stays on while the link is up.
- **A link line on the top row of every screen**: `NOT CONNECTED`, `WI-FI name -52 DBM`, `USB 192.168.137.2` or `BT phone 192.168.44.x`.
  Ping, DNS, health check and the other tools use whichever link is up.
- **Three modes** after the title screen: Wi-Fi scan, Bluetooth scan and LAN tools. `L` opens the LAN tools from either scan screen (or says
  `NOT CONNECTED`), `B` and `W` move between the modes, `ESC` goes to the title screen without dropping the connection and a key there carries on
  in the mode that was left.
- **The Wi-Fi scan scans over and over by itself**, merging each scan into the list (an access point not heard for three scans drops off), with the
  marker staying on the chosen access point. `P=PAUSE` pauses; the scan stays paused while a connection is up, because a scan can disturb it.
- **The Bluetooth scan is one screen**: the Low Energy list, and below it a Classic area (a Classic inquiry after every four 5-second Low Energy
  windows) that scrolls through every Classic device heard (`CLASSIC  1-4 OF 17`). `F` finds and `I` reads the chosen device, `T` is the tracker
  alert, `X` the crowding report. The old Bluetooth menu and separate Classic list are gone.
- **Saved networks keep the BSSID** of each access point joined (up to four per network; file version `NTW2`, the old kind is still read). In
  the Wi-Fi scan a name is green when a saved network has both its name and BSSID, yellow when only one of them is saved. Saved networks, and
  Classic devices paired with the PicoCalc (green), are kept at the top of their lists.
- **Leaving a network politely**: a DHCP release and a proper disassociation (Wi-Fi), a closed adapter (USB) or a closed BNEP link (Bluetooth),
  with `DISCONNECTING...` shown meanwhile, also on `~` (BOOTSEL) and when leaving for the loader.

### Changed
- On the Wi-Fi scan `Enter` connects and `D` shows the details (it was the other way round); help lines read `KEY=WHAT`.
- LAN tools menu: the speed test is now `E` and the clock `K`; `W` and `B` go to the Wi-Fi and Bluetooth scans.
- Headings that were on the top row moved down one row. The continuous ping host and the first hop of the trace route chart moved too.
- Pico W (RP2040) build: no Windows test server tools and no Bluetooth tethering, and the certificate tool copies files in 512 byte pieces, to
  fit in its RAM (about 243 KB of 264 KB is used). This build is still not tried on real hardware.
- Windows test server version V1.10A (no changes in the server itself).

## [V1.01A]

### Changed
- The status screen has fewer keys: `T` Wi-Fi tools, `L` LAN tools, `B` Bluetooth, `D` disconnect. The health check and the router ping
  are no longer separate keys there; they are in the Wi-Fi tools menu.
- The tools are in three menus instead of one: Wi-Fi tools (health check, signal meter, channel survey), LAN tools (health check, ping,
  ping the router, continuous ping, trace route, DNS checker, name lookup, speed test, web page check, clock, certificates, LAN scan, port
  check, Windows test server tools) and Bluetooth tools.
- Disconnecting (`D`) and leaving the program while Wi-Fi is connected now ask `Y/N` first, so a few ESC presses too many do not drop
  the connection.
- Windows test server version V1.01A. A few harmless build warnings in `url.c` are gone.
