/*
 * modes.h - the three modes the program is in after the title screen, and the
 * result a mode gives back when it ends: the mode to go to next (or the title
 * screen, for ESC).
 *
 * Author: Thomas Dzubin
 */
#ifndef MODES_H
#define MODES_H

#define MODE_NONE    (-1)       /* no change asked for */
#define MODE_WIFI    0          /* the Wi-Fi scan screen */
#define MODE_BT      1          /* the Bluetooth scan screen */
#define MODE_LAN     2          /* the LAN tools menu */
#define MODE_TITLE   3          /* back to the title screen */

#endif /* MODES_H */
