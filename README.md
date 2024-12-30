# LynxCartEmu

This repo is based on Brian's repo, see original comments below.

This fork is solely to support Atari Lynx Carts.
Upon start, the firmware loads either a `game.lnx` file or if not available provides a uBLL for Lynx.

A new ROM image can be uploaded (as .lnx file) and will be stored in internal RAM and on SD card (if available) for the next boot.

Sending is done via `SendCard.py`.

The firmware allows to "reload" the SD cart via serial command `cr` if game.lnx was saved on a PC on the card.


---

# EPROMEmu

**!! WARNING: I have no idea what I'm doing !!**

I've decided to make this public in the rare event it might help someone.

This is a very simple, likely very wrong (EP)ROM emulator using a Teensy 4.1 and Arduino/PlatformIO.  It works for my purposes (emulating an EPROM for
use in an Atari Lynx EPROM cartridge), but it it is probably not the "correct" way to design hardware.  I have also used it on Game Gear and Vectrex EPROM cartridges,
but it was not heavily tested.

Please take caution with using anything you see here.  It's my first time using KiCad, my first time designing a PCB, and my first time creating hardware.  Again,
it works for my purposes, but perhaps it'll burn your house down.

Enjoy!
