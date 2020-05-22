M-Audio MIDISPORT USB 64-bit MIDI device driver for MacOS 10.14 (Mojave)
========================================================================

This project provides an open source MacOS 10.14 compatible CoreMIDI device driver
for M-Audio's MIDISPORT range of USB MIDI interfaces. This driver supports the following
devices:

    + MIDISPORT 1x1
    + MIDISPORT 2x2
    + MIDISPORT 4x4
    + MIDISPORT 8x8/S

I originally wrote M-Audio's first MacOS X version back in 2000 and donated it to them, as
part of a start-up project that needed a MIDI device driver running on what was then a
pre-release version of MacOS X. M-Audio took the code and updated it many times without
giving me back their updated source code. M-Audio have seemed to have now abandoned the
hardware and no longer support their version of the driver since 2009, as their newer
devices do not use that driver. With the move by Apple to v10.14 (Mojave) and v10.15
(Catalina) to no longer support 32 bit drivers, I have modified and updated my original
code donated to M-Audio to now compile as 64 bit versions on these latest MacOS versions, so
MIDISPORT owners can continue to support and operate their hardware.

The project consists of three parts:

    1. The MacOS X CoreMIDI device driver itself, consisting of a modified version of Apple's
       publicly available (and now very old) MIDI device driver example code.

    2. A firmware downloader which downloads to the EZUSB 8051 compatible microcontroller
       within the MIDISPORT devices the firmware to transmit and receive to and from MIDI and USB
       ports on the devices.

    3. Some very simple, and now fairly obsolete, MacOS X CoreMIDI client code to test
       transmitting and receiving MIDI code.

MacOS X CoreMIDI Device Driver
------------------------------

The MacOS X CoreMIDI device driver is a modified version of Apple's publicly available
(and now very old) CoreMIDI device driver example code. This was adapted for the
MIDISPORT, taking all technical details of the driver from those publicly available from
the (open source Linux version of the MIDISPORT driver)[https://www.alsa-project.org/wiki/Usb-midi-fw],
together with (publicly available example code for downloading firmware)[https://github.com/esden/fxload],
provided by EZUSB, the provider of the microcontroller inside the MIDISPORT devices.

Currently the code has only been tested on MacOS 10.14, but the goal is to also support MacOS
10.15, which seems to have changed the USB Host API.

MIDISPORT Firmware
------------------

In order to avoid distributing any M-Audio supplied code, or infringe on NDAs, none of the
MIDISPORT firmware is distributed as part of this project. Part of the updates to the
firmware downloader code have been to now read Intel Hex format versions of firmware files
that were publicly distributed by M-Audio as part of their
(Linux driver effort)[http://usb-midi-fw.sourceforge.net/]. Users will therefore need to download those
files to their Mac and save them into the appropriate folder location for the firmware
downloader to find them. However the driver installation script will perform this
downloading as part of the installation of the driver.

Not all firmware for every MIDISPORT device is distributed with the Linux driver. Support
is currently missing for some of the later models which used different firmware
(distributed as part of the download for M-Audio's last 32 bit driver).


Installation instructions 
-------------------------
1. Decompress the tarball (MIDISPORT-2.1.2.b.MOX.pkg.tar) to create the package MIDISPORT.pkg and this file.

Double-Click on the .tar file to create MIDISPORT.pkg and this file using StuffIt Expander
or OpenUp.app (obtainable via www.stepwise.com) or open Terminal.app and run:

	     tar xvf MIDISPORT-2.1.2.b.MOX.pkg.tar

to extract MIDISPORT.pkg.

2. Double-Click the .pkg package, and follow the standard installation operation to
install the plugin. When prompted for authorization to install the plugin, click on the
lock icon in the lower left corner of the dialog and enter an adminstrator's password. The
installer should place the plugin into /Library/Audio/MIDI Drivers/MIDISPORT.plugin.

3. Connect the MIDISPORT device to the USB chain. The status light will not glow
immediately after connecting, only after the MIDISPORT firmware has been downloaded to the
interface.

4. The first time an application attempts to open the MIDI driver, the firmware for the
MIDISPORT will be downloaded which will be indicated by its LED labelled "USB"
pulsing. Subsequent opening of the device will then use the already downloaded firmware.
