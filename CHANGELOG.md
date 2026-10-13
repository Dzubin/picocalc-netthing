# Changelog

All notable changes to PicoCalc NetThing.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `netthing_const.h`.

## [Unreleased]

### Changed
- Three modes after the title screen: Wi-Fi scan, Bluetooth scan and LAN tools. `ESC` in a mode goes to the title screen (the connection stays
  up) and a key there goes on in that mode. The LAN tools menu has `W=WIFI` and `B=BLUETOOTH`; the speed test is now `E` and the clock `K`.
- `P=PAUSE` is on a line of its own on both scan screens. A paused scan starts again when you come back from another screen (the Wi-Fi
  scan stays paused while a connection is up).
- The separate full-screen Classic list (`C`) is gone. The Classic area of the Bluetooth screen scrolls through every Classic device heard
  in the last minute (`CLASSIC  1-4 OF 17`). The old Find and Read list screens are gone too: `F` and `I` act on the device under the cursor.
- The Wi-Fi scan heading reads `WIFI SCAN`, the Bluetooth one `BT SCAN`. The help lines of both scan screens start with `P=PAUSE` and
  write each key as `KEY=WHAT`.
- The Pico W (RP2040) build leaves out the Windows test server tools: 6.5 KB of program space and 5.8 KB of RAM saved.
- The link line is the top row of every screen; headings that were there moved down one row.
- `B` goes straight to the nearby devices list (no Bluetooth menu). The other Bluetooth tools are keys on it: `F` find, `I` read, `C`
  Classic list, `T` tracker alert, `X` crowding report, `P` pause, `R` empty the list, `W` or `ESC` back.

### Added
- IP over USB (first version, not yet tried on real hardware): `U` on the title screen makes the PicoCalc a USB network adapter (RNDIS for Windows, ECM for the others) that
  gets its address from the PC's shared connection (Internet Connection Sharing). The top line shows `USB <address>` and the LAN tools run over it.
  The link layer (`link.c`) gives the tools the addresses of whichever link is up, and a second connection asks to give up the first.
- `L` opens the LAN tools menu from the Wi-Fi scan list and the Bluetooth screen when a link is up, and says so when none is. A new
  connection first asks to give up the one that is up (one way to the LAN at a time).
- On the nearby devices list `Enter` is the connect key and `D` the details; the cursor goes on from the Low Energy list into the Classic
  rows. Connecting by Bluetooth is not built yet.
- A line under the heading of every screen (row 1) says how the PicoCalc reaches the LAN: `WI-FI name  -52 DBM`, or `NOT CONNECTED`.
  The continuous ping host name and the first hop of the trace route chart moved down one row to leave it free. The scan screen opens paused while a link is up, so a scan
  does not disturb the connection; `P` starts it.
- On the Wi-Fi scan list `Enter` connects and `D` shows the details (it was the other way round). Connecting while a link is up asks
  first. The splash has a `U` key for IP over USB, which is not built yet.
- The Wi-Fi scan screen scans over and over by itself: each scan updates the list (an access point not heard for three scans drops off),
  the marker stays on the chosen access point, `P` pauses, `R` empties the list and starts again.
- The nearby devices list has a Classic area at the bottom: after four 5 second Low Energy windows the scan stops for one Classic inquiry.
- Saved networks now keep the BSSID of the access points joined (up to four per network). In the scan list a name is green when a saved
  network has both its name and its BSSID, yellow when only one of them is saved. Joining a saved network adds the BSSID. The saved file
  is now version 2 (`NTW2`); the old kind is still read.

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
