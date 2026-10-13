# Changelog

All notable changes to PicoCalc NetThing.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `netthing_const.h`.

## [Unreleased]

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
