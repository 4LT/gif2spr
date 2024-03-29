gif2spr
=======

A utility for converting animated GIF images to Quake and Half-Life sprites.

by Seth "4LT" Rader

How to Use
==========

CLI
---

`gif2spr`

Output usage to stderr.

`gif2spr GIFFILE SPRFILE`

Converts a GIF file to a Quake sprite using default settings.

`gif2spr -hl -dummy GIFFILE SPRFILE`

Converts a GIF file to a Half-Life sprite using default settings, appending a "dummy" frame to the end to (required hack for Half-Life, omit `-dummy` for Sven Co-op sprites).

`gif2spr -align ALIGNMENT GIFFILE SPRFILE`

Creates a sprite with a specified alignment type, one of:
* vp-parallel - Always facing the player. (default)
* upright - Facing the player's XY position, but always upright.
* oriented - Always facing the same direction.

`gif2spr -origin X,Y GIFFILE SPRFILE`

Creates a sprite that is centered on the (X, Y) position of the image.  0,0 centers the sprite on the lower left of the image, and 1,1 centers the sprite on the upper right.  The default value is 0.5,0.5.

`gif2spr -palette PALFILE GIFFILE SPRFILE`

Creates a sprite, but color quantization is performed matching the colors from a given file instead of the default Quake palette.  The palette format is the same as the palette lump used in Quake: 256 RGB triplets, 8 bits per component.

GUI
---

Run `gif2spr-gui.tcl` on Linux, or `gif2spr-gui.exe` on Windows.

Building
========

Run `make` to create a standalone executable for the CLI on Linux.  To install, just copy gif2spr to `/usr/local/bin` or `/usr/bin`.

Run `make TARGET_PLAT=win32 win-gui` to build CLI/GUI for Windows.  Make sure the generated executables are in the same directory for the GUI to work.

Source Code
===========

Available at https://github.com/4LT/gif2spr
